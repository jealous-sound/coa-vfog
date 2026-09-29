import argparse
import csv
import hashlib
import io
import json
import os
import struct
import sys
import zipfile

DESCRIPTION = ("Convert the Forever fog and lighting kit's volumetric fog, glow and colour-grading data into "
               "data/fogdata.bin, placed by the Light and ZoneLight records of a previous fogdata.bin or of the "
               "Classic client's CSV exports.")
KIT_ROOT = "coa-forever-fog-lighting-kit/"
KIT_CHECKSUMS = "SHA256SUMS"
LIGHT_DATA_TABLE = "data/decoded/LightData.json"
LIGHT_PARAMS_TABLE = "data/decoded/LightParams.json"
FOG_TABLE = "data/decoded/LightDataGlobalVolumeFog.json"
ZONE_POINT_TABLE = "data/decoded/ZoneLightPoint.json"
GRADING_ASSET = "assets/%d.blp"
FOG_TABLE_LAYOUT = "24290E20"
PLACEMENT_TABLE_NAMES = ("Light.csv", "ZoneLight.csv")

LAYER_INDEX_SLOTS = 3
CLIENT_SELECTED_FLAG = 0x8
LIGHT_PARAMS_SLOTS = 8
MINIMUM_OUTLINE_POINTS = 3

FILE_MAGIC = b"VFD1"
FORMAT_VERSION = 4
HEADER_FORMAT = "<4s8I"
LIGHT_FORMAT = "<Ii5f8I"
PARAMS_FORMAT = "<3If"
KEY_FORMAT = "<2H3I"
LAYER_FORMAT = "<4I11fI11f"
ZONE_LIGHT_FORMAT = "<IiI2f2I"
ZONE_POINT_FORMAT = "<2f"
GRADING_CURVE_FORMAT = "<I32B"
NO_GRADING_CURVE = 0
U16_MASK = 0xFFFF
U32_MASK = 0xFFFFFFFF
RGB_MASK = 0xFFFFFF

PLACEMENT_RECORD_SIZES = {
    3: {"params": 12, "key": 12, "layer": 60},
    4: {"params": 16, "key": 16, "layer": 108},
}
PLACEMENT_HEADER_FORMATS = {3: "<4s7I", 4: "<4s8I"}

BLP_MAGIC = b"BLP2"
BLP_TYPE = 1
BLP_RAW_BGRA = 3
BLP_HEADER_FORMAT = "<4sI4B2I16I16I"
BLP_FIRST_OFFSET_FIELD = 8
BLP_FIRST_SIZE_FIELD = 24
BGRA_BYTES = 4
BLUE, GREEN, RED = range(3)
LUT_SIDE = 32
LUT_STRIP_WIDTH = LUT_SIDE * LUT_SIDE
GRADING_CURVE_TOLERANCE = 1

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
NOISE_FADE_COLUMN = 4
NOISE_DIRECTION_COLUMNS = (16, 17, 18, 19, 20, 21)
NOISE_PAIR_COLUMNS = (27, 28)
UNMAPPED_TOGGLE_COLUMN = 24
NOISE_OCTAVES = 2


class Kit:
    def __init__(self, source):
        self.source = source
        self.archive = zipfile.ZipFile(source) if zipfile.is_zipfile(source) else None
        self.names = set(self.archive.namelist()) if self.archive else set()
        self.checksums = self.parse_checksums(self.read_unverified(KIT_CHECKSUMS))

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        if self.archive:
            self.archive.close()

    @staticmethod
    def parse_checksums(data):
        checksums = {}
        for line in data.decode("utf-8").splitlines():
            digest, _, name = line.strip().partition("  ")
            if digest and name:
                checksums[name] = digest.lower()
        return checksums

    def read_unverified(self, relative):
        for candidate in (KIT_ROOT + relative, relative):
            data = self.read_member(candidate)
            if data is not None:
                return data
        raise SystemExit("the kit has no %s" % relative)

    def read(self, relative):
        expected = self.checksums.get(relative)
        if expected is None:
            raise SystemExit("%s lists no checksum for %s" % (KIT_CHECKSUMS, relative))
        data = self.read_unverified(relative)
        if hashlib.sha256(data).hexdigest() != expected:
            raise SystemExit("%s does not match its %s entry" % (relative, KIT_CHECKSUMS))
        return data

    def read_member(self, name):
        if self.archive:
            return self.archive.read(name) if name in self.names else None
        path = os.path.join(self.source, *name.split("/"))
        if not os.path.isfile(path):
            return None
        with open(path, "rb") as handle:
            return handle.read()

    def table(self, relative):
        return json.loads(self.read(relative).decode("utf-8"))


