#!/usr/bin/env python3
"""Independent check of a P0 world file, written from the specs alone.

Reads a world file with the standard-library sqlite3 module (read-only,
immutable) and checks it against docs/specs/p0-02-world-db.md and
docs/specs/p0-04-worldgen-cli.md without using any of the project's C++:

  - SQLite header fields (P0-02 6.1);
  - schema text (P0-02 5.4, 8) and the meta rows (P0-02 5.5, 8);
  - every layer blob: header (P0-02 7.1), strict decode of codec 0/1 (7.2,
    7.3) and canonical re-encoding to the same bytes;
  - for a test-pattern world, every cell of the three layers against the
    formulas of P0-04 5.2, computed with core_reference.py;
  - the `hash --detail` lines (P0-04 8), the `report` text (P0-04 7) and the
    SHA-256 of the gray `render_rgb` buffer of each layer (P0-04 6).

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/p0_world_check.py check WORLD
            (--seed N | --phrase TEXT) [--map-size N] [--test-pattern]
            [--detail FILE] [--report FILE] [--generator-version V]
            [--content-hash HEX]

        Checks WORLD against the expected content for those generate
        arguments. --detail and --report name files holding the output of
        `worldgen-cli hash --detail` and `worldgen-cli report` for WORLD;
        they are compared byte for byte with the text computed here.
        Prints the computed detail lines and render hashes; exit 1 on any
        disagreement. --phrase is the phrase as given to `generate
        --seed-phrase`, taken as the raw argument bytes (any bytes, not only
        UTF-8); the expected seed_phrase row is its scrubbed form and the
        seed is seed_from_phrase of it (P0-01 7.3, P0-02 amendment 12).
        Write --phrase=TEXT for a phrase that starts with '-'.
        --generator-version is the expected meta.generator_version
        (default GENERATOR_VERSION below). --content-hash is the expected
        meta.content_hash (default the hash of no data, e3b0c442...b855);
        a world written by worldgen-cli since the `worldgen` kind was
        registered carries the hash of the loaded data (P0-03 amendment 17),
        which `worldgen-cli report` prints.

    python3 -B worldgen/tests/reference/p0_world_check.py stats WORLD...

        Decodes the layers of each world (header and codec checks only) and
        prints one line per world and layer: file sha256, layer, min, max,
        sum, cell count. Used by seed sweeps.

Standard library only; no floating point in the checks.
"""

import argparse
import hashlib
import os
import sqlite3
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import core_reference as ref  # noqa: E402

TYPES = {1: ("u8", 1, False), 2: ("i8", 1, True), 3: ("u16", 2, False), 4: ("i16", 2, True),
         5: ("u32", 4, False), 6: ("i32", 4, True), 7: ("u64", 8, False), 8: ("i64", 8, True)}
TYPE_CODE = {v[0]: k for k, v in TYPES.items()}

DDL_META = b"CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL) STRICT, WITHOUT ROWID"
DDL_LAYER = (b"CREATE TABLE layer (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, width INTEGER NOT NULL, "
             b"height INTEGER NOT NULL, format TEXT NOT NULL, data BLOB NOT NULL) STRICT")
# Schema version 2 adds table pass (P1-01 9.1, docs/specs/schema.md); every
# world has it, with no rows in a P0 world.
DDL_PASS = (b"CREATE TABLE pass (id INTEGER PRIMARY KEY, range_id INTEGER NOT NULL, kind INTEGER NOT NULL, "
            b"capped INTEGER NOT NULL, col_x INTEGER NOT NULL, col_y INTEGER NOT NULL, saddle_dm INTEGER NOT NULL, "
            b"col_dm INTEGER NOT NULL, path BLOB NOT NULL) STRICT")
# Schema version 3 adds tables biome and climate, executed after pass
# (P1-04 3, P1-03 3); every world has both, with no rows in a P0 world.
# biome.key is UNIQUE, so SQLite adds the index sqlite_autoindex_biome_1.
DDL_BIOME = (b"CREATE TABLE biome (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, "
             b"colour INTEGER NOT NULL, water INTEGER NOT NULL) STRICT")
