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
    // std::string decompress_gzip(const std::string& str) {
    z_stream zs;                        // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));

    if (inflateInit2(&zs, MOD_GZIP_ZLIB_WINDOWSIZE + 16) != Z_OK) {
        LOG_ERROR("inflateInit failed while decompressing.");
        return -1;
    }
    zs.next_in = reinterpret_cast<Bytef *>(get_buffer(in));
    zs.avail_in = buffer_size(in);

    int ret;

    Bytef *buffer = reinterpret_cast<Bytef*>(get_buffer(out));
    uInt avail = buffer_capacity(out);
    // get the decompressed bytes blockwise using repeated calls to inflate
    do {
        zs.next_out = buffer + zs.total_out;
        zs.avail_out = avail - zs.total_out;;
        ret = inflate(&zs, 0);
    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        LOG_ERROR("zlib decompression fail");
        return -2;
    }
    LOG_DEBUG("zlib decompression OK: %ul", zs.total_out);
    set_buffer_size(out, zs.total_out);
    return zs.total_out;
}