def fog_column(row, index):
    return row["Field_1_60_1_69876_%03d" % index]


def rgb(value):
    return int(value) & RGB_MASK


def layer_flags(row):
    return int(fog_column(row, FLAGS_COLUMN))


def layer_index(row):
    return int(fog_column(row, LAYER_INDEX_COLUMN))


def layers_by_index(rows):
    selected = [r for r in rows if layer_flags(r) & CLIENT_SELECTED_FLAG and 0 <= layer_index(r) < LAYER_INDEX_SLOTS]
    selected.sort(key=lambda r: r["ID"])
    slots = [None] * LAYER_INDEX_SLOTS
    for row in selected:
        if slots[layer_index(row)] is None:
            slots[layer_index(row)] = row
    while slots and slots[-1] is None:
        slots.pop()
    return slots


class Placements:
    def __init__(self, lights, zone_lights, origin):
        self.lights = lights
        self.zone_lights = zone_lights
        self.origin = origin
        self.outlines = {}

    def light_ids(self):
        return {struct.unpack_from(LIGHT_FORMAT, light)[0] for light in self.lights}

    def params_ids(self):
        ids = set()
        for light in self.lights:
            ids.update(p for p in struct.unpack_from(LIGHT_FORMAT, light)[7:] if p)
        return ids


def unpack_records(data, offset, record_format, count):
    size = struct.calcsize(record_format)
    return [data[offset + i * size:offset + (i + 1) * size] for i in range(count)], offset + size * count


def zone_placement(record):
    return struct.unpack_from(ZONE_LIGHT_FORMAT, record)[:5]


def parse_placements(data, origin):
    magic, version = struct.unpack_from("<4sI", data)
    if magic != FILE_MAGIC or version not in PLACEMENT_RECORD_SIZES:
        raise SystemExit("%s is not a fogdata.bin of a known version" % origin)
    header_format = PLACEMENT_HEADER_FORMATS[version]
    counts = struct.unpack_from(header_format, data)[2:8]
    light_count, params_count, key_count, layer_count, zone_count, point_count = counts
    sizes = PLACEMENT_RECORD_SIZES[version]
    header_size = struct.calcsize(header_format)
    lights, offset = unpack_records(data, header_size, LIGHT_FORMAT, light_count)
    offset += params_count * sizes["params"] + key_count * sizes["key"] + layer_count * sizes["layer"]
    zones, offset = unpack_records(data, offset, ZONE_LIGHT_FORMAT, zone_count)
    points, offset = unpack_records(data, offset, ZONE_POINT_FORMAT, point_count)
    placements = Placements(lights, [zone_placement(z) for z in zones], "%s (format %d)" % (origin, version))
    for zone in zones:
        zone_id, _, _, _, _, first, count = struct.unpack_from(ZONE_LIGHT_FORMAT, zone)
        placements.outlines[zone_id] = b"".join(points[first:first + count])
    return placements


def placements_from_bin(path):
    with open(path, "rb") as handle:
        return parse_placements(handle.read(), path)


def csv_float(text):
    return float(text) if text else 0.0


def csv_int(text):
    return int(float(text or 0))


