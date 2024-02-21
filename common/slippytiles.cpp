
#include <math.h>

#include "logging.hpp"
#include "slippytiles.hpp"

double resolution(double latitude, uint32_t zoom) {
    return 156543.03 * cos(to_radians(latitude)) / pow(2.0, (double)zoom);
}

double tilex2long(int32_t x, uint32_t zoom) {
    return x / (double)(1 << zoom) * 360.0 - 180;
}

double tiley2lat(int32_t y, uint32_t zoom) {
    double n = M_PI - 2.0 * M_PI * y / (double)(1 << zoom);
    return to_degrees(atan(0.5 * (exp(n) - exp(-n))));
}

void lat_lon_to_pixel(double lat, double  lon, uint32_t zoom, int32_t tile_size, double &x, double &y) {
    int32_t num_tiles = (1 << zoom);
    x = (lon + 180.0) / 360.0 * num_tiles * tile_size;
    y = ((1 - log(tan(to_radians(lat)) + 1 / cos(to_radians(lat))) / M_PI) / 2 * num_tiles * tile_size);
    LOG_DEBUG("x=%.2f y=%.2f",x,y);
}

void lat_lon_to_tile(double lat, double  lon, uint32_t zoom, int32_t tile_size, uint32_t& tile_x, uint32_t& tile_y) {
    double pixel_x, pixel_y;
    lat_lon_to_pixel(lat, lon, zoom, tile_size, pixel_x, pixel_y);
    tile_x = pixel_x / 256;
    tile_y = pixel_y / 256;
    LOG_DEBUG("tile_x=%lu  tile_y=%lu pixel_x=%.2f pixel-y=%.2f", tile_x, tile_y, pixel_x, pixel_y);
}

void compute_pixel_offset(double lat, double  lon, uint32_t zoom, int32_t tile_size,
                          uint32_t& tile_x, uint32_t& tile_y, uint32_t &offset_x, uint32_t &offset_y) {
    double pixel_x, pixel_y;
    lat_lon_to_tile(lat, lon, zoom, tile_size,  tile_x, tile_y);
    lat_lon_to_pixel(lat, lon, zoom, tile_size, pixel_x, pixel_y);
    offset_x = round(pixel_x - tile_x * 256);
    offset_y = round(pixel_y - tile_y * 256);
    LOG_DEBUG("offset_x=%lu offset_y=%lu", offset_x, offset_y);
}
