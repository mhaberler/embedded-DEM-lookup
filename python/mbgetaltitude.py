from PIL import Image
import sqlite3
from io import BytesIO
import math
import sys

tile_size = 256


def read_max_zoom_from_mbtiles(mbtiles_path):
    conn = sqlite3.connect(mbtiles_path)
    cursor = conn.cursor()
    cursor.execute("SELECT max(zoom_level) FROM tiles")
    max_zoom = cursor.fetchone()[0]
    conn.close()
    return max_zoom


def read_tile_from_mbtiles(mbtiles_path, zoom, x, y):
    conn = sqlite3.connect(mbtiles_path)
    cursor = conn.cursor()
    cursor.execute(
        "SELECT tile_data FROM tiles WHERE zoom_level = ? AND tile_column = ? AND tile_row = ?",
        (zoom, x, y),
    )
    tile_data = cursor.fetchone()
    conn.close()
    return tile_data[0] if tile_data else None


def lat_lon_to_pixel(lat, lon, zoom, tile_size):
    # Convert latitude and longitude to pixel coordinates at a given zoom level

    num_tiles = 2**zoom
    x = (lon + 180) / 360 * num_tiles * tile_size
    y = (
        (
            1
            - math.log(math.tan(math.radians(lat)) + 1 / math.cos(math.radians(lat)))
            / math.pi
        )
        / 2
        * num_tiles
        * tile_size
    )
    return x, y


def lat_lon_to_tile(lat, lon, zoom, tile_size):
    # Convert latitude and longitude to tile coordinates at a given zoom level
    pixel_x, pixel_y = lat_lon_to_pixel(lat, lon, zoom, tile_size)
    tile_x = pixel_x // 256
    tile_y = pixel_y // 256
    return tile_x, tile_y


def compute_pixel_offset(lat, lon, zoom, tile_size):
    # Compute X, Y tile coordinates and pixel offset for a given coordinate
    tile_x, tile_y = lat_lon_to_tile(lat, lon, zoom, tile_size)
    pixel_x, pixel_y = lat_lon_to_pixel(lat, lon, zoom, tile_size)
    offset_x = pixel_x - tile_x * 256
    offset_y = pixel_y - tile_y * 256
    return tile_x, tile_y, offset_x, offset_y


def rgb2h(red, green, blue, debug=False):
    altitude = -10000 + ((red * 256 * 256 + green * 256 + blue) * 0.1)
    if debug:
        print(f"{red=} {green=} {blue=} altitude={altitude:.1f}m")
    return altitude


def altitudeAt(image, x, y, debug=False):
    pixel = image.getpixel((x, y))
    red, green, blue = pixel[0], pixel[1], pixel[2]
    return rgb2h(red, green, blue, debug), red, green, blue


def decode_png(tile_data):
    return Image.open(BytesIO(tile_data))


# Example usage: getaltitude.py <dem.mbtiles>
if len(sys.argv) == 1:
    mbtiles_path = "data/mbtiles/test13.mbtiles"
else:
    mbtiles_path = sys.argv[1]
debug = len(sys.argv) > 2
# debug = True
zoom_level = read_max_zoom_from_mbtiles(mbtiles_path)


def getAltitude(mbtiles_path, latitude, longitude, zoom_level, debug=False):
    tile_x, tile_y, offset_x, offset_y = compute_pixel_offset(
        latitude, longitude, zoom_level, tile_size
    )
    if debug:
        print(f"{tile_x=} {tile_y=} {offset_x=} {offset_y=} {zoom_level=}")
    tile_data = read_tile_from_mbtiles(mbtiles_path, zoom_level, tile_x, tile_y)

    if tile_data:
        # Decode PNG, retrieve RGB values and compute altitude
        image = decode_png(tile_data)
        # altitudeAt(image, offset_x, offset_y, debug)
        return altitudeAt(image, round(offset_x), round(offset_y), debug)
        # altitudeAt(image, int(offset_x), int(offset_y), debug)
    else:
        print("Tile not found in the MBTiles archive.")


# testset from https://github.com/syncpoint/terrain-rgb/blob/master/README.md#verifying-the-elevation-data
testset = [
    (
        "8113 Stiwoll Kehrer",
        15.209778656353123,
        47.12925176802318,
        0,
        0,
        0,
        # Austria V2 10m DTM from https://sonny.4lima.de
        # gdallocationinfo -geoloc  -wgs84 DTM_Austria_10m_v2_by_Sonny.tif 15.209778656353123 47.12925176802318
        # Report:
        #   Location: (43264P,21366L)
        #   Band 1:
        #     Value: 865.799987792969
        865.799987792969,
    ),
    (
        "1180 Utopiaweg 1",
        16.299522929921253,
        48.2383409011934,
        333.406,
        333.0957,
        333.0957,
        333.0,
    ),
    (
        "1190 Höhenstraße",
        16.289583084029545,
        48.2610837936095,
        403.356,
        403.1385,
        403.662,
        403.6,
    ),
    (
        "1010 Stephansplatz",
        16.37255104738311,
        48.208694143314325,
        171.766,
        171.418,
        171.418,
        171.4,
    ),
    (
        "1070 Lindengasse 3",
        16.354641842524874,
        48.201276304040626,
        198.515,
        198.0624,
        198.2035,
        198.2,
    ),
    (
        "1220 Industriestraße 81",
        16.44120643847108,
        48.225003606677504,
        158.911,
        158.625,
        158.625,
        158.6,
    ),
    (
        # within bounding box so has a NODATA value - outside DEM coverage
        # should return elevation 0
        "Traunstein DE, outside coverage",
        12.658149018177179,
        47.86762217761052,
        0,
        0,
        0,
        0,
    ),
]

for test in testset:
    loc, longitude, latitude, proven, lambert, epsg3857, epsg3857rgb = test
    altitude,red,green,blue = getAltitude(mbtiles_path, latitude, longitude, zoom_level, debug)
    delta = round((epsg3857rgb - altitude) * 100)
    print(f"{loc}: {red=} {green=} {blue=} reference={epsg3857rgb} altitude={altitude:.1f} delta={delta}cm")