def placement_csv_tables(source):
    tables = {}
    if zipfile.is_zipfile(source):
        with zipfile.ZipFile(source) as archive:
            members = {m.rsplit("/", 1)[-1]: m for m in reversed(archive.namelist())}
            for name in PLACEMENT_TABLE_NAMES:
                if name not in members:
                    raise SystemExit("the archive has no %s" % name)
                tables[name] = archive.read(members[name]).decode("utf-8")
    else:
        for name in PLACEMENT_TABLE_NAMES:
            with open(os.path.join(source, name), encoding="utf-8") as handle:
                tables[name] = handle.read()
    return {name: list(csv.DictReader(io.StringIO(text))) for name, text in tables.items()}


def pack_csv_light(row):
    return struct.pack(
        LIGHT_FORMAT,
        int(row["ID"]),
        csv_int(row["ContinentID"]),
        csv_float(row["GameCoords_0"]),
        csv_float(row["GameCoords_1"]),
        csv_float(row["GameCoords_2"]),
        csv_float(row["GameFalloffStart"]),
        csv_float(row["GameFalloffEnd"]),
        *[csv_int(row["LightParamsID_%d" % slot]) for slot in range(LIGHT_PARAMS_SLOTS)],
    )


def csv_zone_placement(row):
    record = struct.pack(ZONE_LIGHT_FORMAT, int(row["ID"]), csv_int(row["MapID"]), int(row["LightID"]),
                         csv_float(row["Zmin"]), csv_float(row["Zmax"]), 0, 0)
    return zone_placement(record)


def placements_from_csv(source):
    tables = placement_csv_tables(source)
    lights = [pack_csv_light(row) for row in tables["Light.csv"]]
    zones = sorted((csv_zone_placement(row) for row in tables["ZoneLight.csv"]), key=lambda zone: zone[0])
    return Placements(lights, zones, "%s (CSV exports)" % source)


def read_placements(source):
    if os.path.isfile(source) and not zipfile.is_zipfile(source):
        return placements_from_bin(source)
    return placements_from_csv(source)


def pack_header(light_count, params_count, key_count, layer_count, zone_light_count, zone_point_count, curve_count):
    return struct.pack(HEADER_FORMAT, FILE_MAGIC, FORMAT_VERSION, light_count, params_count, key_count, layer_count,
                       zone_light_count, zone_point_count, curve_count)


def pack_params(params_id, first_key, key_count, glow):
    return struct.pack(PARAMS_FORMAT, params_id, first_key, key_count, glow)


def pack_key(half_minute_of_day, layer_count, first_layer, direct_rgb, grading_curve):
    return struct.pack(KEY_FORMAT, half_minute_of_day, layer_count, first_layer, direct_rgb, grading_curve)


def noise_pairs(row):
    return [value for column in NOISE_PAIR_COLUMNS for value in fog_column(row, column)[:NOISE_OCTAVES]]


def pack_layer(row):
    if row is None:
        return struct.pack(LAYER_FORMAT, *([0] * 4 + [0.0] * 11 + [0] + [0.0] * 11))
    return struct.pack(
        LAYER_FORMAT,
        rgb(fog_column(row, DIFFUSE_COLUMN)),
        rgb(fog_column(row, EMISSIVE_COLUMN)),
        rgb(fog_column(row, SHADOW_EMISSIVE_COLUMN)),
        layer_flags(row) & U32_MASK,
        fog_column(row, START_COLUMN),
        fog_column(row, DENSITY_COLUMN),
        fog_column(row, SHADOW_MULTIPLIER_COLUMN),
        fog_column(row, UPPER_DENSITY_COLUMN),
        fog_column(row, UPPER_HEIGHT_COLUMN),
        fog_column(row, LOWER_DENSITY_COLUMN),
        fog_column(row, LOWER_HEIGHT_COLUMN),
        fog_column(row, INTENSITY_COLUMN),
        fog_column(row, G_COLUMN),
        fog_column(row, STRENGTH_COLUMN),
        fog_column(row, EXPONENT_COLUMN),
        rgb(fog_column(row, NOISE_FADE_COLUMN)),
        *[fog_column(row, column) for column in NOISE_DIRECTION_COLUMNS],
        *noise_pairs(row),
        fog_column(row, UNMAPPED_TOGGLE_COLUMN),
    )


