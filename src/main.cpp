
#include <M5Unified.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <SPI.h>
#include <FS.h>
#include "SD.h"
#include "math.h"
#include "Esp.h"
#include "mbtiles.hpp"
#include "mercmath.hpp"
extern bool loopTaskWDTEnabled;

bool psRAMavail;

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

    delay(3000);
    psRAMavail = ESP.getFreePsram() > 0;
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);

    log_e("C++ version: %ld", __cplusplus);
    log_e("free heap: %lu", ESP.getFreeHeap());
    log_e("used psram: %lu", ESP.getPsramSize() - ESP.getFreePsram());

    SPI.begin();
    if (SDCardExist()) {
        log_e("SD card detected, mounting..");
        SDInit();
    }
    int rc;
    double lat,lon,ref;
    demInfo_t *di = NULL;
    locInfo_t li = {};

    sqlite3_initialize();

    rc = addMBTiles("/sd/test13.mbtiles", &di);
    if (rc != SQLITE_OK) {
        log_e("addMBTiles fail: %d\n", rc);
    } else {
        log_e("maxZoom %d resolution: %.1fm/pixel", di->maxZoom, resolution(di->bbox.ll_lat, di->maxZoom));
    }

    lat = 47.12925176802318;
    lon = 15.209778656353123;
    ref = 865.799987792969;
    uint32_t now =  micros();;
    rc = getLocInfo(lat, lon, &li);
    log_e("8113 Stiwoll Kehrer:  %d %d %.1f %.1f - %d uS cold", rc, li.status, li.elevation, ref,  micros()-now);
    li = {};

    now =  micros();;
    rc = getLocInfo(lat, lon, &li);
    log_e("8113 Stiwoll Kehrer:  %d %d %.1f %.1f - %d uS cached", rc, li.status, li.elevation, ref,  micros()-now);

    li = {};
    lat = 48.2383409011934;
    lon = 16.299522929921253;
    ref = 333.0;
    rc = getLocInfo(lat, lon, &li);
    log_e("1180 Utopiaweg 1:  %d %d %.1f %.1f", rc, li.status, li.elevation, ref);

    li = {};
    lat = 48.2610837936095;
    lon = 16.289583084029545;
    ref = 403.6;
    rc = getLocInfo(lat, lon, &li);
    log_e("1190 Höhenstraße:  %d %d %.1f %.1f", rc, li.status, li.elevation, ref);

    li = {};
    lat = 48.208694143314325;
    lon =16.37255104738311;
    ref = 171.4;
    rc = getLocInfo(lat, lon, &li);
    log_e("1010 Stephansplatz:   %d %d %.1f %.1f", rc, li.status, li.elevation, ref);

    li = {};
    lat = 48.225003606677504;
    lon = 16.44120643847108;
    ref = 158.6;
    rc = getLocInfo(lat, lon, &li);
    log_e("1220 Industriestraße 81:  %d %d %.1f %.1f\n", rc, li.status, li.elevation, ref);

    printCache();
    printDems();
    log_e("free heap: %lu", ESP.getFreeHeap());
    log_e("used psram: %lu", ESP.getPsramSize() - ESP.getFreePsram());

}

void loop(void) {
    delay(1);
}
