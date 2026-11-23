#!/bin/sh
# Cross-compiler world hash check (spec P0-04 section 9 with amendment 16 (d);
# P1-00 section 14, X5; P3-03 section 13; P3-07 amendment 18.6).
#
# Generates the golden cases with one preset's worldgen-cli, saves each
# world's `hash --detail` and `report` text (and, for a world with history,
# its history report at both levels), checks them against the golden
# file, and compares the saved text of several presets. Used by continuous
# integration (.github/workflows/ci.yml) and by developers locally:
#
#   tools/ci/world-hashes.sh generate PRESET [--build-dir DIR] [--out-dir DIR]
#   tools/ci/world-hashes.sh expect   PRESET [--build-dir DIR] [--out-dir DIR] [--golden FILE] [--golden-geo FILE]
#                                             [--golden-hist FILE]
#   tools/ci/world-hashes.sh compare  PRESET PRESET... [--out-dir DIR]
#   tools/ci/world-hashes.sh local    PRESET PRESET... [--out-dir DIR]
#   tools/ci/world-hashes.sh cases
#
# generate  writes, for every case, into the output directory:
#             <preset>-<case>.db           the world file
#             <preset>-<case>.txt          worldgen-cli hash --db <file> --detail
#             <preset>-<case>.report.txt   worldgen-cli report --db <file>
#           and, for every case of HISTORY_CASES (P3-07 18.6):
#             <preset>-<case>.history.summary.txt
#                 worldgen-cli report --db <file> --history --level summary
#             <preset>-<case>.history.full.txt
#                 worldgen-cli report --db <file> --history --level full
#             <preset>-<case>.territory.txt
#                 P4-01 13.6: lines "<year> <sha256 of the PNG>" for
#                 worldgen-cli render --composite territory at year 0,
#                 floor(E/2) and the history's end year E (the default year),
#                 ascending, a year that coincides with the one before once.
#             <preset>-<case>.markers.txt
#                 P4-04 11: lines "<year> <sha256 of the PNG>" for
#                 worldgen-cli render --composite territory --year <year>
#                 --markers --faded at the years of the territory file.
#                 The PNGs are written to a scratch directory and removed.
#           A .txt file exists only if its command succeeded.
# expect    runs hash --expect for every case against its golden file: the
#           P0 cases against worldgen/tests/golden/p0-worlds.txt (--golden),
#           the geography cases against worldgen/tests/golden/p1-worlds.txt
#           (--golden-geo), the history cases against
#           worldgen/tests/golden/p3-worlds.txt (--golden-hist). A golden
#           file that does not exist is reported and its cases are skipped.
# compare   fails if any preset's .txt, .report.txt or (for a case with
#           history) .history.summary.txt, .history.full.txt, .territory.txt
#           or .markers.txt
#           for any case
#           differs from the others, byte for byte, or is missing, and prints
#           a diff -u of at most DIFF_LINES lines.
# local     generate for each preset (each must already be built), then
#           compare them.
# cases     prints the case names.
#
# Defaults follow CLAUDE.md "Build and test". Build root: UNMAPPED_BUILD_ROOT
# if set, else %LOCALAPPDATA%\the-unmapped on Windows, else
# $HOME/.cache/the-unmapped. With UNMAPPED_BUILD_ROOT set, the build tree is
# <root>/build/<preset> and the output directory <root>/out/world-hashes;
# otherwise <root>/build/<source dir name>/<preset> and
# <root>/out/<source dir name>/world-hashes. Nothing is written in the source
# tree; an output directory inside it is refused.
#
# POSIX sh; runs under Git Bash on Windows (worldgen-cli.exe, C:/ paths).
# Exit status: 0 success, 1 a check failed, 2 usage error.

set -u

# The case table of P0-04 section 9 (with amendment 16 (d): the empty-*
# cases pass --until none) and the geography cases of P1-00 section 14 X5
# that the implemented stages allow, and the history cases of P3-03 section
# 13 and P3-04 section 15, each list in case-name order (the order of its golden file). Arguments
# are given in run_generate below.
P0_CASES='empty-phrase empty-seed-0 empty-seed-0-512 empty-seed-1 empty-seed-42 empty-seed-max pattern-seed-0 pattern-seed-42'
GEO_CASES='geo-phrase-512 geo-seed-0-512 geo-seed-1-1024 geo-seed-42-512'
HIST_CASES='hist-dense-1024 hist-races-elf-orc-1024 hist-span-2000-512'
CASES="$P0_CASES $GEO_CASES $HIST_CASES"

