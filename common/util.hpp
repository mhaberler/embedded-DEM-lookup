#pragma once

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include "logging.hpp"
using namespace std;


string string_format(const std::string fmt, ...);

void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
