import argparse
import hashlib
import json
import math
import os
import struct
import sys
import zipfile
from collections import Counter

DESCRIPTION = "Convert the Forever water kit's liquid presets, FFT tiles and foam masks into data/waterdata.bin."
KIT_ROOT = "coa-forever-water-kit/"
LIQUID_TYPE_TABLE = "data/decoded/LiquidType.json"
TEXTURE_LINK_TABLE = "data/decoded/LiquidTypeXTexture.json"
TILE_LINK_TABLE = "data/decoded/LiquidTypeXFFTTile.json"
TILE_TABLE = "data/decoded/FFTTile.json"
TEXTURE_MANIFEST = "textures/manifest.json"
TEXTURE_PATH = "textures/%d.blp"

GENERIC_LAKE = 1240
GENERIC_RIVER = 1288
GENERIC_OCEAN = 1250
WMO_INTERIOR = 1290
PRESET_LIQUIDS = (GENERIC_LAKE, GENERIC_RIVER, GENERIC_OCEAN, WMO_INTERIOR)
PBR_WATER_MATERIAL = 130
LIQUID_FLOATS = 38
LIQUID_COLORS = 3
PRESET_TILES = 4
MAX_TILES = 8
MASK_SLOTS = 6
NO_INDEX = -1

ABSORPTION_COLOR = 0
SCATTERING_TOP_COLOR = 1
SCATTERING_BOTTOM_COLOR = 2
ABSORPTION_DENSITY = 0
PHASE_ANISOTROPY = 1
WAVE_HEIGHT_RANGE = 2
SCATTERING_INTENSITIES = range(3, 7)
DEPTH_FADE_FOAM = range(7, 11)
SHORE_FOAM = range(11, 15)
WAVE_FOAM_INTENSITY = 15
WAVE_FOAM_SCALING = range(16, 19)
WAVE_FOAM_SCROLL = 19
FLOW = range(20, 24)
SUN_ROUGHNESS = 27
ENVIRONMENT_ROUGHNESS = 29
REFLECTION_GAIN = 30
ROUGHNESS = (SUN_ROUGHNESS, ENVIRONMENT_ROUGHNESS, REFLECTION_GAIN)

TILE_SIZE_COLUMN = 0
TILE_AMPLITUDE_COLUMN = 1
TILE_WIND_ALIGNMENT_COLUMN = 2
TILE_WIND_MULTIPLIER_COLUMN = 3
TILE_FOAM_COLUMNS = (4, 5, 6)
TILE_OXYGEN_COLUMNS = (7, 8, 9)

FILE_MAGIC = b"VWD1"
FORMAT_VERSION = 1
HEADER_FORMAT = "<4s13I"
PRESET_FORMAT = "<I40f4i6i"
TILE_FORMAT = "<I10f"
MASK_FORMAT = "<3I6f2I"

BLP_MAGIC = b"BLP2"
BLP_TYPE = 1
BLP_RAW_BGRA = 3
BLP_ALPHA_BITS = 8
BLP_HEADER_FORMAT = "<4sI4B2I16I16I"
BLP_PIXEL_DATA_START = 1172
BLP_MIP_SLOTS = 16
BGRA_BYTES = 4
BLUE, GREEN, RED, ALPHA = range(BGRA_BYTES)

CHANNEL_MAX = 255.0
DISPLAY_GAMMA = 2.2
MIN_TRANSMITTANCE = 1.0 / CHANNEL_MAX
TINT_SAMPLE_STRIDE = 13


class Kit:
    def __init__(self, source):
        self.source = source
        self.archive = zipfile.ZipFile(source) if zipfile.is_zipfile(source) else None
        self.names = set(self.archive.namelist()) if self.archive else set()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        if self.archive:
            self.archive.close()

    def read(self, relative):
        for candidate in (KIT_ROOT + relative, relative):
            data = self.read_member(candidate)
            if data is not None:
                return data
        raise SystemExit("the kit has no %s" % relative)

    def read_member(self, name):
        if self.archive:
            return self.archive.read(name) if name in self.names else None
        path = os.path.join(self.source, *name.split("/"))
        if not os.path.isfile(path):
            return None
        with open(path, "rb") as handle:
            return handle.read()

    def rows(self, table):
        return json.loads(self.read(table).decode("utf-8"))["rows"]