# The cases whose world has a history (P3-07 18.6: every case except empty-*
# and pattern-*; the geography cases run every implemented stage). Only these
# get history reports; for the others report --history exits 3 (no history),
# which generate never runs and compare never expects.
HISTORY_CASES="$GEO_CASES $HIST_CASES"

# The kinds of saved text compared per case (file suffixes).
TEXT_KINDS='txt report.txt'
HISTORY_KINDS='history.summary.txt history.full.txt territory.txt markers.txt'

# Longest diff -u that compare prints for one differing file; the rest is
# counted, not printed (a full history report is tens of thousands of lines).
DIFF_LINES=200

has_history() {
    case " $HISTORY_CASES " in
        *" $1 "*) return 0 ;;
        *) return 1 ;;
    esac
}

# Run "worldgen-cli generate" for case $1 writing world file $2. Every case
# reads the shipped data of this checkout (--data, so that UNMAPPED_DATA_DIR
# in the environment cannot change a golden world; P1-00 6.4).
run_generate() {
    case $1 in
        empty-seed-0)     "$cli" generate --out "$2" --data "$data_dir" --until none --seed 0 ;;
        empty-seed-1)     "$cli" generate --out "$2" --data "$data_dir" --until none --seed 1 ;;
        empty-seed-42)    "$cli" generate --out "$2" --data "$data_dir" --until none --seed 42 ;;
        empty-seed-max)   "$cli" generate --out "$2" --data "$data_dir" --until none --seed 18446744073709551615 ;;
        empty-seed-0-512) "$cli" generate --out "$2" --data "$data_dir" --until none --seed 0 --map-size 512 ;;
        empty-phrase)     "$cli" generate --out "$2" --data "$data_dir" --until none --seed-phrase "The Unmapped" ;;
        pattern-seed-0)   "$cli" generate --out "$2" --data "$data_dir" --seed 0 --test-pattern ;;
        pattern-seed-42)  "$cli" generate --out "$2" --data "$data_dir" --seed 42 --test-pattern ;;
        # P1-00 14 X5. Default stages: every implemented stage (the geography
        # stages, chronology and history).
        geo-seed-0-512)   "$cli" generate --out "$2" --data "$data_dir" --seed 0 --map-size 512 ;;
        geo-seed-42-512)  "$cli" generate --out "$2" --data "$data_dir" --seed 42 --map-size 512 ;;
        geo-phrase-512)   "$cli" generate --out "$2" --data "$data_dir" --seed-phrase "The Unmapped" --map-size 512 ;;
        geo-seed-1-1024)  "$cli" generate --out "$2" --data "$data_dir" --seed 1 ;;
        # P3-03 13: the span and the race subset change chronology bytes.
        hist-span-2000-512)      "$cli" generate --out "$2" --data "$data_dir" --seed 7 --map-size 512 --history-span 2000 ;;
        hist-races-elf-orc-1024) "$cli" generate --out "$2" --data "$data_dir" --seed 3 --races elf,orc ;;
        # P3-04 15: the detail level changes history bytes.
        hist-dense-1024)         "$cli" generate --out "$2" --data "$data_dir" --seed 11 --history-detail dense ;;
        *) echo "world-hashes: internal error: unknown case $1" >&2; return 1 ;;
    esac
}

me=world-hashes

usage() {
    sed -n '11,16p' "$0" | sed 's/^# *//' >&2
    exit 2
}

die() {
    echo "$me: error: $*" >&2
    exit 2
}

# Sanitizer builds: make undefined behaviour and leaks fail the run, as the
# linux-clang-asan test preset does. No effect on other builds.
: "${ASAN_OPTIONS:=detect_leaks=1:abort_on_error=1}"
: "${UBSAN_OPTIONS:=print_stacktrace=1:halt_on_error=1}"
export ASAN_OPTIONS UBSAN_OPTIONS

