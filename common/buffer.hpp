#pragma once

#include "psram_allocator.hpp"

static inline size_t next_power_of_2(size_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
};


// typedef esp32::psram::string buffer_t;
typedef  std::vector<char, esp32::psram::allocator<char>> buffer_t;
