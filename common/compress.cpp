#include <string>
#include <string.h>
#include <stdexcept>

#include "compress.hpp"
#include "buffer.hpp"
#include "logging.hpp"
#include "zlib.h"

using std::string;
using std::stringstream;

// Found these here http://mail-archives.apache.org/mod_mbox/trafficserver-dev/201110.mbox/%3CCACJPjhYf=+br1W39vyazP=ix
//eQZ-4Gh9-U6TtiEdReG3S4ZZng@mail.gmail.com%3E
#define MOD_GZIP_ZLIB_WINDOWSIZE 15
#define MOD_GZIP_ZLIB_CFACTOR    9
#define MOD_GZIP_ZLIB_BSIZE      8096

int32_t decompress_gzip(buffer_t &in, buffer_t &out) {
    #if 0
    int ret;
    z_stream zs = {};
    ret = inflateInit2(&zs, MOD_GZIP_ZLIB_WINDOWSIZE + 16);
    if (ret != Z_OK) {
        LOG_ERROR("inflateInit failed while decompressing. %d", ret);
        return ret;
    }
    zs.avail_in = buffer_size(in);
    zs.next_in = reinterpret_cast<Bytef *>(get_buffer(in));
    do {
        strm.avail_out = CHUNK;
        strm.next_out = out;

        // Bytef *buffer;
        // if ((int32_t)buffer_capacity(out) - zs.total_out < 0) {
        //     buffer = reinterpret_cast<Bytef*>(get_buffer(out, zs.total_out * 2));
        // } else {
        //     buffer = reinterpret_cast<Bytef*>(get_buffer(out));
        // }
        // uInt avail = buffer_capacity(out);
        // zs.next_out = buffer + zs.total_out;
        // zs.avail_out = avail - zs.total_out;;
        // ret = inflate(&zs, 0);
    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        LOG_ERROR("zlib decompression fail");
        return -2;
    }
    LOG_DEBUG("zlib decompression OK: %u", zs.total_out);
    set_buffer_size(out, zs.total_out);
    return zs.total_out;
    #endif
}
