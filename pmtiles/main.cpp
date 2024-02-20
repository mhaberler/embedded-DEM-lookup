
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include "pmtiles.hpp"
#include <stdexcept>

#include "compress.hpp"
#include "pngle.h"

static void on_init(pngle_t *pngle, uint32_t w, uint32_t h) {
    void *img = malloc(w * h * 3);
    pngle_set_user_data(pngle, img);
    printf("init w %u h %u\n", w, h);
}

static inline double rgb2alt( uint8_t r, uint8_t g, uint8_t b) {
    return  -10000 + ((r * 256 * 256 + g * 256 + b) * 0.1);
}

void on_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]) {
    uint8_t *img = (uint8_t *) pngle_get_user_data(pngle);
    size_t offset = (x + pngle_get_width(pngle) * y) * 3;
    img[offset] = rgba[0];
    img[offset+1] = rgba[1];
    img[offset+2] = rgba[2];
}

using namespace std;
using namespace pmtiles;

const char * file_name = "/Volumes/MAPDATA/mapdata/sonny-dems/DTM_Austria_10m_v2_by_Sonny_LANCZOS_RGB_png.pmtiles";

std::string decompress_gzip(const std::string& str);

static string mydecompress(const string &input, uint8_t compression) {
    if (compression == pmtiles::COMPRESSION_GZIP)
        return decompress_gzip(input);
    throw runtime_error("Unsupported compression");
}

int main(int argc, char *argv[]) {
    struct stat st;
    if (argc > 1)
        file_name = argv[1];
    int fd = open (file_name, O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }
    if (fstat(fd, &st)) {
        perror(file_name);
        exit(1);
    }
    char *map = static_cast<char *>(mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    printf("size %lld buf %p\n", st.st_size, map);
    auto result = entries_tms(&mydecompress, map);
    printf("entries vlen=%u sizeof entry_zxy=%u total=%u\n", result.size(), sizeof(entry_zxy), result.size()*sizeof(entry_zxy));

    // lat = 47.12925176802318;
    // lon = 15.209778656353123;
    // refalt = 865.799987792969;
    // tile_x=4442.0 tile_y=2877.0 offset_x=27.38257980090566 offset_y=5.985081114224158 zoom_level=13
    // -> 13, 4442, 2877 offset 27,6
    
    auto tile = get_tile(&mydecompress, map, 13, 4442, 2877);
    printf("tid %lld sec %u\n", tile.first, tile.second);

    pngle_t *pngle = pngle_new();

    pngle_set_init_callback(pngle, on_init);
    pngle_set_draw_callback(pngle, on_draw);

    int fed = pngle_feed(pngle, map + tile.first, tile.second);
    if (fed < 0) {
        fprintf(stderr, "%s\n", pngle_error(pngle));
        exit(1);
    }
    printf("size %u fed %d\n", tile.second, fed);
    uint8_t *img = (uint8_t *) pngle_get_user_data(pngle);
    int x = 27;
    int y = 6;
    size_t offset = (x + pngle_get_width(pngle) * y) * 3;

    uint8_t r  = img[offset];
    uint8_t g  = img[offset+1];
    uint8_t b  = img[offset+2];
    double elevation = rgb2alt(r,g,b);

    printf("elevation=%.1f\n", elevation);

    pngle_destroy(pngle);

    return 0;
}