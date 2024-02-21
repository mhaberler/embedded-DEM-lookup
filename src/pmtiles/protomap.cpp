
#include <Arduino.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include <string_view>

#include "pngle.h"

#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/types.h"

#include "util.hpp"
#include "logging.hpp"
#include "protomap.hpp"
#include "slippytiles.hpp"

using namespace std;
using namespace pmtiles;

static void evictTile(uint64_t key, tile_t *t);

static cache::lru_cache<uint64_t, tile_t *> tile_cache(TILECACHE_SIZE, {}, evictTile);
static vector<demInfo_t *> dems;
static uint8_t dbindex;
static uint8_t pngSignature[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

void decodeInit(void) {

}

static const string_view get_bytes( demInfo_t *di, size_t start, size_t length) {

    if (length > di->buffer_size) {
        size_t n = next_power_of_2(length);
        di->buffer = heap_caps_realloc(di->buffer, n, MALLOC_CAP_SPIRAM);
        LOG_DEBUG("realloc  %zu -> %zu  %p", di->buffer_size, n, di->buffer );

        di->buffer_size = (di->buffer == NULL) ? 0 : n;
    }
    if (length > di->buffer_size) {
        return string();
    }
    off_t ofst = fseek(di->fp, start, SEEK_SET); //lseek(p->fd, iOfst, SEEK_SET);
    if( ofst != 0 ) {
        perror("seek");
        return string();
    }
    size_t got = fread(di->buffer, 1, length, di->fp);
    if (got != length) {
        LOG_ERROR("read failed: got %zu of %zu, %s", got, length, strerror(errno));
        return string();
    }
    return string_view((const char*)di->buffer, length);
}

int addDEM(const char *path, demInfo_t **demInfo) {
    struct	stat st;

    if (stat(path, &st)) {
        perror(path);
        return -1;
    }
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        perror(path);
        return -1;
    }
    LOG_DEBUG("open '%s' size %zu : %s", path, st.st_size, strerror(errno));

    demInfo_t *di = new demInfo_t();
    di->fp = fp;
    di->buffer = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    di->buffer_size = BUFFER_SIZE;

    string hdr = string(get_bytes(di, 0, 127));
    LOG_INFO("hdr size %zu magic='%s'", hdr.size(), hdr.substr(0, 7).c_str());

    headerv3 header = deserialize_header(hdr);

    di->tile_size = TILESIZE;
    di->path = strdup(path);
    dems.push_back(di);
    if (demInfo != NULL) {
        *demInfo = di;
    }
    return 0;
}

bool demContains(demInfo_t *di, double lat, double lon) {
    int32_t lat_e7 = to_e7(lat);
    int32_t lon_e7 = to_e7(lon);
    return  ((lat_e7 > di->header.min_lat_e7) && (lat_e7 < di->header.max_lat_e7) &&
             (lon_e7 > di->header.min_lon_e7) && (lon_e7 < di->header.max_lon_e7));
}

string keyStr(uint64_t key) {
    xyz_t k;
    k.key = key;
    return string_format("dem=%d %d/%d/%d",k.entry.index, k.entry.z, k.entry.x, k.entry.y);
}

void printCache(void) {
    for (auto item: tile_cache.items()) {
        LOG_INFO("%s", keyStr(item.first).c_str());
    }
}

void printDems(void) {
    for (auto d: dems) {
        LOG_INFO("dem %u: %s bbx=%.2f/%.2f..%.2f/%.2f dberr=%u tile_err=%u hits=%u misses=%u tilesize=%u",
                 d->index, d->path,
                 min_lat(d), min_lon(d),
                 max_lat(d), max_lon(d),
                 d->db_errors, d->tile_errors, d->cache_hits, d->cache_misses, d->tile_size);
    }
}

static void freeTile(tile_t *tile) {
    if (tile == NULL)
        return;
    if (tile->buffer != NULL)
        heap_caps_free(tile->buffer);
    heap_caps_free(tile);
}

static void evictTile(uint64_t key, tile_t *t) {
    LOG_DEBUG("evict %s",keyStr(key).c_str());
    if (t != NULL) {
        freeTile(t);
    }
}

static encoding_t encodingType(const uint8_t *blob, int blob_size) {
    if (memcmp(blob, pngSignature, sizeof(pngSignature)) == 0) {
        return ENC_PNG;
    }
    if (!memcmp(blob, "RIFF", 4) && !memcmp(blob + 8, "WEBP", 4)) {
        return ENC_WEBP;
    }
    return ENC_UNKNOWN;
}

static void pngle_init_cb(pngle_t *pngle, uint32_t w, uint32_t h) {
    size_t size =  w * h * 3;
    tile_t *tile = (tile_t *)heap_caps_malloc(sizeof(tile_t), MALLOC_CAP_SPIRAM);
    tile->buffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    tile->width = w;
    pngle_set_user_data(pngle, tile);
}

static int lines = 6;
static void pngle_draw_cb(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]) {
    tile_t *tile = (tile_t *) pngle_get_user_data(pngle);
    size_t offset = (x + tile->width * y) * 3;
    tile->buffer[offset] = rgba[0];
    tile->buffer[offset+1] = rgba[1];
    tile->buffer[offset+2] = rgba[2];
}