def blp_strip(file_data_id, data):
    header = struct.unpack_from(BLP_HEADER_FORMAT, data)
    magic, kind, encoding, _, _, _, width, height = header[:8]
    offset, size = header[BLP_FIRST_OFFSET_FIELD], header[BLP_FIRST_SIZE_FIELD]
    if (magic != BLP_MAGIC or kind != BLP_TYPE or encoding != BLP_RAW_BGRA
            or (width, height) != (LUT_STRIP_WIDTH, LUT_SIDE) or size != width * height * BGRA_BYTES
            or offset + size > len(data)):
        raise SystemExit("grading LUT %d is not a %dx%d BGRA8 BLP2 strip" % (file_data_id, LUT_STRIP_WIDTH, LUT_SIDE))
    return data[offset:offset + size]


def lut_outputs_by_input(strip):
    outputs = [[] for _ in range(LUT_SIDE)]
    for green in range(LUT_SIDE):
        for blue in range(LUT_SIDE):
            for red in range(LUT_SIDE):
                texel = (green * LUT_STRIP_WIDTH + blue * LUT_SIDE + red) * BGRA_BYTES
                outputs[red].append(strip[texel + RED])
                outputs[green].append(strip[texel + GREEN])
                outputs[blue].append(strip[texel + BLUE])
    return outputs


def grading_curve(kit, file_data_id):
    outputs = lut_outputs_by_input(blp_strip(file_data_id, kit.read(GRADING_ASSET % file_data_id)))
    curve = [round(sum(values) / len(values)) for values in outputs]
    deviation = max(abs(value - entry) for values, entry in zip(outputs, curve) for value in values)
    if deviation > GRADING_CURVE_TOLERANCE:
        raise SystemExit("grading LUT %d is not one curve shared by R, G and B (deviation %d)"
                         % (file_data_id, deviation))
    return struct.pack(GRADING_CURVE_FORMAT, file_data_id, *curve), deviation


def kit_outlines(kit):
    points_by_zone = {}
    for row in kit.table(ZONE_POINT_TABLE)["rows"]:
        points_by_zone.setdefault(row["ZoneLightID"], []).append(row)
    outlines = {}
    for zone_id, points in points_by_zone.items():
        ordered = sorted(points, key=lambda point: point["PointOrder"])
        outlines[zone_id] = b"".join(struct.pack(ZONE_POINT_FORMAT, *point["Pos"]) for point in ordered)
    return outlines


def fog_keys_by_params(kit):
    fog = kit.table(FOG_TABLE)
    if fog["layout_hash"] != FOG_TABLE_LAYOUT:
        raise SystemExit("LightDataGlobalVolumeFog has layout %s, not %s; the column numbers may have moved"
                         % (fog["layout_hash"], FOG_TABLE_LAYOUT))
    fog_by_data = {}
    for row in fog["rows"]:
        fog_by_data.setdefault(row["LightDataID"], []).append(row)
    keys_by_params = {}
    for row in kit.table(LIGHT_DATA_TABLE)["rows"]:
        layers = layers_by_index(fog_by_data.get(row["ID"], []))
        if layers:
            key = (row["Time"] & U16_MASK, layers, rgb(row["DirectColor"]), row["ColorGradingFileDataID"],
                   row["DarkerColorGradingFileDataID"])
            keys_by_params.setdefault(row["LightParamID"], []).append(key)
    return keys_by_params


def glow_by_params(kit):
    return {row["ID"]: row["Glow"] for row in kit.table(LIGHT_PARAMS_TABLE)["rows"]}


