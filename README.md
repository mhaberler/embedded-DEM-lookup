# embedded-DEM-lookup

given latitude and longitude, and a Digital Elevation Model in GeoTIFF format, this code retrieves the elevation on a resource-constrained platform - i.e. assuming GDAL is not available

This is the embedded equivalent to
````
$ gdallocationinfo -wgs84  DTM_Austria_10m_v2_by_Sonny.tif 15.20977 47.12925
Report:
  Location: (43264P,21366L)
  Band 1:
    Value: 865.799987792969   <--- elevation in meters
````

## use case
- given coordinats from a GPS, this code should give much better vertical accuracy than the GPS elevation
- for flying, this is a basis for determining height-over-ground using a GPS

## Design
- choose a DEM of the desired resolution or higher. For this example, I took the [Austria 10m DEM published by Sonny](https://sonny.4lima.de/). 
- choose a [zoom level matching the desired resolution](https://wiki.openstreetmap.org/wiki/Zoom_levels) (in my case: zoom 13 results in about 14m/pixel covering an area of 3.6km squared per tile)
- convert the DEM into [Terrain-RGB](https://github.com/syncpoint/terrain-rgb/blob/master/README.md) format stored in an [MBTiles](https://docs.mapbox.com/help/glossary/mbtiles/) file, breaking up a large GeoTIFF into small tiles at the chosen zoom level
- since the MBTiles format is an Sqlite3 database, any platform with an Sqlite3 library and a webp or PNG decoder can read this DEM
- since the reading process is slow, decoded tiles are LRU-cached. A typical 256x256 tile requires about 200kB.

DEMs are stored as single files on a SD card. See below for file samples.

### Prerequisites for building a DEM

`pip install gdal rio-rgbify mb-util`

## Converting the GeoTIFF DEM into a Terrain-RGB MBTiles archive

Here is a [tutorial outlining the steps](https://github.com/syncpoint/terrain-rgb/blob/master/README.md) - recommended

`````
gdalwarp \
    -t_srs EPSG:3857  \
    -dstnodata None  \
    -novshiftgrid \
    -co TILED=YES  \
    -co COMPRESS=DEFLATE  \
    -co BIGTIFF=IF_NEEDED \
    -r lanczos \
    DTM_Austria_10m_v2_by_Sonny.tif \
    DTM_Austria_10m_v2_by_Sonny_LANCZOS.tif

rio rgbify   \
    -b -10000   \
    -i 0.1  \
    DTM_Austria_10m_v2_by_Sonny_LANCZOS.tif  \
    DTM_Austria_10m_v2_by_Sonny_LANCZOS_RGB.tif

gdal2tiles.py --zoom=13-13 --processes=8  DTM_Austria_10m_v2_by_Sonny_LANCZOS_RGB.tif ./tiles

mb-util --silent --image_format=png --scheme=xyz ./tiles/ DTM_Austria_10m_v2_by_Sonny_LANCZOS_RGB_13.mbtiles
`````
Copy the `.mbtiles` file to a fast SD card and set the name in `platformio.ini` accordingly.



## Platform

This code was tested on a M5Stack CoreS3 but should run on any ESP32 platform with an SD card reader and sufficient PSRAM. 

Default cache size is 5 tiles, using 1M PSRAM.

A Python PoC implementation is here: python/getaltitude.py

## Other platforms

This should run with minor mods on any platform with sufficient RAM to hold a tile, provided:
- a port of the file system adaptation layer in [esp32_arduino_sqlite3_lib](https://github.com/siara-cc/esp32_arduino_sqlite3_lib) to whatever the target provides
- the memory allocation calls are fixed (currently ESP-IDF specifc)

## Performance

Reading a tile from SD is slow: 800ms for the first tile using webp, 1.4s using PNG.
Looking up elevation on a cached tile is fast - less than 0.5ms.

SD card used was a Transcend Ultimate 633x, 32GB, FAT32 filesystem.

## Verifying results
I used a few samples of [proven elevation data](https://github.com/syncpoint/terrain-rgb/blob/master/README.md#verifying-the-elevation-data) and these come out well:

`````
...
1180 Utopiaweg 1: reference=333.0 altitude=332.8 delta=20cm
1190 Höhenstraße: reference=403.6 altitude=402.8 delta=80cm
1010 Stephansplatz: reference=171.4 altitude=171.3 delta=10cm
1070 Lindengasse 3: reference=198.2 altitude=198.0 delta=20cm
1220 Industriestraße 81: reference=158.6 altitude=158.6 delta=0cm
`````

The DEMs used in this test are available at:

- https://static.mah.priv.at/cors/AT-10m-webp.mbtiles webp-lossless encoding, 256x256, 374M
- https://static.mah.priv.at/cors/AT-10m-png.mbtiles PNG encoding, 256x256, 607M

## Usage example

see `src/main.cpp`.

## Status
works fine, but very C-ish code.

## What about compression?

TLDR;  I tried - don't.

Compression will likely produce slightly different RGB values for a given point. To retrieve altitude, the RGB values are fed into the formula:

`altitude = -10000 + ((red * 256 * 256 + green * 256 + blue) * 0.1)`

Example:

47.1292 15.2097 has RGB(1,168,127) with lossless encoding, giving altitude 867.1m.

Now assume compression changes this to RGB(2,168,127) - which results in altitude of 7420.7m.

This is why using compressed PNG or webp tiles returns an error code.

## possible improvements
- the code supports several DEM's concurrently, reporting the first match - not yet tested

## parts list

- reading the MBTiles archive in SQLite3 format: [esp32_arduino_sqlite3_lib](https://github.com/siara-cc/esp32_arduino_sqlite3_lib)
- decoding PNG tiles: [pngle](https://github.com/kikuchan/pngle)
- decoding webp tiles: [libwebp](https://github.com/webmproject/libwebp)
- LRU cache for decoded PNG tiles: a modified version of [cpp-lru-cache](https://github.com/lamerman/cpp-lru-cache)