bool lookupTile(demInfo_t *di, locInfo_t *locinfo, double lat, double lon) {
    xyz_t key;
    tile_t *tile = NULL;
    uint32_t offset_x, offset_y;
    uint32_t tile_x, tile_y;
    compute_pixel_offset(lat, lon, di->max_zoom, di->tile_size,
                         tile_x, tile_y, offset_x, offset_y);

    key.entry.index = di->index;
    key.entry.x =  (uint16_t)tile_x;
    key.entry.y =  (uint16_t)tile_y;
    key.entry.z = di->max_zoom;

    if (!tile_cache.exists(key.key)) {
        LOG_DEBUG("cache entry %s not found", keyStr(key.key).c_str());
        di->cache_misses++;

        TIMESTAMP(query);
        STARTTIME(query);

        uint64_t tile_id = zxy_to_tileid(di->max_zoom, tile_x, tile_y);

        bool found;
        if (found) {
            const uint8_t *blob ;
            int blob_size;

            PRINT_LAPTIME("blob retrieve %u us", query);

            switch(encodingType(blob, blob_size)) {
                case ENC_PNG: {
                        TIMESTAMP(pngdecode);
                        STARTTIME(pngdecode);
                        pngle_t *pngle = pngle_new();
                        pngle_set_init_callback(pngle, pngle_init_cb);
                        pngle_set_draw_callback(pngle, pngle_draw_cb);
                        int fed = pngle_feed(pngle, blob, blob_size);
                        if (fed != blob_size) {
                            LOG_ERROR("%s: decode failed: decoded %d out of %u: %s",
                                      keyStr(key.key).c_str(), fed, blob_size, pngle_error(pngle));
                            freeTile(tile);
                            tile = NULL;
                            di->tile_errors++;
                            locinfo->status = LS_PNG_DECODE_ERROR;
                        } else {
                            pngle_ihdr_t *hdr = pngle_get_ihdr(pngle);
                            if (hdr->compression) {
                                locinfo->status = LS_PNG_COMPRESSED;
                                LOG_ERROR("%s: compressed PNG tile",
                                          keyStr(key.key).c_str());
                            } else {
                                di->tile_size = pngle_get_width(pngle);
                                tile = (tile_t *) pngle_get_user_data(pngle);
                                tile_cache.put(key.key, tile);
                                locinfo->status = LS_VALID;
                            }
                        }
                        pngle_destroy(pngle);
                        PRINT_LAPTIME("PNG decode %u us", pngdecode);
                    }
                    break;
                case ENC_WEBP: {
                        TIMESTAMP(webpdecode);
                        STARTTIME(webpdecode);

                        int width, height, rc;
                        VP8StatusCode sc;
                        WebPDecoderConfig config;
                        size_t bufsize;
                        uint8_t *buffer;

                        WebPInitDecoderConfig(&config);
                        rc = WebPGetInfo(blob, blob_size, &width, &height);
                        if (!rc) {
                            LOG_ERROR("%s: WebPGetInfo failed rc=%d", keyStr(key.key).c_str(), rc);
                            locinfo->status = LS_WEBP_DECODE_ERROR;
                            break;
                        }
                        sc = WebPGetFeatures(blob, blob_size, &config.input);
                        if (sc != VP8_STATUS_OK) {
                            LOG_ERROR("%s: WebPGetFeatures failed sc=%d", keyStr(key.key).c_str(), sc);
                            locinfo->status = LS_WEBP_DECODE_ERROR;
                            break;
                        }
                        if (config.input.format != 2) {
                            LOG_ERROR("%s: lossy WEBP compression", keyStr(key.key).c_str());
                            locinfo->status = LS_WEBP_COMPRESSED;
                            break;
                        }
                        tile = (tile_t *) heap_caps_malloc(sizeof(tile_t), MALLOC_CAP_SPIRAM);
                        bufsize = width * height * 3;
                        buffer = (uint8_t *) heap_caps_malloc(bufsize, MALLOC_CAP_SPIRAM);
                        if ((tile != NULL) && (buffer != NULL) && (bufsize != 0)) {
                            tile->buffer = buffer;
                            tile->width = config.input.width;
                            if (WebPDecodeRGBInto(blob, blob_size,
                                                  buffer, bufsize, width * 3) == NULL) {
                                LOG_ERROR("%s: WebPDecode failed", keyStr(key.key).c_str());
                                freeTile(tile);
                                tile = NULL;
                                di->tile_errors++;
                                locinfo->status = LS_WEBP_DECODE_ERROR;
                            } else {
                                di->tile_size = config.input.width;
                                tile_cache.put(key.key, tile);
                                locinfo->status = LS_VALID;
                            }
                            WebPFreeDecBuffer(&config.output);
                            PRINT_LAPTIME("WEBP decode %u us", webpdecode);
                        }
                        break;
                    default:
                        locinfo->status = LS_UNKNOWN_IMAGE_FORMAT;
                        break;
                    }
            }
        }
    } else {
        LOG_DEBUG("cache entry %s found: ", keyStr(key.key).c_str());
        tile = tile_cache.get(key.key);
        locinfo->status = LS_VALID;
        di->cache_hits++;
    }
    // assert(tile->buffer != NULL);

    if (locinfo->status == LS_VALID) {
        size_t i = round(offset_x)  + round(offset_y) * di->tile_size;
        locinfo->elevation = rgb2alt(&tile->buffer[i * 3] );
        return true;
    }
    return false;
}

int getLocInfo(double lat, double lon, locInfo_t *locinfo) {
    for (auto di: dems) {
        if (demContains(di, lat, lon)) {
            LOG_DEBUG("%.2f %.2f contained in %s", lat, lon, di->path);
            if (lookupTile(di, locinfo, lat, lon)) {
                return 0;
            }
        }
    }
    locinfo->status = LS_TILE_NOT_FOUND;
    return 0;
}