def convert(kit, placements):
    keys_by_params = fog_keys_by_params(kit)
    glows = glow_by_params(kit)
    placed_params = sorted(placements.params_ids() & set(glows))
    grading_ids = sorted({key[3] for p in placed_params for key in keys_by_params.get(p, []) if key[3]})
    curves = [grading_curve(kit, file_data_id) for file_data_id in grading_ids]
    params_blob, keys_blob, layers_blob = [], [], []
    key_count = layer_count = darker_keys = 0
    for params_id in placed_params:
        keys = sorted(keys_by_params.get(params_id, []), key=lambda k: k[0])
        params_blob.append(pack_params(params_id, key_count, len(keys), glows[params_id]))
        for half_minute_of_day, layers, direct_rgb, grading_id, darker_grading_id in keys:
            curve = grading_ids.index(grading_id) + 1 if grading_id else NO_GRADING_CURVE
            keys_blob.append(pack_key(half_minute_of_day, len(layers), layer_count, direct_rgb, curve))
            layers_blob.extend(pack_layer(r) for r in layers)
            layer_count += len(layers)
            key_count += 1
            darker_keys += 1 if darker_grading_id else 0

    outlines = kit_outlines(kit)
    light_ids = placements.light_ids()
    zones_blob, points_blob = [], []
    zone_point_count = 0
    kept_zones = []
    for zone_id, map_id, light_id, z_min, z_max in placements.zone_lights:
        outline = outlines.get(zone_id, b"")
        point_count = len(outline) // struct.calcsize(ZONE_POINT_FORMAT)
        if light_id not in light_ids or point_count < MINIMUM_OUTLINE_POINTS:
            continue
        zones_blob.append(struct.pack(ZONE_LIGHT_FORMAT, zone_id, map_id, light_id, z_min, z_max, zone_point_count,
                                      point_count))
        points_blob.append(outline)
        zone_point_count += point_count
        kept_zones.append(zone_id)

    header = pack_header(len(placements.lights), len(params_blob), key_count, layer_count, len(zones_blob),
                         zone_point_count, len(curves))
    blob = header + b"".join(placements.lights + params_blob + keys_blob + layers_blob + zones_blob + points_blob +
                             [curve for curve, _ in curves])
    changed_outlines = [z for z in kept_zones if z in placements.outlines and placements.outlines[z] != outlines[z]]
    summary = {"keys": key_count, "layers": layer_count, "zones": kept_zones, "changed_outlines": changed_outlines,
               "unplaced_params": len(placements.params_ids()) - len(placed_params), "grading_ids": grading_ids,
               "curve_deviation": max((deviation for _, deviation in curves), default=0), "darker_keys": darker_keys}
    return blob, summary


def check_placements_carried(blob, placements, kept_zones):
    written = parse_placements(blob, "the written fogdata")
    if written.lights != placements.lights:
        raise SystemExit("the written light records differ from %s" % placements.origin)
    expected_zones = [zone for zone in placements.zone_lights if zone[0] in kept_zones]
    if written.zone_lights != expected_zones:
        raise SystemExit("the written zone lights differ from %s" % placements.origin)


def main():
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument("kit", help="the Forever fog and lighting kit folder or its zip archive")
    parser.add_argument("--placements", required=True,
                        help="a previous fogdata.bin, or a folder or zip archive with the Classic client's Light.csv "
                             "and ZoneLight.csv exports")
    parser.add_argument("output", help="output path, normally data/fogdata.bin")
    args = parser.parse_args()
    placements = read_placements(args.placements)
    with Kit(args.kit) as kit:
        blob, summary = convert(kit, placements)
    check_placements_carried(blob, placements, summary["zones"])
    with open(args.output, "wb") as handle:
        handle.write(blob)
    params = struct.unpack_from(HEADER_FORMAT, blob)[3]
    print("%s: %d lights and %d zone lights placed from %s (%d zone outlines changed by the kit), %d light params "
          "(%d referenced params missing from LightParams), %d keys, %d layers, grading curves %s (largest deviation "
          "from one shared curve %d/255; %d keys with a darker grading LUT left out), %d bytes"
          % (args.output, len(placements.lights), len(summary["zones"]), placements.origin,
             len(summary["changed_outlines"]), params, summary["unplaced_params"], summary["keys"], summary["layers"],
             summary["grading_ids"], summary["curve_deviation"], summary["darker_keys"], len(blob)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
