
#include <M5Unified.h>

#include <stdlib.h>
#include <SPI.h>
#include <FS.h>
#include "SD.h"
#include "math.h"
#include "Esp.h"
#include "logging.hpp"
#include "protomap.hpp"
#include "slippytiles.hpp"
#include "util.hpp"

#ifdef CORES3
#define AW9523_ADDR 0x58
#define SD_CS 4

static bool SDCardExist() {
    return (bool)!((M5.In_I2C.readRegister8(AW9523_ADDR, 0x00, 100000L) >> 4) & 0x01);
}

bool SDInit() {
    return SD.begin(SD_CS, SPI, 4000000UL);
}
#endif

void setup(void) {
    bool mount_ok = false;
    delay(3000);
#ifdef M5UNIFIED
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
#else
    Serial.begin(115200);
#endif
    set_loglevel(LOG_LEVEL_DEBUG);

    LOG_INFO("C++ version: %ld", __cplusplus);
    LOG_INFO("free heap: %lu", ESP.getFreeHeap());
    LOG_INFO("used psram: %lu", ESP.getPsramSize() - ESP.getFreePsram());

    SPI.begin();
    if (SDCardExist()) {
        LOG_INFO("SD card detected, mounting..");
        mount_ok = SDInit();
        if (!mount_ok) {
            LOG_ERROR("mount failed");
        } else {
            LOG_INFO("SD Card Type: %u", SD.cardType());
            uint32_t cardSize = SD.cardSize() / (1000 * 1000 * 1000);
            LOG_INFO("SD Card Size: %u GB\n", cardSize);
            listDir(SD, "/", 2);
        }
    }
    int rc;
    double lat,lon,ref;
    demInfo_t *di = NULL;
    locInfo_t li = {};
    TIMESTAMP(now);

    decodeInit();

    rc = addDEM(TEST_DEM, &di);
    if (rc != 0) {
        LOG_ERROR("addDEM fail: %d\n", rc);
    } else {
        // LOG_INFO("max_zoom %d resolution: %.2fm/pixel", di->max_zoom, resolution(di->bbox.ll_lat, di->max_zoom));
    }

    lat = 47.12925176802318;
    lon = 15.209778656353123;
    ref = 865.799987792969;

    STARTTIME(now);
    rc = getLocInfo(lat, lon, &li);
    LOG_INFO("8113 Stiwoll Kehrer:  %d %d %.2f %.2f - %d uS cold", rc, li.status, li.elevation, ref,  LAPTIME(now));
    li = {};

    STARTTIME(now);
    rc = getLocInfo(lat, lon, &li);
    LOG_INFO("8113 Stiwoll Kehrer:  %d %d %.2f %.2f - %d uS cached", rc, li.status, li.elevation, ref,  LAPTIME(now));

    li = {};
    lat = 48.2383409011934;
    lon = 16.299522929921253;
    ref = 333.0;
    rc = getLocInfo(lat, lon, &li);
    LOG_INFO("1180 Utopiaweg 1:  %d %d %.2f %.2f", rc, li.status, li.elevation, ref);

    li = {};
    lat = 48.2610837936095;
    lon = 16.289583084029545;
    ref = 403.6;
    rc = getLocInfo(lat, lon, &li);
    LOG_INFO("1190 Höhenstraße:  %d %d %.2f %.2f", rc, li.status, li.elevation, ref);

    li = {};
    lat = 48.208694143314325;
    lon =16.37255104738311;
    ref = 171.4;
    rc = getLocInfo(lat, lon, &li);
    LOG_INFO("1010 Stephansplatz:   %d %d %.2f %.2f", rc, li.status, li.elevation, ref);

    li = {};
    lat = 48.225003606677504;
    lon = 16.44120643847108;
    ref = 158.6;
    rc = getLocInfo(lat, lon, &li);
    LOG_INFO("1220 Industriestraße 81:  %d %d %.2f %.2f\n", rc, li.status, li.elevation, ref);

    printCache();
    printDems();

    LOG_INFO("free heap: %u", ESP.getFreeHeap());
    LOG_INFO("used psram: %u", ESP.getPsramSize() - ESP.getFreePsram());
}

void loop(void) {
    delay(1);
}
