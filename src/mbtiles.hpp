#pragma once
#include "lrucache.hpp"

#include <sqlite3.h>
#include <vector>

#define TILE_SIZE 256 // FIXME should come from mbtiles

#ifndef TILECACHE_SIZE
    #define TILECACHE_SIZE 5
#endif

typedef struct {
    uint8_t *buffer;
    size_t size;
} rgbaTile_t;

typedef struct  {
    uint16_t index;
    uint16_t x;
    uint16_t y;
    uint16_t z;
} dbxyz_t;

union xyz_t {
    uint64_t key;
    dbxyz_t entry;
};

typedef enum {
    LS_INVALID=0,
    LS_VALID,
    LS_NODATA,
    LS_TILE_NOT_FOUND,
    LS_DB_ERROR
} locStatus_t;

typedef struct {
    double elevation;
    locStatus_t status;
} locInfo_t;

typedef struct {
    double ll_lat;
    double ll_lon;
    double tr_lat;
    double tr_lon;
} bbox_t;

typedef struct {
    const char *path;
    sqlite3 *db;
    bbox_t bbox;
    uint32_t db_errors;
    uint32_t tile_errors;
    uint32_t cache_hits;
    uint32_t cache_misses;
    uint16_t tileSize;

    uint8_t index;
    uint8_t maxZoom;
} demInfo_t;

typedef int (*blob_cb)(int zoom, int x, int y, const int32_t size, const void *data);

int addMBTiles(const char *path, demInfo_t **demInfo = NULL);
int getLocInfo(double lat, double lon, locInfo_t *locinfo);

void printCache(void);
void printDems(void);

std::string string_format(const std::string fmt, ...);
