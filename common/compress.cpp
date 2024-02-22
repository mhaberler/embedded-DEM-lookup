
#include "zlib.h"
#include <string>
#include <string_view>

#include "compress.hpp"
#include "buffer.hpp"
#include "logging.hpp"

#include "zlib.h"

using std::string;
using std::stringstream;

#define MOD_GZIP_ZLIB_WINDOWSIZE 15

int32_t decompress_gzip(const buffer_t &in, buffer_t &out) {
    z_stream zs = {};
    int rc = inflateInit2(&zs, MOD_GZIP_ZLIB_WINDOWSIZE + 16);
    if (rc != Z_OK) {
        LOG_ERROR("inflateInit failed: %d", rc);
        return rc;
    }

    zs.next_in = (Bytef*)in.data();
    zs.avail_in = in.size();

    char *outbuffer =reinterpret_cast<char *>(malloc(32768));

    // get the decompressed bytes blockwise using repeated calls to inflate
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(32768);

        rc = inflate(&zs, 0);

        if (out.size() < zs.total_out) {
            std::string_view b(outbuffer, zs.total_out - out.size());
            out.insert(std::end(out), std::begin(b), std::end(b));
            // out.push_back(std::vector(outbuffer, zs.total_out - out.size()));
        }

    } while (rc == Z_OK);

    inflateEnd(&zs);

    if (rc != Z_STREAM_END) {          // an error occurred that was not EOF
        LOG_ERROR("zlib decompressio failed: %d", rc);
    }
    return rc;
}