is_windows=no
case $(uname -s 2>/dev/null) in
    MINGW* | MSYS* | CYGWIN*) is_windows=yes ;;
esac

# Forward-slash form of a path (C:\a\b -> C:/a/b under Git Bash), which both
# the shell and native Windows programs accept.
native_path() {
    if [ "$is_windows" = yes ] && command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    else
        printf '%s\n' "$1"
    fi
}

is_absolute() {
    case $1 in
        /* | [A-Za-z]:/* | [A-Za-z]:\\*) return 0 ;;
        *) return 1 ;;
    esac
}

# Physical path of a path that may not exist yet: the nearest existing
# ancestor resolved with pwd -P, plus the rest.
physical_path() {
    p=$1
    rest=
    while [ ! -d "$p" ]; do
        rest="/${p##*/}$rest"
        p=${p%/*}
        [ -n "$p" ] || p=/
    done
    printf '%s%s\n' "$(cd "$p" && pwd -P)" "$rest"
}

script_dir=$(cd "$(dirname "$0")" && pwd -P) || exit 2
source_dir=$(cd "$script_dir/../.." && pwd -P) || exit 2
source_name=${source_dir##*/}
data_dir=$(native_path "$source_dir/game/data")

[ $# -ge 1 ] || usage
command=$1
shift

presets=
build_dir=
out_dir=
golden=
golden_geo=
golden_hist=
while [ $# -gt 0 ]; do
    case $1 in
        --build-dir) [ $# -ge 2 ] || die "--build-dir needs a value"; build_dir=$2; shift 2 ;;
        --out-dir)   [ $# -ge 2 ] || die "--out-dir needs a value"; out_dir=$2; shift 2 ;;
        --golden)    [ $# -ge 2 ] || die "--golden needs a value"; golden=$2; shift 2 ;;
        --golden-geo) [ $# -ge 2 ] || die "--golden-geo needs a value"; golden_geo=$2; shift 2 ;;
        --golden-hist) [ $# -ge 2 ] || die "--golden-hist needs a value"; golden_hist=$2; shift 2 ;;
        --help | -h) usage ;;
        -*) die "unknown option $1" ;;
        *) presets="$presets $1"; shift ;;
    esac
done
set -- $presets
preset_count=$#

case $command in
    cases) printf '%s\n' $CASES; exit 0 ;;
    generate | expect)
        [ "$preset_count" -eq 1 ] || die "$command takes exactly one preset" ;;
    compare | local)
        [ "$preset_count" -ge 2 ] || die "$command takes two or more presets"
        [ -z "$build_dir" ] || die "--build-dir is for generate and expect; local uses each preset's default build tree" ;;
    *) usage ;;
esac

# Build root, build tree and output directory (CLAUDE.md "Build and test").
if [ -n "${UNMAPPED_BUILD_ROOT:-}" ]; then
    build_root=$(native_path "$UNMAPPED_BUILD_ROOT")
    tree_part=
elif [ "$is_windows" = yes ] && [ -n "${LOCALAPPDATA:-}" ]; then
    build_root=$(native_path "$LOCALAPPDATA")/the-unmapped
    tree_part=/$source_name
elif [ "$is_windows" = no ] && [ -n "${HOME:-}" ]; then
    build_root=$HOME/.cache/the-unmapped
    tree_part=/$source_name
else
    die "no build root: set UNMAPPED_BUILD_ROOT"
fi
is_absolute "$build_root" || die "the build root must be an absolute path: $build_root"

if [ -n "$out_dir" ]; then
    out_dir=$(native_path "$out_dir")
else
    out_dir=$build_root/out$tree_part/world-hashes
