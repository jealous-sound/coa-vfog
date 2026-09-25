import argparse
import csv
import io
import os
import struct
import sys
import zipfile

DESCRIPTION = "Convert the Classic client's volumetric fog tables into data/fogdata.bin."
TABLE_NAMES = ("Light.csv", "LightData.csv", "LightDataGlobalVolumeFog.csv", "ZoneLight.csv", "ZoneLightPoint.csv")
LAYER_INDEX_SLOTS = 3
CLIENT_SELECTED_FLAG = 0x8
LIGHT_PARAMS_SLOTS = 8

FILE_MAGIC = b"VFD1"
FORMAT_VERSION = 3
HEADER_FORMAT = "<4s7I"
LIGHT_FORMAT = "<Ii5f8I"
PARAMS_FORMAT = "<3I"
KEY_FORMAT = "<2H2I"
LAYER_FORMAT = "<4I11f"
ZONE_LIGHT_FORMAT = "<IiI2f2I"
ZONE_POINT_FORMAT = "<2f"
U16_MASK = 0xFFFF
U32_MASK = 0xFFFFFFFF

DIFFUSE_COLUMN = 1
EMISSIVE_COLUMN = 2
SHADOW_EMISSIVE_COLUMN = 3
START_COLUMN = 5
DENSITY_COLUMN = 6
SHADOW_MULTIPLIER_COLUMN = 7
UPPER_DENSITY_COLUMN = 8
UPPER_HEIGHT_COLUMN = 10
LOWER_DENSITY_COLUMN = 11
LOWER_HEIGHT_COLUMN = 12
INTENSITY_COLUMN = 14
G_COLUMN = 15
FLAGS_COLUMN = 22
LAYER_INDEX_COLUMN = 23
STRENGTH_COLUMN = 25
EXPONENT_COLUMN = 26


def field(row, index):
    return row["Field_1_60_1_69876_%03d" % index]


def as_float(text):
    return float(text) if text else 0.0


def as_int(text):
    return int(float(text or 0))


def as_color(text):
    return int(float(text)) & 0xFFFFFF if text else 0


def layer_flags(row):
    return as_int(field(row, FLAGS_COLUMN))


def layer_index(row):
    return as_int(field(row, LAYER_INDEX_COLUMN))


def archive_members_by_table(archive):
    members = {}
    for member in archive.namelist():
        base = member.rsplit("/", 1)[-1]
        if base in TABLE_NAMES and base not in members:
            members[base] = member
    missing = [name for name in TABLE_NAMES if name not in members]
    if missing:
        raise SystemExit("the archive has no %s" % ", ".join(missing))
    return members


def read_tables(source):
    tables = {}
    if zipfile.is_zipfile(source):
        with zipfile.ZipFile(source) as archive:
            for name, member in archive_members_by_table(archive).items():
                tables[name] = archive.read(member).decode("utf-8")
    else:
        for name in TABLE_NAMES:
            with open(os.path.join(source, name), encoding="utf-8") as handle:
                tables[name] = handle.read()
    return {name: list(csv.DictReader(io.StringIO(text))) for name, text in tables.items()}


def layers_by_index(rows):
    selected = [r for r in rows if layer_flags(r) & CLIENT_SELECTED_FLAG and 0 <= layer_index(r) < LAYER_INDEX_SLOTS]
    selected.sort(key=lambda r: int(r["ID"]))
    slots = [None] * LAYER_INDEX_SLOTS
    for row in selected:
        if slots[layer_index(row)] is None:
            slots[layer_index(row)] = row
    while slots and slots[-1] is None:
        slots.pop()
    return slots


def pack_header(light_count, params_count, key_count, layer_count, zone_light_count, zone_point_count):
    return struct.pack(HEADER_FORMAT, FILE_MAGIC, FORMAT_VERSION, light_count, params_count, key_count, layer_count,
                       zone_light_count, zone_point_count)


def pack_light(row, params_by_slot):
    return struct.pack(
        LIGHT_FORMAT,
        int(row["ID"]),
        int(float(row["ContinentID"])),
        as_float(row["GameCoords_0"]),
        as_float(row["GameCoords_1"]),
        as_float(row["GameCoords_2"]),
        as_float(row["GameFalloffStart"]),
        as_float(row["GameFalloffEnd"]),
        *params_by_slot,
    )


def pack_params(params_id, first_key, key_count):
    return struct.pack(PARAMS_FORMAT, params_id, first_key, key_count)


def pack_key(half_minute_of_day, layer_count, first_layer, direct_rgb):
    return struct.pack(KEY_FORMAT, half_minute_of_day, layer_count, first_layer, direct_rgb)


