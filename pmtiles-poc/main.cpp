#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>

#include "pmtiles.hpp"
#include "compress.hpp"
#include "pngle.h"

using namespace pmtiles;
using namespace std;

const char * file_name = PMTILES_PATH;

static void on_init(pngle_t *pngle, uint32_t w, uint32_t h) {
    void *img = malloc(w * h * 3);
    pngle_set_user_data(pngle, img);
    printf("on_init w %u h %u\n", w, h);
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

void decodeTile(char *map, size_t start, size_t length) {
    pngle_t *pngle = pngle_new();

    pngle_set_init_callback(pngle, on_init);
    pngle_set_draw_callback(pngle, on_draw);

    int fed = pngle_feed(pngle, map + start, length);
    if (fed < 0) {
        fprintf(stderr, "%s\n", pngle_error(pngle));
        exit(1);
    }
    printf("size %u fed %d\n", length, fed);
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

}

bool isNull(const entryv3 &e) {
    return ((e.tile_id == 0) &&  (e.offset == 0) &&
            (e.length == 0) && (e.run_length == 0));
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

    // test example - knowns:
    //   lat lon
    //   actual altitude
    //   z,x,y
    //   x,y offsets in resulting tile
    //
    // lat = 47.12925176802318;
    // lon = 15.209778656353123;
    // refalt = 865.799987792969 meters
    // tile_x=4442.0 tile_y=2877.0 offset_x=27.38257980090566 offset_y=5.985081114224158 zoom_level=13
    // -> 13, 4442, 2877 offset 27,6

    // Executing .pio/build/pmtiles/program
    // size 558458853 buf 0x10b641000
    // tid 79295500
    // init w 256 h 256
    // size 79198 fed 79198
    // elevation=863.3

    uint64_t tile_id = zxy_to_tileid(13, 4442, 2877);
    printf("tid %lld\n", tile_id);

    headerv3 header = deserialize_header(string(map, 127));

    uint64_t dir_offset  = header.root_dir_offset;
    uint64_t dir_length = header.root_dir_bytes;

    for (int i = 0; i < 4; i++) {
        string dir;
        if (header.internal_compression == COMPRESSION_GZIP) {
            printf("---- COMPRESSION_GZIP\n");
            dir = decompress_gzip(string(map + dir_offset, dir_length));
        } else {
            dir = string(map + dir_offset, dir_length);
        }
        std::vector<entryv3> directory = deserialize_directory(dir);
        entryv3 result = find_tile(directory,  tile_id);

        if (!isNull(result)) {
            if (result.run_length == 0) {
                dir_offset = header.leaf_dirs_offset + result.offset;
                dir_length = result.length;
            } else {
                // result
                decodeTile(map, header.tile_data_offset + result.offset, result.length);
                break;
            }
        }
    }
    return 0;
}