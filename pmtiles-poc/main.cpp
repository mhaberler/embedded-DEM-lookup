#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <string_view>
#ifdef ESP32
    #include <esp_heap_caps.h>
#endif

#include "logging.hpp"
#include "zlib.h"
#include "pngle.h"

#include "pmtiles.hpp"

#define MIN_ALLOC_SIZE 8192
#define MOD_GZIP_ZLIB_WINDOWSIZE 15

static inline size_t next_power_of_2(size_t s) {
    s--;
    s |= s >> 1;
    s |= s >> 2;
    s |= s >> 4;
    s |= s >> 8;
    s |= s >> 16;
    s++;
    return s;
};

static inline size_t alloc_size(size_t s) {
    if (s < MIN_ALLOC_SIZE)
        return MIN_ALLOC_SIZE;
    return next_power_of_2(s);
}

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

void decodeTile(const void *buffer, size_t size) {
    pngle_t *pngle = pngle_new();

    pngle_set_init_callback(pngle, on_init);
    pngle_set_draw_callback(pngle, on_draw);

    int fed = pngle_feed(pngle, buffer, size);
    if (fed < 0) {
        fprintf(stderr, "%s\n", pngle_error(pngle));
        exit(1);
    }
    printf("size %u fed %zu\n", size, fed);
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
typedef enum {
    PM_OK = 0,
    PM_IOBUF_ALLOC_FAILED = -1,
    PM_SEEK_FAILED = -2,
    PM_READ_FAILED = -3,
    PM_DEFLATE_INIT_FAILED = -4,
    PM_ZLIB_DECOMP_FAILED = -5,

} pmErrno_t;

void *io_buffer;
size_t io_size;
void *decomp_buffer;
size_t decomp_size;
int pmerrno;

void *decompress_gzip(const void *in, size_t size, size_t &got) {
    z_stream zs = {};
    int rc = inflateInit2(&zs, MOD_GZIP_ZLIB_WINDOWSIZE + 16);
    if (rc != Z_OK) {
        LOG_ERROR("inflateInit failed: %d", rc);
        pmerrno = PM_DEFLATE_INIT_FAILED;
        return nullptr;
    }

    zs.next_in = (Bytef*)in;
    zs.avail_in = size;
    size_t remain = decomp_size;

    do {
        zs.next_out = (Bytef*)(((char *)decomp_buffer) + zs.total_out);
        zs.avail_out = remain;
        rc = inflate(&zs, 0);

        if (decomp_size < zs.total_out) {
            size_t na = alloc_size(zs.total_out);
            LOG_DEBUG("realloc %zu -> %zu", decomp_size, na);
            decomp_buffer = realloc(decomp_buffer, na);
            // FIXME check nullptr
            decomp_size = na;
            remain = decomp_size - zs.total_out;
        }
    } while (rc == Z_OK);

    inflateEnd(&zs);

    if (rc != Z_STREAM_END) {          // an error occurred that was not EOF
        LOG_ERROR("zlib decompression failed: %d", rc);
        pmerrno = PM_ZLIB_DECOMP_FAILED;
        return nullptr;
    }
    got = zs.total_out;
    return decomp_buffer;
}

void *get_bytes(FILE *fp,
                off_t start,
                size_t length,
                uint8_t compression = COMPRESSION_NONE) {
    if (io_size < length) {
        io_size = alloc_size(length);
        io_buffer = realloc(io_buffer, io_size);
        if (io_buffer == nullptr) {
            LOG_ERROR("realloc %zu failed:  %s", length, strerror(errno));
            pmerrno = PM_IOBUF_ALLOC_FAILED;
            io_size = 0;
            return nullptr;
        }
    }
    off_t ofst = fseek(fp, start, SEEK_SET);
    if (ofst != 0 ) {
        LOG_ERROR("fseek to %zu failed:  %s", start, strerror(errno));
        pmerrno = PM_SEEK_FAILED;
        return nullptr;;
    }
    size_t got = fread(io_buffer, 1, length, fp);
    if (got != length) {
        LOG_ERROR("read failed: got %zu of %zu, %s", got, length, strerror(errno));
        pmerrno = PM_READ_FAILED;
        return nullptr;;
    }
    return io_buffer;
}

int main(int argc, char *argv[]) {
    int32_t rc;

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

    FILE *fp = fopen(file_name, "rb");
    if (fp == nullptr) {
        perror(file_name);
        exit(1);
    }
    LOG_DEBUG("open '%s' size %zu", file_name, st.st_size);

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
    decomp_buffer = realloc(decomp_buffer, 1024);
    decomp_size = 1024;

    uint64_t tile_id = zxy_to_tileid(13, 4442, 2877);
    printf("tid %lld\n", tile_id);
    void *p;

    if ((p = get_bytes(fp, 0, 127)) == nullptr) {
        perror("get_bytes");
        exit(1);
    }
    headerv3 header = deserialize_header(string_view(reinterpret_cast<const char *>(p), 127));

    uint64_t dir_offset  = header.root_dir_offset;
    uint64_t dir_length = header.root_dir_bytes;

    for (int i = 0; i < 4; i++) {
        string_view dir;
        if ((p = get_bytes(fp, dir_offset, dir_length)) == nullptr) {
            perror("get_bytes");
            break;
        }

        std::vector<entryv3> directory;

        if (header.internal_compression == COMPRESSION_GZIP) {
            printf("---- COMPRESSION_GZIP\n");
            size_t got;
            void *s = decompress_gzip(p, dir_length, got);
            directory = deserialize_directory(string((char *)s, got));
        } else {
            directory = deserialize_directory(string((char *)p, dir_length));
        }

        entryv3 result = find_tile(directory,  tile_id);

        if (!isNull(result)) {
            if (result.run_length == 0) {
                dir_offset = header.leaf_dirs_offset + result.offset;
                dir_length = result.length;
            } else {
                if ((p = get_bytes(fp, header.tile_data_offset + result.offset, result.length)) == nullptr) {
                    perror("get_bytes");
                    break;
                }
                decodeTile(p, result.length);
                break;
            }
        }
    }
    return 0;
}