# Schema version 4 replaces the climate statement in place with one that has
# the column temp_offset_dc (P1-03 3, P0-02 amendment 16).
DDL_CLIMATE = (b"CREATE TABLE climate (id INTEGER PRIMARY KEY, wind_toward INTEGER NOT NULL, "
               b"temp_offset_dc INTEGER NOT NULL) STRICT")
# Schema version 5 adds tables region, region_border and region_biome,
# executed sixth to eighth, after climate (P1-06 3.1, P0-02 amendment 17);
# every world has them, with no rows in a P0 world. No automatic index.
DDL_REGION = (b"CREATE TABLE region (id INTEGER PRIMARY KEY, name TEXT NOT NULL, biome INTEGER NOT NULL, "
              b"x_min INTEGER NOT NULL, y_min INTEGER NOT NULL, x_max INTEGER NOT NULL, y_max INTEGER NOT NULL, "
              b"seed_x INTEGER NOT NULL, seed_y INTEGER NOT NULL, landmass INTEGER NOT NULL, cells INTEGER NOT NULL, "
              b"lake_cells INTEGER NOT NULL, river_cells INTEGER NOT NULL, river_class_max INTEGER NOT NULL, "
              b"coast_cells INTEGER NOT NULL, walk_cells INTEGER NOT NULL, walk_area INTEGER NOT NULL, "
              b"elev_mean_dm INTEGER NOT NULL, elev_max_dm INTEGER NOT NULL, fertility_mean INTEGER NOT NULL, "
              b"forage_mean INTEGER NOT NULL, timber_mean INTEGER NOT NULL, ore_mean INTEGER NOT NULL, "
              b"fresh_water_mean INTEGER NOT NULL, habitability_mean INTEGER NOT NULL, "
              b"habitability_p90 INTEGER NOT NULL, ore_rich_cells INTEGER NOT NULL, "
              b"ore_reach_cells INTEGER NOT NULL) STRICT")
DDL_REGION_BORDER = (b"CREATE TABLE region_border (a INTEGER NOT NULL, b INTEGER NOT NULL, "
                     b"length_m INTEGER NOT NULL, low_dm INTEGER NOT NULL, low_x INTEGER NOT NULL, "
                     b"low_y INTEGER NOT NULL, walk_pairs INTEGER NOT NULL, river_pairs INTEGER NOT NULL, "
                     b"river_class_max INTEGER NOT NULL, pass_id INTEGER NOT NULL, passes INTEGER NOT NULL, "
                     b"PRIMARY KEY (a, b)) STRICT, WITHOUT ROWID")
DDL_REGION_BIOME = (b"CREATE TABLE region_biome (region INTEGER NOT NULL, biome INTEGER NOT NULL, "
                    b"cells INTEGER NOT NULL, PRIMARY KEY (region, biome)) STRICT, WITHOUT ROWID")
SCHEMA_VERSION = 5
# The generator version the checked worlds are expected to carry (P0-02 5.5,
# amendment 12). One edit per version bump; --generator-version overrides it.
GENERATOR_VERSION = "0.11.0"
EMPTY_SHA = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"


class Failures:
    def __init__(self):
        self.items = []

    def check(self, cond, what):
        if not cond:
            self.items.append(what)
        return cond


# --- blob codec (P0-02 section 7) -------------------------------------------