def tile_column(row, index):
    return row["Field_1_60_1_69876_%03d" % index]


def argb_channels(value):
    argb = value & 0xFFFFFFFF
    return ((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF)


def optical_depth(channel):
    transmittance = max(channel / CHANNEL_MAX, MIN_TRANSMITTANCE)
    return -math.log(transmittance) if transmittance < 1.0 else 0.0


def linearised(channel):
    return (channel / CHANNEL_MAX) ** DISPLAY_GAMMA


def absorption(row):
    coefficients = [optical_depth(c) for c in argb_channels(row["Color"][ABSORPTION_COLOR])]
    return coefficients + [row["Float"][ABSORPTION_DENSITY]]


def scattering_color(row, color, extent):
    return [linearised(c) for c in argb_channels(row["Color"][color])] + [row["Float"][extent]]


def floats(row, indices):
    return [row["Float"][i] for i in indices]


def liquid_row(liquids, liquid_id):
    row = liquids.get(liquid_id)
    if row is None:
        raise SystemExit("LiquidType has no row %d" % liquid_id)
    if (row["MaterialID"] != PBR_WATER_MATERIAL or len(row["Float"]) != LIQUID_FLOATS
            or len(row["Color"]) != LIQUID_COLORS):
        raise SystemExit("LiquidType %d is not a PBR water preset" % liquid_id)
    return row


def related_tile_ids(tile_links, liquid_id):
    links = sorted((link for link in tile_links if link["LiquidTypeID"] == liquid_id), key=lambda link: link["ID"])
    if len(links) > PRESET_TILES:
        raise SystemExit("LiquidType %d has %d FFT tiles, more than %d" % (liquid_id, len(links), PRESET_TILES))
    return [link["FFTTileID"] for link in links]


def texture_ids_by_slot(texture_links, liquid_id):
    slots = [None] * MASK_SLOTS
    for link in texture_links:
        if link["LiquidTypeID"] != liquid_id:
            continue
        slot = link["OrderIndex"]
        if not 0 <= slot < MASK_SLOTS or slots[slot] is not None:
            raise SystemExit("LiquidType %d has an animated or out-of-range texture slot %d" % (liquid_id, slot))
        slots[slot] = link["FileDataID"]
    return slots


def first_appearance(groups):
    ordered = []
    for group in groups:
        for item in group:
            if item is not None and item not in ordered:
                ordered.append(item)
    return ordered


def indices_into(ordered, items, width):
    indices = [ordered.index(item) if item is not None else NO_INDEX for item in items]
    return indices + [NO_INDEX] * (width - len(items))


def pack_preset(row, tiles, masks):
    return struct.pack(
        PRESET_FORMAT,
        row["ID"],
        *absorption(row),
        *floats(row, SCATTERING_INTENSITIES),
        *scattering_color(row, SCATTERING_TOP_COLOR, WAVE_HEIGHT_RANGE),
        *scattering_color(row, SCATTERING_BOTTOM_COLOR, PHASE_ANISOTROPY),
        *floats(row, DEPTH_FADE_FOAM),
        *floats(row, SHORE_FOAM),
        row["Float"][WAVE_FOAM_INTENSITY], row["Float"][WAVE_FOAM_SCROLL], 0.0, 0.0,
        *floats(row, WAVE_FOAM_SCALING), 0.0,
        *floats(row, FLOW),
        *floats(row, ROUGHNESS), 0.0,
        *tiles,
        *masks,
    )


def pack_tile(row):
    size = tile_column(row, TILE_SIZE_COLUMN)
    if not isinstance(size, int) or size <= 0:
        raise SystemExit("FFTTile %d has no positive integer size" % row["ID"])
    return struct.pack(
        TILE_FORMAT,
        row["ID"],
        float(size),
        tile_column(row, TILE_AMPLITUDE_COLUMN),
        tile_column(row, TILE_WIND_MULTIPLIER_COLUMN),
        tile_column(row, TILE_WIND_ALIGNMENT_COLUMN),
        *(tile_column(row, i) for i in TILE_FOAM_COLUMNS),
        *(tile_column(row, i) for i in TILE_OXYGEN_COLUMNS),
    )


def full_mip_count(side):
    return side.bit_length()


def verified_texture(kit, manifest, file_data_id):
    entry = manifest.get(file_data_id)
    if entry is None:
        raise SystemExit("textures/manifest.json has no texture %d" % file_data_id)
    data = kit.read(TEXTURE_PATH % file_data_id)
    if hashlib.sha256(data).hexdigest() != entry["sha256"].lower():
        raise SystemExit("texture %d does not match its manifest SHA-256" % file_data_id)
    return data, entry


def blp_levels(file_data_id, data, entry):
    if len(data) < BLP_PIXEL_DATA_START:
        raise SystemExit("texture %d is shorter than a BLP2 header" % file_data_id)
    header = struct.unpack_from(BLP_HEADER_FORMAT, data)
    magic, kind, encoding, alpha_bits, _, has_mips, width, height = header[:8]
    offsets = header[8:8 + BLP_MIP_SLOTS]
    sizes = header[8 + BLP_MIP_SLOTS:]
    square_power_of_two = width == height and width > 0 and width & (width - 1) == 0
    if (magic != BLP_MAGIC or kind != BLP_TYPE or encoding != BLP_RAW_BGRA or alpha_bits != BLP_ALPHA_BITS
            or not has_mips or not square_power_of_two or full_mip_count(width) > BLP_MIP_SLOTS
            or (width, height) != (entry["width"], entry["height"])):
        raise SystemExit("texture %d is not a square power-of-two BGRA8 BLP2 with mips" % file_data_id)
    levels = []
    end = BLP_PIXEL_DATA_START
    for level in range(BLP_MIP_SLOTS):
        if level >= full_mip_count(width):
            if offsets[level] or sizes[level]:
                raise SystemExit("texture %d stores mips below 1x1" % file_data_id)
            continue
        side = max(width >> level, 1)
        start, stop = offsets[level], offsets[level] + sizes[level]
        if sizes[level] != side * side * BGRA_BYTES or start < end or stop > len(data):
            raise SystemExit("texture %d level %d is outside the file or has the wrong size" % (file_data_id, level))
        levels.append(data[start:stop])
        end = stop
    return width, levels


def joint_histogram(coverage, channel):
    return Counter(zip(coverage, channel))


def fitted_tint(level):
    texel_stride = BGRA_BYTES * TINT_SAMPLE_STRIDE
    coverage = level[ALPHA::texel_stride]
    low, high = [], []
    for channel in (RED, GREEN, BLUE):
        n = sx = sy = sxx = sxy = 0.0
        for (a, c), count in sorted(joint_histogram(coverage, level[channel::texel_stride]).items()):
            x = a / CHANNEL_MAX
            y = linearised(c)
            n += count
            sx += count * x
            sy += count * y
            sxx += count * x * x
            sxy += count * x * y
        spread = n * sxx - sx * sx
        slope = (n * sxy - sx * sy) / spread if spread > 0.0 else 0.0
        intercept = (sy - slope * sx) / n
        low.append(min(max(intercept, 0.0), 1.0))
        high.append(min(max(intercept + slope, 0.0), 1.0))
    return low, high


def convert_mask(kit, manifest, file_data_id, mip_offset):
    data, entry = verified_texture(kit, manifest, file_data_id)
    side, levels = blp_levels(file_data_id, data, entry)
    chain = b"".join(level[ALPHA::BGRA_BYTES] for level in levels)
    low, high = fitted_tint(levels[0])
    record = struct.pack(MASK_FORMAT, file_data_id, side, len(levels), *low, *high, mip_offset, len(chain))
    return record, chain, side, len(levels)


def convert(kit):
    liquids = {row["ID"]: row for row in kit.rows(LIQUID_TYPE_TABLE)}
    tile_links = kit.rows(TILE_LINK_TABLE)
    texture_links = kit.rows(TEXTURE_LINK_TABLE)
    tile_rows = {row["ID"]: row for row in kit.rows(TILE_TABLE)}
    manifest = {entry["file_data_id"]: entry for entry in json.loads(kit.read(TEXTURE_MANIFEST).decode("utf-8"))}

    rows = [liquid_row(liquids, liquid_id) for liquid_id in PRESET_LIQUIDS]
    tiles_by_preset = [related_tile_ids(tile_links, row["ID"]) for row in rows]
    slots_by_preset = [texture_ids_by_slot(texture_links, row["ID"]) for row in rows]
    tile_ids = first_appearance(tiles_by_preset)
    mask_ids = first_appearance(slots_by_preset)
    if len(tile_ids) > MAX_TILES:
        raise SystemExit("the presets use %d FFT tiles, more than %d" % (len(tile_ids), MAX_TILES))
    missing_tiles = [tile_id for tile_id in tile_ids if tile_id not in tile_rows]
    if missing_tiles:
        raise SystemExit("FFTTile has no rows %s" % missing_tiles)

    presets = []
    for row, tiles, slots in zip(rows, tiles_by_preset, slots_by_preset):
        presets.append(pack_preset(row, indices_into(tile_ids, tiles, PRESET_TILES),
                                   indices_into(mask_ids, slots, MASK_SLOTS)))
    tiles = [pack_tile(tile_rows[tile_id]) for tile_id in tile_ids]
    masks, chains, shapes = [], [], []
    mip_offset = 0
    for file_data_id in mask_ids:
        record, chain, side, levels = convert_mask(kit, manifest, file_data_id, mip_offset)
        masks.append(record)
        chains.append(chain)
        shapes.append((side, levels))
        mip_offset += len(chain)

    preset_offset = struct.calcsize(HEADER_FORMAT)
    tile_offset = preset_offset + struct.calcsize(PRESET_FORMAT) * len(presets)
    mask_offset = tile_offset + struct.calcsize(TILE_FORMAT) * len(tiles)
    mip_data_offset = mask_offset + struct.calcsize(MASK_FORMAT) * len(masks)
    file_size = mip_data_offset + mip_offset
    header = struct.pack(HEADER_FORMAT, FILE_MAGIC, FORMAT_VERSION, file_size,
                         preset_offset, len(presets), struct.calcsize(PRESET_FORMAT),
                         tile_offset, len(tiles), struct.calcsize(TILE_FORMAT),
                         mask_offset, len(masks), struct.calcsize(MASK_FORMAT),
                         mip_data_offset, mip_offset)
    blob = header + b"".join(presets + tiles + masks + chains)
    return blob, len(presets), len(tiles), shapes, mip_offset


def main():
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument("source", help="the Forever water kit folder or its zip archive")
    parser.add_argument("output", help="output path, normally data/waterdata.bin")
    args = parser.parse_args()
    with Kit(args.source) as kit:
        blob, presets, tiles, shapes, mip_bytes = convert(kit)
    with open(args.output, "wb") as handle:
        handle.write(blob)
    mask_shapes = ", ".join("%dpx/%d levels" % shape for shape in sorted(set(shapes)))
    print("%s: %d presets, %d FFT tiles, %d foam masks (%s), %d mip bytes, %d bytes"
          % (args.output, presets, tiles, len(shapes), mask_shapes, mip_bytes, len(blob)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
