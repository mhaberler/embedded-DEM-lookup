#include <Arduino.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include <PNGdec.h>

#include "logging.hpp"
#include "mbtiles.hpp"
#include "slippytiles.hpp"


static const char *tileQuery = "SELECT tile_data FROM tiles WHERE"
                               " zoom_level = ? AND tile_column = ? AND tile_row = ?";
static const char *maxZoomQuery = "SELECT max(zoom_level) FROM tiles";
static const char *bboxQuery = "SELECT min(tile_column),max(tile_column),"
                               "min(tile_row),max(tile_row) FROM tiles WHERE zoom_level = ?";

static void evictTile(uint64_t key, rgbaTile_t *t);
static cache::lru_cache<uint64_t, rgbaTile_t *> tile_cache(TILECACHE_SIZE, {}, evictTile);
static void imageSpecs(PNG &p);

static PNG png;
static std::vector<demInfo_t *> dems;
static uint8_t dbindex;



int getBBox(sqlite3 *db, demInfo_t *di) {
    sqlite3_stmt* stmt = nullptr;
    int maxZoom = -1;
    int tc_min, tc_max, tr_min, tr_max;

    int rc = sqlite3_prepare_v2(db, maxZoomQuery, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return rc;
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        maxZoom = sqlite3_column_int(stmt, 0);
        di->maxZoom = maxZoom;
        LOG_DEBUG("maxzoom: %d", maxZoom);
    } else {
        LOG_ERROR("maxzoom query failed %d", sqlite3_column_int(stmt, 0));
        sqlite3_finalize(stmt);
        return rc;
    }
    sqlite3_finalize(stmt);
    stmt = nullptr;
    rc = sqlite3_prepare_v2(db, bboxQuery, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, maxZoom);
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

    di->bbox.ll_lat = tiley2lat(tr_max, maxZoom);
    di->bbox.ll_lon = tilex2long(tc_min, maxZoom);
    di->bbox.tr_lat = tiley2lat(tr_min, maxZoom);
    di->bbox.tr_lon = tilex2long(tc_max, maxZoom);
    di->index = ++dbindex;

    LOG_DEBUG("bbox %F %F %F %F",
          di->bbox.ll_lat, di->bbox.tr_lat,
          di->bbox.ll_lon, di->bbox.tr_lon);
    return SQLITE_OK;
}

int addMBTiles(const char *path, demInfo_t **demInfo) {
    demInfo_t *di = new demInfo_t();
    int rc = sqlite3_open(path, &di->db);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Can't open database %s: rc=%d %s", path, rc, sqlite3_errmsg(di->db));
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
    di->tileSize = TILE_SIZE; // FIXME get from mbtiles
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
        LOG_INFO("dem %d: %s bbx=%F/%F..%F/%F dberr=%d tile_err=%d hits=%d misses=%d",
              d->index, d->path,d->bbox.ll_lat,d->bbox.ll_lon, d->bbox.tr_lat,d->bbox.tr_lon,
              d->db_errors, d->tile_errors, d->cache_hits, d->cache_misses);
    }
}

void freeTile(rgbaTile_t *tile) {
    if (tile == NULL)
        return;
    if (tile->buffer != NULL)
        heap_caps_free(tile->buffer);
    heap_caps_free(tile);
}

static void evictTile(uint64_t key, rgbaTile_t *t) {
    LOG_DEBUG("evict %s",keyStr(key).c_str());
    if (t != NULL) {
        if ( t->buffer != NULL)
            heap_caps_free(t->buffer);
        heap_caps_free(t);
    }
}

bool lookupTile(demInfo_t *di, locInfo_t *locinfo, double lat, double lon) {
    xyz_t key;
    rgbaTile_t *tile = NULL;
    double offset_x, offset_y;
    int32_t tile_x, tile_y;
    compute_pixel_offset(lat, lon, di->maxZoom, di->tileSize,
                         tile_x, tile_y, offset_x, offset_y);

    key.entry.index = di->index;
    key.entry.x =  (uint16_t)tile_x;
    key.entry.y =  (uint16_t)tile_y;
    key.entry.z = di->maxZoom;

    if (!tile_cache.exists(key.key)) {
        LOG_DEBUG("cache entry %s not found", keyStr(key.key).c_str());
        di->cache_misses++;

        // fetch the missing tile
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(di->db, tileQuery, -1, &stmt, nullptr);

        sqlite3_bind_int(stmt, 1, key.entry.z);
        sqlite3_bind_int(stmt, 2, key.entry.x);
        sqlite3_bind_int(stmt, 3, key.entry.y);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const uint8_t *blob = (const uint8_t *)sqlite3_column_blob(stmt, 0);
            int blob_size = sqlite3_column_bytes(stmt, 0);
            rc = png.openRAM((uint8_t *)blob, blob_size, NULL);
            if (rc == PNG_SUCCESS) {
                tile = (rgbaTile_t *) heap_caps_malloc(sizeof(rgbaTile_t), MALLOC_CAP_SPIRAM);
                int tsize = png.getBufferSize();
                uint8_t *buffer = (uint8_t *) heap_caps_malloc(tsize, MALLOC_CAP_SPIRAM);
                if ((tile != NULL) && (buffer != NULL) && (tsize != 0)) {
                    tile->buffer = buffer;
                    tile->size = tsize;
                    png.setBuffer(tile->buffer);
                    if (png.decode(NULL, 0) == PNG_SUCCESS) {
                        imageSpecs(png);
                        tile_cache.put(key.key, tile);
                        locinfo->status = LS_VALID;
                    } else {
                        freeTile(tile);
                        tile = NULL;
                        di->tile_errors++;
                        locinfo->status = LS_TILE_NOT_FOUND;
                    }
                }
            } else {

            }
            png.close();
        }
        sqlite3_finalize(stmt);
    } else {
        LOG_DEBUG("cache entry %s found: ", keyStr(key.key).c_str());
        tile = tile_cache.get(key.key);
        locinfo->status = LS_VALID;
        di->cache_hits++;
    }
    // assert(locinfo->status == LS_VALID);
    // assert(tile->buffer != NULL);
    if (locinfo->status == LS_VALID) {
        const rgba_t *img =(rgba_t*)tile->buffer;
        size_t i = round(offset_x)  + round(offset_y) * di->tileSize;
        locinfo->elevation = rgb2alt(img[i]);
        return true;
    }
    return false;
}

int getLocInfo(double lat, double lon, locInfo_t *locinfo) {
    for (auto di: dems) {
        if (demContains(di, lat, lon)) {
            LOG_DEBUG("%F %F contained in %s", lat, lon, di->path);
            if (lookupTile(di, locinfo, lat, lon)) {
                return SQLITE_OK;
            }
        }
    }
    locinfo->status = LS_TILE_NOT_FOUND;
    return SQLITE_OK;
}

static void imageSpecs(PNG &p) {
    LOG_DEBUG("image specs: (%d x %d), %d bpp, pixel type: %d alpha: %d buffersize: %d",
          p.getWidth(), p.getHeight(), p.getBpp(),
          p.getPixelType(), p.hasAlpha(), p.getBufferSize());
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
