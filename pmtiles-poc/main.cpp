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
buffer_t io, decomp;

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

void decodeTile(void *start, size_t length) {
    pngle_t *pngle = pngle_new();

    pngle_set_init_callback(pngle, on_init);
    pngle_set_draw_callback(pngle, on_draw);

    int fed = pngle_feed(pngle, start, length);
    if (fed < 0) {
        fprintf(stderr, "%s\n", pngle_error(pngle));
        exit(1);
    }
    printf("size %u fed %zu\n", length, fed);
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

string buffer2str(const buffer_t &b) {
    return string(reinterpret_cast<const char *>(b.buffer), b.size);
}

static int get_bytes( FILE *fp,
                      buffer_t &b,
                      size_t start,
                      size_t length,
                      uint8_t internal_compression = COMPRESSION_NONE) {

    void *buf = get_buffer(b, length);

    if (length > buffer_capacity(b)) {
        return -1;
    }

    off_t ofst = fseek(fp, start, SEEK_SET);
    if (ofst != 0 ) {
        perror("seek");
        return -2;
    }
    size_t got = fread(buf, 1, length, fp);
    if (got != length) {
        LOG_ERROR("read failed: got %zu of %zu, %s", got, length, strerror(errno));
        return -3;
    }
    set_buffer_size(b, got);
    return 0;
}

int main(int argc, char *argv[]) {
    set_loglevel(LOG_LEVEL_VERBOSE);
    setbuf(stdout, NULL);
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
    init_buffer(io, 16384, 0);
    init_buffer(decomp, 16384, 0);

    FILE *fp = fopen(file_name, "rb");
    if (fp == nullptr) {
        perror(file_name);
        exit(1);
    }
    LOG_DEBUG("open '%s' size %zu : %s", file_name, st.st_size, strerror(errno));

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

    if (get_bytes(fp, io, 0, 127)) {
        perror("get_bytes");
        exit(1);
    }

    string hdr = buffer2str(io);
    headerv3 header = deserialize_header(hdr);

    uint64_t dir_offset  = header.root_dir_offset;
    uint64_t dir_length = header.root_dir_bytes;

    for (int i = 0; i < 4; i++) {
        string dir;
        if (get_bytes(fp, io, dir_offset, dir_length)) {
            perror("get_bytes");
            exit(1);
        }
        print_buffer("io", io);

        if (header.internal_compression == COMPRESSION_GZIP) {
            printf("---- COMPRESSION_GZIP\n");
            set_buffer_size(decomp, 0);
            int32_t rc = decompress_gzip(io, decomp);
            print_buffer("decomp", decomp);
            dir = buffer2str(decomp);
        } else {
            dir =  buffer2str(io);
        }
        std::vector<entryv3> directory = deserialize_directory(dir);
        entryv3 result = find_tile(directory,  tile_id);

        if (!isNull(result)) {
            if (result.run_length == 0) {
                dir_offset = header.leaf_dirs_offset + result.offset;
                dir_length = result.length;
            } else {
                // result
                if (get_bytes(fp, io, header.tile_data_offset + result.offset, result.length)) {
                    perror("get_bytes");
                    exit(1);
                }
                decodeTile(get_buffer(io), buffer_size(io));
                break;
            }
        }
    }
    return 0;
}