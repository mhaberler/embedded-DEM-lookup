#pragma once

#include <vector>
#include <string>
#include "pmtiles.hpp"
#include "lrucache.hpp"
#include "buffer.hpp"
#include "slippytiles.hpp"

using namespace std;
using namespace pmtiles;

#ifdef ESP32_TIMING
    #define xstr(s) str(s)
    #define str(s) #s
    #define TIMESTAMP(x) int64_t x;
    #define STARTTIME(x) do { x = esp_timer_get_time();} while (0);
    #define PRINT_LAPTIME(fmt, x)  do { Serial.printf(fmt xstr(\n), (uint32_t) (esp_timer_get_time() - x)); } while (0);
    #define LAPTIME(x)  ((uint32_t) (esp_timer_get_time() - x))
#else
    #define TIMESTAMP(x)
    #define STARTTIME(x)
    #define PRINT_LAPTIME(fmt, x)
    #define LAPTIME(x) 0
#endif

#ifndef TILESIZE
    #define TILESIZE 256
#endif

#ifndef TILECACHE_SIZE
    #define TILECACHE_SIZE 5
#endif


typedef struct {
    uint8_t *buffer;
    size_t width; // of a line in pixels
} tile_t;

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
    LS_WEBP_DECODE_ERROR,
    LS_PNG_DECODE_ERROR,
    LS_PNG_COMPRESSED,
    LS_WEBP_COMPRESSED,
    LS_UNKNOWN_IMAGE_FORMAT,
    LS_DB_ERROR
} locStatus_t;

typedef enum {
    ENC_NONE,
    ENC_UNKNOWN,
    ENC_PNG,
    ENC_WEBP,
    ENC_BAD_FORMAT,
} encoding_t;

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
    FILE *fp;
    headerv3 header;
    uint32_t db_errors;
    uint32_t tile_errors;
    uint32_t cache_hits;
    uint32_t cache_misses;
    uint16_t tile_size;
    encoding_t encoding;
    uint8_t index;
    // uint8_t max_zoom;
} demInfo_t;


static inline double min_lat(const demInfo_t *d) {
    return from_e7(d->header.min_lat_e7);
}
static inline double max_lat(const demInfo_t *d) {
    return from_e7(d->header.max_lat_e7);
}
static inline double min_lon(const demInfo_t *d) {
    return from_e7(d->header.min_lon_e7);
}
static inline double max_lon(const demInfo_t *d) {
    return from_e7(d->header.max_lon_e7);
}

void decodeInit(void);
int addDEM(const char *path, demInfo_t **demInfo = NULL);
int getLocInfo(double lat, double lon, locInfo_t *locinfo);

void printCache(void);
void printDems(void);

string string_format(const string fmt, ...);
