#pragma once
#include <string_view>

#include "buffer.hpp"


int32_t decompress_gzip(const buffer_t &in, buffer_t &out);