def pack_layer(row):
    if row is None:
        return struct.pack(LAYER_FORMAT, *([0] * 4 + [0.0] * 11))
    return struct.pack(
        LAYER_FORMAT,
        as_color(field(row, DIFFUSE_COLUMN)),
        as_color(field(row, EMISSIVE_COLUMN)),
        as_color(field(row, SHADOW_EMISSIVE_COLUMN)),
        layer_flags(row) & U32_MASK,
        as_float(field(row, START_COLUMN)),
        as_float(field(row, DENSITY_COLUMN)),
        as_float(field(row, SHADOW_MULTIPLIER_COLUMN)),
        as_float(field(row, UPPER_DENSITY_COLUMN)),
        as_float(field(row, UPPER_HEIGHT_COLUMN)),
        as_float(field(row, LOWER_DENSITY_COLUMN)),
        as_float(field(row, LOWER_HEIGHT_COLUMN)),
        as_float(field(row, INTENSITY_COLUMN)),
        as_float(field(row, G_COLUMN)),
        as_float(field(row, STRENGTH_COLUMN)),
        as_float(field(row, EXPONENT_COLUMN)),
    )


def pack_zone_light(row, first_point, point_count):
    return struct.pack(
        ZONE_LIGHT_FORMAT,
        int(row["ID"]),
        int(float(row["MapID"])),
        int(row["LightID"]),
        as_float(row["Zmin"]),
        as_float(row["Zmax"]),
        first_point,
        point_count,
    )


def pack_zone_point(row):
    return struct.pack(ZONE_POINT_FORMAT, as_float(row["Pos_0"]), as_float(row["Pos_1"]))


def zone_outlines(tables, light_ids):
    points_by_zone = {}
    for row in tables["ZoneLightPoint.csv"]:
        points_by_zone.setdefault(row["ZoneLightID"], []).append(row)
    zones = [row for row in tables["ZoneLight.csv"]
             if int(row["LightID"]) in light_ids and len(points_by_zone.get(row["ID"], [])) >= 3]
    zones.sort(key=lambda row: int(row["ID"]))
    return [(zone, sorted(points_by_zone[zone["ID"]], key=lambda p: int(p["PointOrder"]))) for zone in zones]


def convert(tables):
    fog_by_data = {}
    for row in tables["LightDataGlobalVolumeFog.csv"]:
        fog_by_data.setdefault(row["LightDataID"], []).append(row)

    keys_by_params = {}
    for row in tables["LightData.csv"]:
        layers = layers_by_index(fog_by_data.get(row["ID"], []))
        if layers:
            half_minute_of_day = int(float(row["Time"])) & U16_MASK
            direct_rgb = as_color(row["DirectColor"])
            keys_by_params.setdefault(int(row["LightParamID"]), []).append((half_minute_of_day, layers, direct_rgb))

    lights = []
    used_params = set()
    for row in tables["Light.csv"]:
        params_by_slot = [as_int(row["LightParamsID_%d" % slot]) for slot in range(LIGHT_PARAMS_SLOTS)]
        used_params.update(p for p in params_by_slot if p in keys_by_params)
        lights.append(pack_light(row, params_by_slot))

    params_blob, keys_blob, layers_blob = [], [], []
    key_count = layer_count = 0
    for params_id in sorted(used_params):
        keys = sorted(keys_by_params[params_id], key=lambda k: k[0])
        params_blob.append(pack_params(params_id, key_count, len(keys)))
        for half_minute_of_day, layers, direct_rgb in keys:
            keys_blob.append(pack_key(half_minute_of_day, len(layers), layer_count, direct_rgb))
            layers_blob.extend(pack_layer(r) for r in layers)
            layer_count += len(layers)
            key_count += 1

    zones_blob, points_blob = [], []
    light_ids = {int(row["ID"]) for row in tables["Light.csv"]}
    for zone, points in zone_outlines(tables, light_ids):
        zones_blob.append(pack_zone_light(zone, len(points_blob), len(points)))
        points_blob.extend(pack_zone_point(p) for p in points)

    header = pack_header(len(lights), len(params_blob), key_count, layer_count, len(zones_blob), len(points_blob))
    blob = header + b"".join(lights + params_blob + keys_blob + layers_blob + zones_blob + points_blob)
    return blob, len(lights), len(params_blob), key_count, layer_count, len(zones_blob)


def main():
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument("source", help="a folder or zip archive with the Classic DB2 tables exported as CSV")
    parser.add_argument("output", help="output path, normally data/fogdata.bin")
    args = parser.parse_args()
    blob, lights, params, keys, layers, zone_lights = convert(read_tables(args.source))
    with open(args.output, "wb") as handle:
        handle.write(blob)
    print("%s: %d lights, %d light params, %d keys, %d layers, %d zone lights, %d bytes"
          % (args.output, lights, params, keys, layers, zone_lights, len(blob)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
