#pragma once
#include <math.h>
#include <stdint.h>

static inline double to_radians(double degrees) {
    return degrees * M_PI / 180.0;
}

static inline double to_degrees(double radians) {
    return radians * (180.0 / M_PI);
}
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} rgba_t;

static inline double rgb2alt(const rgba_t px) {
    return  -10000 + ((px.red * 256 * 256 + px.green * 256 + px.blue) * 0.1);
}

double resolution(double latitude, uint32_t zoom);
double tilex2long(int32_t x, uint32_t zoom);
double tiley2lat(int32_t y, uint32_t zoom);
void compute_pixel_offset(double lat, double  lon, uint32_t zoom, int32_t tile_size,
                          int32_t&tile_x, int32_t&tile_y, double &offset_x, double &offset_y);
void lat_lon_to_tile(double lat, double  lon, uint32_t zoom, int32_t tile_size, int32_t&tile_x, int32_t&tile_y);
void lat_lon_to_pixel(double lat, double  lon, uint32_t zoom, int32_t tile_size, double &x, double &y);
