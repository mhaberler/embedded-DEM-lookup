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
- convert the DEM into [Terrain-RGB](https://github.com/syncpoint/terrain-rgb/blob/master/README.md) format stored in an [MBTiles](https://docs.mapbox.com/help/glossary/mbtiles/) file, breaking up a large GeoTIFF into small PNG tiles at the chosen zoom level
- since the MBTiles format is an Sqlite3 database, any platform with an Sqlite3 library and a PNG decoder can read this DEM
- since the reading process is slow, tiles are LRU-cached. A typical 256x256 tile requires a 262kB buffer.

The DEM is a single file stored on a SD card. The example DEM is about 606MB.

### Prerequisites

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
Copy the `.mbtiles` file to a fast SD card and set the name in `src/main.cpp` accordingly.

## Platform

This was done with a M5Stack CoreS3 but should run on any ESP32 platform with an SD card reader and sufficient PSRAM.

Current cache is 5 tiles, using 1.3M PSRAM.

A Python PoC implementation is here: python/getaltitude.py

## Other platforms

This should run with minor mods on any platform with sufficient RAM to hold a tile, provided:
- a port of the file system adaptation layer in [esp32_arduino_sqlite3_lib](https://github.com/siara-cc/esp32_arduino_sqlite3_lib) to whatever the target provides
- the memory allocation calls are fixed (currently ESP-IDF specifc)

## Performance

Reading a tile is slow: 1.3s for the first tile.
Looking up elevation on a cached tile is fast - about 300uS.

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

The DEM used in the test is available at https://static.mah.priv.at/cors/test13.mbtiles .
## Status
works, but unpolished. Very C-ish code.

## possible improvements

- use webp for tile compression. The file sizes for different encoding methods of the sample above are:
   - png: 606MB
   - webp-lossless:  374MB
   - webp at quality 75:  68M
- shrink memory requirements by storing 3 bytes/per pixel instead of 4 - the alpha channel is not needed right now (but might be useful for missing data/NODATA anyway)
- the code supports several DEM's concurrently, reporting the first match - not yet tested

## parts list

- reading the MBTiles archive in SQLite3 format: [esp32_arduino_sqlite3_lib](https://github.com/siara-cc/esp32_arduino_sqlite3_lib)
- decoding PNG tiles: [PNGdec](https://github.com/bitbank2/PNGdec)
- LRU cache for decoded PNG tiles: a modified version of [cpp-lru-cache](https://github.com/lamerman/cpp-lru-cache)