def leb128(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def range_of(tcode):
    _, size, signed = TYPES[tcode]
    bits = 8 * size
    return (-(1 << (bits - 1)), (1 << (bits - 1)) - 1) if signed else (0, (1 << bits) - 1)


def encode_delta_varint(values):
    out = bytearray()
    prev = 0
    zeros = 0
    for e in values:
        d = (e - prev) & ref.M64
        sd = d - (1 << 64) if d >> 63 else d
        z = ((d << 1) & ref.M64) ^ ((sd >> 63) & ref.M64)
        prev = e & ref.M64
        if z == 0:
            zeros += 1
            continue
        if zeros:
            out += leb128(0) + leb128(zeros - 1)
            zeros = 0
        out += leb128(z)
    if zeros:
        out += leb128(0) + leb128(zeros - 1)
    return bytes(out)


def decode_blob(blob):
    """Strict decode; returns (tcode, codec, width, height, values) or raises ValueError."""
    if len(blob) < 24:
        raise ValueError("blob shorter than 24 bytes")
    if blob[0:4] != b"UMLY":
        raise ValueError("bad magic")
    version, tcode, codec, width, height, plen = struct.unpack("<HBBIIQ", blob[4:24])
    if version != 1 or tcode not in TYPES or codec not in (0, 1):
        raise ValueError("bad version, type or codec")
    if not (1 <= width <= 65536 and 1 <= height <= 65536):
        raise ValueError("bad dimensions")
    if plen != len(blob) - 24:
        raise ValueError("payload length mismatch")
    n = width * height
    payload = blob[24:]
    _, size, signed = TYPES[tcode]
    lo, hi = range_of(tcode)
    if codec == 0:
        if len(payload) != n * size:
            raise ValueError("raw payload size")
        vals = [int.from_bytes(payload[i * size:(i + 1) * size], "little", signed=signed) for i in range(n)]
        return tcode, codec, width, height, vals
    vals = []
    pos = 0
    prev = 0
    last_was_run = False

    def read():
        nonlocal pos
        shift = 0
        v = 0
        start = pos
        while True:
            if pos >= len(payload):
                raise ValueError("payload ends inside a token")
            b = payload[pos]
            pos += 1
            v |= (b & 0x7F) << shift
            shift += 7
            if not b & 0x80:
                break
        if v >> 64:
            raise ValueError("LEB128 over 64 bits")
        if leb128(v) != payload[start:pos]:
            raise ValueError("non-shortest LEB128")
        return v

    def push(z):
        nonlocal prev
        d = (z >> 1) ^ ((-(z & 1)) & ref.M64)
        u = (prev + d) & ref.M64
        e = u - (1 << 64) if (signed and u >> 63) else u
        if not (lo <= e <= hi):
            raise ValueError("value out of range for type")
        vals.append(e)
        prev = u

    while pos < len(payload):
        if len(vals) >= n:
            raise ValueError("trailing bytes")
        z = read()
        if z == 0:
            if last_was_run:
                raise ValueError("two consecutive zero-run tokens")
            k = read() + 1
            if len(vals) + k > n:
                raise ValueError("run past element count")
            for _ in range(k):
                push(0)
            last_was_run = True
        else:
            push(z)
            last_was_run = False
    if len(vals) != n:
        raise ValueError("too few elements")
    return tcode, codec, width, height, vals


# --- logical hashes (P0-04 section 8) ----------------------------------------

def enc_str(b):
    return struct.pack("<Q", len(b)) + b


def typed_value(tname, v):
    """A column value from sqlite typeof() and the raw value: None, int, or (kind, bytes)."""
    if tname == "null":
        return None
    if tname == "integer":
        return int(v)
    if tname == "real":
        raise ValueError("REAL value")
    if tname == "text":
        return ("text", bytes(v))
    return ("blob", bytes(v))


def enc_typed(tv):
    if tv is None:
        return b"\x00"
    if isinstance(tv, int):
        return b"\x01" + struct.pack("<q", tv)
    kind, b = tv
    return (b"\x03" if kind == "text" else b"\x04") + enc_str(b)


def escape(b):
    out = bytearray()
    for c in b:
        if c == 0x5C:
            out += b"\\\\"
        elif c < 0x20 or c == 0x7F:
            out += b"\\x%02X" % c
        else:
            out.append(c)
    return bytes(out)


def table_columns(conn, table):
    """Column names in declared order and primary-key columns in key order (bytes)."""
    info = conn.execute("SELECT name, pk FROM pragma_table_info(?) ORDER BY cid", (table,)).fetchall()
    cols = [r[0] for r in info]
    pks = [r[0] for r in sorted((r for r in info if r[1] > 0), key=lambda r: r[1])]
    return cols, pks


def quote(name):
    return '"' + name.decode("utf-8").replace('"', '""') + '"'


def compute(path):
    """Everything derived from the file: header, schema, meta, tables, layers, detail, report."""
    data = open(path, "rb").read()
    uri = "file:" + os.path.abspath(path) + "?mode=ro&immutable=1"
    conn = sqlite3.connect(uri, uri=True)
    conn.text_factory = bytes
    w = {"bytes": data}
    schema_rows = conn.execute(
        "SELECT type, name, tbl_name, sql, typeof(sql) FROM sqlite_schema "
        "ORDER BY type COLLATE BINARY, name COLLATE BINARY").fetchall()
    w["schema_rows"] = schema_rows
    s = bytearray()
    for t, n, tb, sql, tsql in schema_rows:
        s += enc_typed(("text", t)) + enc_typed(("text", n)) + enc_typed(("text", tb))
        s += enc_typed(None if tsql == b"null" else ("text", sql))
    detail = [b"file " + hashlib.sha256(data).hexdigest().encode()]
    detail.append(b"schema " + hashlib.sha256(bytes(s)).hexdigest().encode())
    tables = sorted(r[1] for r in schema_rows if r[0] == b"table" and not r[1].lower().startswith(b"sqlite_"))
    report_tables = []
    w["tables"] = {}
    for tb in tables:
        cols, pks = table_columns(conn, tb.decode())
        if not pks:
            raise ValueError("table without primary key")
        sel = ", ".join(f"typeof({quote(c)}), {quote(c)}" for c in cols)
        order = ", ".join(quote(p) + " COLLATE BINARY" for p in pks)
        rows = conn.execute(f"SELECT {sel} FROM {quote(tb)} ORDER BY {order}").fetchall()
        h = hashlib.sha256(struct.pack("<Q", len(cols)))
        trows = []
        for r in rows:
            tr = [typed_value(r[2 * i].decode(), r[2 * i + 1]) for i in range(len(cols))]
            trows.append(tr)
            for tv in tr:
                h.update(enc_typed(tv))
        w["tables"][tb] = (cols, trows)
        detail.append(b"table " + escape(tb) + b" " + h.hexdigest().encode())
        report_tables.append(escape(tb) + b" = " + str(len(rows)).encode())
    layers = []
    for lid, name, width, height, fmt, blob in conn.execute(
            "SELECT id, name, width, height, format, data FROM layer ORDER BY id").fetchall():
        tcode, codec, bw, bh, vals = decode_blob(blob)
        layers.append({"id": lid, "name": name, "width": width, "height": height, "format": fmt,
                       "blob": bytes(blob), "tcode": tcode, "codec": codec, "bw": bw, "bh": bh, "values": vals})
        _, size, signed = TYPES[tcode]
        lh = hashlib.sha256(struct.pack("<IIB", bw, bh, tcode))
        lh.update(b"".join(v.to_bytes(size, "little", signed=signed) for v in vals))
        detail.append(b"layer " + escape(name) + b" " + lh.hexdigest().encode())
    conn.close()
    w["layers"] = layers
    w["detail"] = b"\n".join(detail) + b"\n"
    meta = w["tables"].get(b"meta", ([], []))[1]
    rep = [b"# worldgen report v2", b"[meta]"]
    for k, v in meta:
        line = escape(k[1]) + b" = " + escape(v[1])
        rep.append(line.rstrip(b" ") if v[1] == b"" else line)
    rep.append(b"[tables]")
    rep += report_tables
    rep.append(b"[layers]")
    if not layers:
        rep.append(b"(none)")
    for L in layers:
        rep.append(escape(L["name"]) + b" " + L["format"] + b" %dx%d codec=%d bytes=%d min=%d max=%d" % (
            L["width"], L["height"], L["codec"], len(L["blob"]), min(L["values"]), max(L["values"])))
    w["report"] = b"\n".join(rep) + b"\n"
    return w


def gray_rgb(values):
    lo, hi = min(values), max(values)
    r = hi - lo
    out = bytearray()
    for v in values:
        g = 0 if r == 0 else (510 * (v - lo) + r) // (2 * r)
        out += bytes((g, g, g))
    return bytes(out)


# --- expected content -------------------------------------------------------

def header_checks(f, data):
    be = lambda off, n: int.from_bytes(data[off:off + n], "big")  # noqa: E731
    f.check(data[0:16] == b"SQLite format 3\x00", "header magic")
    f.check(be(16, 2) == 4096, "page size 4096")
    f.check(data[18] == 1 and data[19] == 1, "write/read format 1, 1")
    f.check(data[20] == 0, "reserved bytes 0")
    f.check(tuple(data[21:24]) == (64, 32, 32), "payload fractions 64 32 32")
    f.check(be(28, 4) * 4096 == len(data), "database size in pages * 4096 == file size")
    f.check(be(32, 4) == 0 and be(36, 4) == 0, "freelist trunk 0, freelist count 0")
    f.check(be(44, 4) == 4, "schema format 4")
    f.check(be(48, 4) == 0, "default page cache size 0")
    f.check(be(52, 4) == 0, "largest root page 0")
    f.check(be(56, 4) == 1, "text encoding UTF-8")
    f.check(be(60, 4) == SCHEMA_VERSION, "user_version %d" % SCHEMA_VERSION)
    f.check(be(64, 4) == 0, "incremental vacuum 0")
    f.check(be(68, 4) == 0x554E4D50, "application_id UNMP")
    f.check(data[72:92] == bytes(20), "reserved 72-91 zero")
    f.check(be(92, 4) == be(24, 4), "version-valid-for == change counter")
    return {"change_counter": be(24, 4), "pages": be(28, 4), "schema_cookie": be(40, 4),
            "sqlite_version": be(96, 4)}


def check_world(args):
    f = Failures()
    w = compute(args.world)
    data = w["bytes"]
    hdr = header_checks(f, data)
    if args.phrase is not None:
        raw = os.fsencode(args.phrase)   # the argument bytes as passed
        try:
            phrase, seed = ref.seed_from_phrase(raw)
        except ref.ParseError as e:
            print("FAIL phrase rejected by seed_from_phrase (%s); generate exits 2 for it" % e)
            return 1
    else:
        phrase = b""
        seed = int(args.seed, 0)
    # schema (P0-02 5.4, 8)
    sch = [(r[0], r[1], r[2], r[3] if r[4] != b"null" else None) for r in w["schema_rows"]]
    f.check(sch == [(b"index", b"sqlite_autoindex_biome_1", b"biome", None),
                    (b"index", b"sqlite_autoindex_layer_1", b"layer", None),
                    (b"table", b"biome", b"biome", DDL_BIOME),
                    (b"table", b"climate", b"climate", DDL_CLIMATE),
                    (b"table", b"layer", b"layer", DDL_LAYER),
                    (b"table", b"meta", b"meta", DDL_META),
                    (b"table", b"pass", b"pass", DDL_PASS),
                    (b"table", b"region", b"region", DDL_REGION),
                    (b"table", b"region_biome", b"region_biome", DDL_REGION_BIOME),
                    (b"table", b"region_border", b"region_border", DDL_REGION_BORDER)],
            "sqlite_schema rows: %r" % (sch,))
    # meta (P0-02 5.5, 8)
    expected_meta = [
        (b"content_hash", args.content_hash.encode()),
        (b"format", b"the-unmapped-world"),
        (b"generator_version", args.generator_version.encode()),
        (b"param.map_size", str(args.map_size).encode()),
        (b"schema_version", str(SCHEMA_VERSION).encode()),
        (b"seed", str(seed).encode()),
        (b"seed_phrase", phrase),
        (b"stages", b"test_pattern" if args.test_pattern else b""),
    ]
    meta = [(k[1], v[1]) for k, v in w["tables"][b"meta"][1]]
    f.check(meta == expected_meta, "meta rows: %r" % (meta,))
    f.check(all(k[0] == "text" and v[0] == "text" for k, v in w["tables"][b"meta"][1]), "meta values are TEXT")
    f.check(list(w["tables"]) == [b"biome", b"climate", b"layer", b"meta", b"pass", b"region", b"region_biome",
                                  b"region_border"],
            "tables are biome, climate, layer, meta, pass, region, region_biome, region_border")
    for empty in (b"pass", b"biome", b"climate", b"region", b"region_biome", b"region_border"):
        f.check(w["tables"][empty][1] == [], "table %s has no rows in a P0 world" % empty.decode())
    # layers
    layers = w["layers"]
    if not args.test_pattern:
        f.check(layers == [], "no layers in an empty world")
        f.check(len(data) == 45056, "empty world is 45056 bytes (P0-02 amendment 17; 11 pages with tables "
                "pass, biome, climate, region, region_border, region_biome and the index of biome.key)")
    else:
        f.check([(L["id"], L["name"], L["format"], L["width"], L["height"]) for L in layers] == [
            (1, b"test.ramp", b"u8", 256, 256), (2, b"test.hash", b"u8", 256, 256),
            (3, b"test.noise", b"i16", 256, 256)], "layer rows")
        for L in layers:
            f.check(TYPES[L["tcode"]][0].encode() == L["format"] and (L["bw"], L["bh"]) == (L["width"], L["height"]),
                    "blob header matches row for %r" % L["name"])
            f.check(L["codec"] == 1, "codec 1 for %r" % L["name"])
            f.check(L["blob"][24:] == encode_delta_varint(L["values"]), "canonical encoding of %r" % L["name"])
        if len(layers) == 3:
            k_hash = ref.stream_key(seed, "test.hash")
            k_noise = ref.stream_key(seed, "test.noise")
            ramp = [(x + 2 * y) % 256 for y in range(256) for x in range(256)]
            hsh = [ref.hash_at(k_hash, x, y) >> 56 for y in range(256) for x in range(256)]
            noise = []
            for y in range(256):
                fy = ref.fx_from_int(y) + ref.HALF
                for x in range(256):
                    n = ref.fbm2(k_noise, ref.fx_from_int(x) + ref.HALF, fy, 5, ref.fx_from_raw(134217728),
                                 ref.fx_from_int(2), ref.HALF)
                    noise.append(ref.mul_div(n, 32767, 1 << 32))
            for L, exp in zip(layers, (ramp, hsh, noise)):
                bad = sum(1 for a, b in zip(L["values"], exp) if a != b)
                f.check(bad == 0, "%r: %d cells differ from the P0-04 5.2 formula" % (L["name"], bad))
    if args.detail:
        got = open(args.detail, "rb").read()
        f.check(got == w["detail"], "hash --detail output differs:\n%s\nexpected:\n%s" % (got, w["detail"]))
    if args.report:
        got = open(args.report, "rb").read()
        f.check(got == w["report"], "report output differs:\n%s\nexpected:\n%s" % (got, w["report"]))
    sys.stdout.write(w["detail"].decode())
    print("header change_counter=%d pages=%d schema_cookie=%d sqlite_version=%d size=%d" % (
        hdr["change_counter"], hdr["pages"], hdr["schema_cookie"], hdr["sqlite_version"], len(data)))
    for L in layers:
        print("render_rgb %s %s" % (L["name"].decode(), hashlib.sha256(gray_rgb(L["values"])).hexdigest()))
    for item in f.items:
        print("FAIL: " + item)
    print("OK" if not f.items else "FAILED (%d)" % len(f.items))
    return 0 if not f.items else 1


def stats(args):
    for path in args.worlds:
        data = open(path, "rb").read()
        conn = sqlite3.connect("file:" + os.path.abspath(path) + "?mode=ro&immutable=1", uri=True)
        digest = hashlib.sha256(data).hexdigest()
        for name, blob in conn.execute("SELECT name, data FROM layer ORDER BY id"):
            vals = decode_blob(blob)[4]
            print(path, digest, name, min(vals), max(vals), sum(vals), len(vals))
        conn.close()
    return 0


def main(argv):
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = p.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("check")
    c.add_argument("world")
    g = c.add_mutually_exclusive_group(required=True)
    g.add_argument("--seed")
    g.add_argument("--phrase")
    c.add_argument("--map-size", type=int, default=1024)
    c.add_argument("--test-pattern", action="store_true")
    c.add_argument("--detail")
    c.add_argument("--report")
    c.add_argument("--generator-version", default=GENERATOR_VERSION)
    c.add_argument("--content-hash", default=EMPTY_SHA)
    s = sub.add_parser("stats")
    s.add_argument("worlds", nargs="+")
    a = p.parse_args(argv[1:])
    return check_world(a) if a.cmd == "check" else stats(a)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
