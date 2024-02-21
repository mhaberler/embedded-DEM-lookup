#pragma once

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include "logging.hpp"
using namespace std;

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

string string_format(const std::string fmt, ...);

void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