fi
is_absolute "$out_dir" || die "--out-dir must be an absolute path: $out_dir"
case "$(physical_path "$out_dir")/" in
    "$source_dir"/*) die "refusing to write inside the source tree: $out_dir" ;;
esac

# Sets $cli to preset $1's worldgen-cli in build tree $2 (or the default).
find_cli() {
    dir=${2:-$build_root/build$tree_part/$1}
    dir=$(native_path "$dir")
    for name in worldgen-cli.exe worldgen-cli; do
        if [ -f "$dir/tools/worldgen-cli/$name" ]; then
            cli=$dir/tools/worldgen-cli/$name
            return 0
        fi
    done
    echo "$me: error: no worldgen-cli for preset $1 in $dir/tools/worldgen-cli/ (build the preset first)" >&2
    return 1
}

# SHA-256 of file $1, as 64 hex digits (sha256sum, else shasum -a 256).
file_sha256() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | cut -d' ' -f1
    else
        shasum -a 256 "$1" | cut -d' ' -f1
    fi
}

# Renders the territory image of world $2 at year 0, floor(E/2) and the default year E
# into scratch directory $3 and writes "<year> <sha256>" lines to file $1.
# E is read from the "year E" line that render --verbose prints on stderr.
territory_hashes() {
    t_target=$1
    t_db=$2
    t_dir=$3
    mkdir -p "$t_dir" || return 1
    rm -f "$t_dir"/*.png "$t_target.partial"
    t_end=$("$cli" render --db "$t_db" --composite territory --verbose --out "$t_dir/ye.png" 2>&1 >/dev/null |
        sed -n 's/^.*year \([0-9][0-9]*\)\r*$/\1/p' | tail -n 1)
    [ -n "$t_end" ] && [ -f "$t_dir/ye.png" ] || return 1
    t_mid=$((t_end / 2))
    : >"$t_target.partial" || return 1
    for t_year in 0 $t_mid $t_end; do
        # ascending; a year that coincides with the previous one is written once
        [ "$t_year" = "${t_prev:-}" ] && continue
        t_prev=$t_year
        if [ "$t_year" = "$t_end" ]; then
            t_png=$t_dir/ye.png
        else
            t_png=$t_dir/y$t_year.png
            "$cli" render --db "$t_db" --composite territory --year "$t_year" --out "$t_png" >/dev/null || return 1
        fi
        echo "$t_year $(file_sha256 "$t_png")" >>"$t_target.partial" || return 1
    done
    t_prev=
    mv -f "$t_target.partial" "$t_target"
    rm -f "$t_dir"/*.png
}

# Renders the territory image with the markers (render --markers --faded) of
# world $2 at each year of territory file $3 (P4-04 11) into scratch
# directory $4 and writes "<year> <sha256>" lines to file $1.
markers_hashes() {
    m_target=$1
    m_db=$2
    m_years=$3
    m_dir=$4
    mkdir -p "$m_dir" || return 1
    rm -f "$m_dir"/*.png "$m_target.partial"
    [ -f "$m_years" ] || return 1
    : >"$m_target.partial" || return 1
    for m_year in $(cut -d' ' -f1 "$m_years"); do
        m_png=$m_dir/m$m_year.png
        "$cli" render --db "$m_db" --composite territory --year "$m_year" --markers --faded --out "$m_png" \
            >/dev/null || return 1
        echo "$m_year $(file_sha256 "$m_png")" >>"$m_target.partial" || return 1
    done
    mv -f "$m_target.partial" "$m_target"
    rm -f "$m_dir"/*.png
}

# Run a command with stdout to file $1, keeping the file only on success.
capture() {
    target=$1
    shift
    if "$@" >"$target.partial"; then
        mv -f "$target.partial" "$target"
    else
        status=$?
        rm -f "$target.partial"
        return "$status"
    fi
}

generate() {
    preset=$1
    find_cli "$preset" "$build_dir" || return 1
    mkdir -p "$out_dir" || return 1
    echo "$me: preset $preset: $cli"
    "$cli" --version || return 1
    failed=0
    for c in $CASES; do
        base=$out_dir/$preset-$c
        for kind in $TEXT_KINDS $HISTORY_KINDS; do
            rm -f "$base.$kind" "$base.$kind.partial"
        done
        rm -f "$base.db"
        if ! run_generate "$c" "$base.db" >/dev/null; then
            echo "$me: FAIL $preset $c: generate failed" >&2
            failed=1
            continue
        fi
        if ! capture "$base.txt" "$cli" hash --db "$base.db" --detail; then
            echo "$me: FAIL $preset $c: hash --detail failed" >&2
            failed=1
        fi
        if ! capture "$base.report.txt" "$cli" report --db "$base.db"; then
            echo "$me: FAIL $preset $c: report failed" >&2
            failed=1
        fi
        if has_history "$c"; then
            for level in summary full; do
                if ! capture "$base.history.$level.txt" "$cli" report --db "$base.db" --history --level "$level"; then
                    echo "$me: FAIL $preset $c: report --history --level $level failed" >&2
                    failed=1
                fi
            done
            if ! territory_hashes "$base.territory.txt" "$base.db" "$out_dir/png-$preset"; then
                echo "$me: FAIL $preset $c: render --composite territory failed" >&2
                failed=1
            elif ! markers_hashes "$base.markers.txt" "$base.db" "$base.territory.txt" "$out_dir/png-$preset"; then
                echo "$me: FAIL $preset $c: render --composite territory --markers failed" >&2
                failed=1
            fi
        fi
        [ -f "$base.txt" ] && echo "$preset $c $(sed -n '1s/^file //p' "$base.txt")"
    done
    rmdir "$out_dir/png-$preset" 2>/dev/null
    [ "$failed" -eq 0 ] && echo "$me: preset $preset: wrote $(echo $CASES | wc -w | tr -d ' ') cases to $out_dir"
    return "$failed"
}

# Checks the cases $3 (a space-separated list) of preset $1 against golden
# file $2. Returns 0 when the file does not exist (reported, cases skipped).
expect_file() {
    preset=$1
    file=$2
    file_cases=$3
    if [ ! -f "$file" ]; then
        echo "$me: NOTE: no golden file at $file; golden hashes of$(printf ' %s' $file_cases) are not checked yet."
        if [ "${GITHUB_ACTIONS:-}" = true ]; then
            echo "::warning title=No golden world hashes::${file#"$source_dir"/} does not exist yet; hash --expect was not run for its cases."
        fi
        return 0
    fi
    if grep -q "$(printf '\r')" "$file"; then
        echo "$me: FAIL: $file has CR line endings" >&2
        return 1
    fi
    file_failed=0
    # Every golden line must name a case of this file.
    while read -r name hex rest; do
        case " $file_cases " in
            *" $name "*) ;;
            *) echo "$me: FAIL: $file: unknown case '$name'" >&2; file_failed=1 ;;
        esac
        if [ -z "${hex:-}" ] || [ -n "${rest:-}" ]; then
            echo "$me: FAIL: $file: malformed line for '$name'" >&2
            file_failed=1
        fi
    done <"$file"
    for c in $file_cases; do
        lines=$(grep -c "^$c " "$file")
        if [ "$lines" -ne 1 ]; then
            echo "$me: FAIL $preset $c: expected one line in $file, found $lines" >&2
            file_failed=1
            continue
        fi
        hex=$(sed -n "s/^$c \([^ ]*\)\$/\1/p" "$file")
        db=$out_dir/$preset-$c.db
        if [ ! -f "$db" ]; then
            echo "$me: FAIL $preset $c: $db missing (run generate first)" >&2
            file_failed=1
            continue
        fi
        "$cli" hash --db "$db" --expect "$hex" >/dev/null
        status=$?
        case $status in
            0) echo "$me: ok   $preset $c $hex" ;;
            5) echo "$me: FAIL $preset $c: hash differs from the golden value (exit 5)" >&2; file_failed=1 ;;
            *) echo "$me: FAIL $preset $c: hash --expect exited $status" >&2; file_failed=1 ;;
        esac
    done
    return "$file_failed"
}

expect() {
    preset=$1
    find_cli "$preset" "$build_dir" || return 1
    failed=0
    expect_file "$preset" "${golden:-$source_dir/worldgen/tests/golden/p0-worlds.txt}" "$P0_CASES" || failed=1
    expect_file "$preset" "${golden_geo:-$source_dir/worldgen/tests/golden/p1-worlds.txt}" "$GEO_CASES" || failed=1
    expect_file "$preset" "${golden_hist:-$source_dir/worldgen/tests/golden/p3-worlds.txt}" "$HIST_CASES" || failed=1
    return "$failed"
}

# diff -u of files $1 and $2, at most DIFF_LINES lines, then a count of the
# lines left out. Files are compared as bytes (cmp); a file with CR line ends
# shows up here as a difference, never normalised.
bounded_diff() {
    total=$(diff -u "$1" "$2" | wc -l | tr -d ' ')
    diff -u "$1" "$2" | head -n "$DIFF_LINES"
    if [ "$total" -gt "$DIFF_LINES" ]; then
        echo "  ... diff truncated: $total lines, first $DIFF_LINES shown"
    fi
}

compare() {
    failed=0
    differing=
    absent=
    for p in "$@"; do
        found=no
        for c in $CASES; do
            [ -f "$out_dir/$p-$c.txt" ] && found=yes
        done
        if [ "$found" = no ]; then
            echo "$me: FAIL: no results at all for preset $p in $out_dir (artifact missing: did its job fail?)" >&2
            absent="$absent $p"
            failed=1
        fi
    done
    for c in $CASES; do
        kinds=$TEXT_KINDS
        has_history "$c" && kinds="$kinds $HISTORY_KINDS"
        # "same" is printed once per case, after every kind agreed.
        case_ok=yes
        same_line=
        for kind in $kinds; do
            # Group the presets by identical content. Each group is named
            # by its first preset (its leader); pairs holds "preset=leader".
            leaders=
            pairs=
            missing=
            for p in "$@"; do
                f=$out_dir/$p-$c.$kind
                if [ ! -f "$f" ]; then
                    case " $absent " in *" $p "*) ;; *) missing="$missing $p" ;; esac
                    continue
                fi
                leader=
                for l in $leaders; do
                    if cmp -s "$out_dir/$l-$c.$kind" "$f"; then
                        leader=$l
                        break
                    fi
                done
                if [ -z "$leader" ]; then
                    leader=$p
                    leaders="$leaders $p"
                fi
                pairs="$pairs $p=$leader"
            done
            if [ -n "$missing" ]; then
                echo "$me: FAIL $c ($kind): missing for:$missing" >&2
                failed=1
                case_ok=no
            fi
            # shellcheck disable=SC2086
            group_count=$(echo $leaders | wc -w | tr -d ' ')
            if [ "$group_count" -gt 1 ]; then
                failed=1
                case_ok=no
                case " $differing " in *" $c "*) ;; *) differing="$differing $c" ;; esac
                echo "$me: FAIL $c ($kind): $group_count different outputs" >&2
                for l in $leaders; do
                    members=
                    for pair in $pairs; do
                        [ "${pair#*=}" = "$l" ] && members="$members ${pair%%=*}"
                    done
                    echo "  variant $l:$members" >&2
                done
                first=
                for l in $leaders; do
                    if [ -z "$first" ]; then
                        first=$l
                        continue
                    fi
                    echo "  diff -u $first-$c.$kind $l-$c.$kind" >&2
                    bounded_diff "$out_dir/$first-$c.$kind" "$out_dir/$l-$c.$kind" >&2
                done
            elif [ "$group_count" -eq 1 ] && [ -z "$missing" ] && [ "$kind" = txt ]; then
                # shellcheck disable=SC2086
                set -- $leaders
                same_line="$me: same $c $(sed -n '1s/^file //p' "$out_dir/$1-$c.txt") ($(echo $pairs | wc -w | tr -d ' ') of $(echo $all_presets | wc -w | tr -d ' ') presets present; $(echo $kinds | wc -w | tr -d ' ') files compared)"
                set -- $all_presets
            fi
        done
        if [ "$case_ok" = yes ] && [ -n "$same_line" ]; then
            echo "$same_line"
        fi
    done
    if [ "$failed" -eq 0 ]; then
        echo "$me: OK: all $(echo $CASES | wc -w | tr -d ' ') cases identical across $# presets:$all_presets"
    else
        echo "$me: FAILED: outputs differ or are missing.${differing:+ Differing cases:$differing}" >&2
    fi
    return "$failed"
}

all_presets=$presets
case $command in
    generate) generate "$1"; exit $? ;;
    expect)   expect "$1"; exit $? ;;
    compare)  compare "$@"; exit $? ;;
    local)
        failed=0
        for p in "$@"; do
            generate "$p" || failed=1
        done
        set -- $all_presets
        compare "$@" || failed=1
        exit "$failed"
        ;;
esac
