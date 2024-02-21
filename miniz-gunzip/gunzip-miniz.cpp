#include <stdio.h>
#include <string>
#include <string.h>
// #include <zlib.h>

using namespace std;

extern "C" {
#include "miniz.h"
};

int xuncompressNew(const string &comp, string &ret) {
    mz_stream stream;
    memset(&stream, 0, sizeof(stream));

    // In case mz_ulong is 64-bits (argh I hate longs).
    if (comp.size() > 0xFFFFFFFFU) return MZ_PARAM_ERROR;

    stream.next_in = (const unsigned char *)(comp.c_str() + 10);
    stream.avail_in = (mz_uint32)comp.size()-18;

    int status = mz_inflateInit(&stream);
    if (status != MZ_OK)
        return status;

    while(status == MZ_OK) {
        unsigned char buffer[100];
        stream.next_out = buffer;
        stream.avail_out = sizeof(buffer);
        status = mz_inflate(&stream, MZ_NO_FLUSH);
        if(status == MZ_STREAM_END || status == MZ_OK) {
            ret.append((char*)buffer, stream.total_out - ret.size());
        }
    }

    mz_inflateEnd(&stream);
    return status;
}

int main() {
    std::string cd ="\x78\x9C\x2B\xC9\x48\x55\x28\x2C\xCD\x4C\xCE\x56\x48\x2A\xCA\x2F\xCF\x53\x48\xCB\xAF\x00\x00\x47\x8E\x07\x34";
    std::string result;
    int rc =  xuncompressNew(cd, result);
    printf("rc=%d %zu -> %zu\n", rc, cd.size(), result.size());
    // == MZ_DATA_ERROR
    return 0;
}
