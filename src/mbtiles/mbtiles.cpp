#include <Arduino.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>

#include "pngle.h"

#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/types.h"

#include "logging.hpp"
#include "mbtiles.hpp"
#include "slippytiles.hpp"

static const char *tileQuery = "SELECT tile_data FROM tiles WHERE"
                               " zoom_level = ? AND tile_column = ? AND tile_row = ?";
static const char *max_zoomQuery = "SELECT max(zoom_level) FROM tiles";
static const char *bboxQuery = "SELECT min(tile_column),max(tile_column),"
                               "min(tile_row),max(tile_row) FROM tiles WHERE zoom_level = ?";

static void evictTile(uint64_t key, tile_t *t);

static cache::lru_cache<uint64_t, tile_t *> tile_cache(TILECACHE_SIZE, {}, evictTile);
static std::vector<demInfo_t *> dems;
static uint8_t dbindex;
static uint8_t pngSignature[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

void decodeInit(void) {
    sqlite3_initialize();
}

int getBBox(sqlite3 *db, demInfo_t *di) {
    sqlite3_stmt* stmt = nullptr;
    int max_zoom = -1;
    int tc_min, tc_max, tr_min, tr_max;

    int rc = sqlite3_prepare_v3(db, max_zoomQuery, -1, 0, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return rc;
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        max_zoom = sqlite3_column_int(stmt, 0);
        di->max_zoom = max_zoom;
        LOG_DEBUG("max_zoom: %d", max_zoom);
    } else {
        LOG_ERROR("max_zoom query failed %d", sqlite3_column_int(stmt, 0));
        sqlite3_finalize(stmt);
        return rc;
    }
    sqlite3_finalize(stmt);
    stmt = nullptr;
    rc = sqlite3_prepare_v3(db, bboxQuery, -1, 0, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, max_zoom);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        tc_min = sqlite3_column_int(stmt, 0);
        tc_max = sqlite3_column_int(stmt, 1);
        tr_min = sqlite3_column_int(stmt, 2);
        tr_max = sqlite3_column_int(stmt, 3);
        LOG_DEBUG("col %d..%d row %d..%d", tc_min, tc_max, tr_min, tr_max);
    } else {
        LOG_ERROR("bboxQuery query failed %d", sqlite3_column_int(stmt, 0));
        sqlite3_finalize(stmt);
        return rc;
    }
    sqlite3_finalize(stmt);

    di->bbox.ll_lat = tiley2lat(tr_max, max_zoom);
    di->bbox.ll_lon = tilex2long(tc_min, max_zoom);
    di->bbox.tr_lat = tiley2lat(tr_min, max_zoom);
    di->bbox.tr_lon = tilex2long(tc_max, max_zoom);
    di->index = ++dbindex;

    LOG_DEBUG("bbox %.2f %.2f %.2f %.2f",
              di->bbox.ll_lat, di->bbox.tr_lat,
              di->bbox.ll_lon, di->bbox.tr_lon);
    return SQLITE_OK;
}

int addDEM(const char *path, demInfo_t **demInfo) {
    demInfo_t *di = new demInfo_t();
    int rc = sqlite3_open(path, &di->db);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Can't open database %s: rc=%d %s", path, rc, sqlite3_errmsg(di->db));
        delete di;
        return rc;
    }
    rc = sqlite3_prepare_v3(di->db, tileQuery, -1, SQLITE_PREPARE_PERSISTENT, &di->stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_DEBUG("sqlite3_prepare_v3 '%s' failed: rc=%d %s", tileQuery, rc, sqlite3_errmsg(di->db));
        di->db_errors++;
        delete di;
        return rc;
    }
    // retrieve bbox
    rc = getBBox(di->db, di);
    if (rc != SQLITE_OK) {
        LOG_DEBUG("bbox query failed %s: rc=%d %s", path, rc, sqlite3_errmsg(di->db));
        di->db_errors++;
        delete di;
        return rc;
    }
    di->tile_size = TILESIZE;
    di->path = strdup(path);
    dems.push_back(di);
    if (demInfo != NULL) {
        *demInfo = di;
    }
    return 0;
}

bool demContains(demInfo_t *di, double lat, double lon) {
    return  ((lat > di->bbox.ll_lat) && (lat < di->bbox.tr_lat) &&
             (lon > di->bbox.ll_lon) && (lon < di->bbox.tr_lon));
}

std::string keyStr(uint64_t key) {
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
        LOG_INFO("dem %d: %s bbx=%.2f/%.2f..%.2f/%.2f dberr=%d tile_err=%d hits=%d misses=%d tilesize=%d",
                 d->index, d->path,d->bbox.ll_lat,d->bbox.ll_lon, d->bbox.tr_lat,d->bbox.tr_lon,
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
    uint32_t offset_x, offset_y, tile_x, tile_y;
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

        // fetch the missing tile
        sqlite3_clear_bindings(di->stmt);

        sqlite3_bind_int(di->stmt, 1, key.entry.z);
        sqlite3_bind_int(di->stmt, 2, key.entry.x);
        sqlite3_bind_int(di->stmt, 3, key.entry.y);

        if (sqlite3_step(di->stmt) == SQLITE_ROW) {
            const uint8_t *blob = (const uint8_t *)sqlite3_column_blob(di->stmt, 0);
            int blob_size = sqlite3_column_bytes(di->stmt, 0);

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
    sqlite3_reset(di->stmt);

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
                return SQLITE_OK;
            }
        }
    }
    locinfo->status = LS_TILE_NOT_FOUND;
    return SQLITE_OK;
}

std::string string_format(const std::string fmt, ...) {
    int size = ((int)fmt.size()) * 2 + 50;   // Use a rubric appropriate for your code
    std::string str;
    va_list ap;
    while (1) {     // Maximum two passes on a POSIX system...
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.data(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size) {  // Everything worked
            str.resize(n);
            return str;
        }
        if (n > -1)  // Needed size returned
            size = n + 1;   // For null char
        else
            size *= 2;      // Guess at a larger size (OS specific)
    }
    return str;
}
