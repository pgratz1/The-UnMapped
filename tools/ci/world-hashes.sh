#!/bin/sh
# Cross-compiler world hash check (spec P0-04 section 9 with amendment 16 (d);
# P1-00 section 14, X5).
#
# Generates the golden cases with one preset's worldgen-cli, saves each
# world's `hash --detail` and `report` text, checks them against the golden
# file, and compares the saved text of several presets. Used by continuous
# integration (.github/workflows/ci.yml) and by developers locally:
#
#   tools/ci/world-hashes.sh generate PRESET [--build-dir DIR] [--out-dir DIR]
#   tools/ci/world-hashes.sh expect   PRESET [--build-dir DIR] [--out-dir DIR] [--golden FILE] [--golden-geo FILE]
#   tools/ci/world-hashes.sh compare  PRESET PRESET... [--out-dir DIR]
#   tools/ci/world-hashes.sh local    PRESET PRESET... [--out-dir DIR]
#   tools/ci/world-hashes.sh cases
#
# generate  writes, for every case, into the output directory:
#             <preset>-<case>.db           the world file
#             <preset>-<case>.txt          worldgen-cli hash --db <file> --detail
#             <preset>-<case>.report.txt   worldgen-cli report --db <file>
#           A .txt file exists only if its command succeeded.
# expect    runs hash --expect for every case against its golden file: the
#           P0 cases against worldgen/tests/golden/p0-worlds.txt (--golden),
#           the geography cases against worldgen/tests/golden/p1-worlds.txt
#           (--golden-geo). A golden file that does not exist is reported
#           and its cases are skipped.
# compare   fails if any preset's .txt or .report.txt for any case differs
#           from the others, or is missing, and prints a diff.
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
# that the implemented stages allow, each list in case-name order (the order
# of its golden file). Arguments are given in run_generate below.
P0_CASES='empty-phrase empty-seed-0 empty-seed-0-512 empty-seed-1 empty-seed-42 empty-seed-max pattern-seed-0 pattern-seed-42'
GEO_CASES='geo-phrase-512 geo-seed-0-512 geo-seed-1-1024 geo-seed-42-512'
CASES="$P0_CASES $GEO_CASES"

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
        # P1-00 14 X5. Default stages (elevation; hydrology once implemented).
        geo-seed-0-512)   "$cli" generate --out "$2" --data "$data_dir" --seed 0 --map-size 512 ;;
        geo-seed-42-512)  "$cli" generate --out "$2" --data "$data_dir" --seed 42 --map-size 512 ;;
        geo-phrase-512)   "$cli" generate --out "$2" --data "$data_dir" --seed-phrase "The Unmapped" --map-size 512 ;;
        geo-seed-1-1024)  "$cli" generate --out "$2" --data "$data_dir" --seed 1 ;;
        *) echo "world-hashes: internal error: unknown case $1" >&2; return 1 ;;
    esac
}

me=world-hashes

usage() {
    sed -n '10,14p' "$0" | sed 's/^# *//' >&2
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
while [ $# -gt 0 ]; do
    case $1 in
        --build-dir) [ $# -ge 2 ] || die "--build-dir needs a value"; build_dir=$2; shift 2 ;;
        --out-dir)   [ $# -ge 2 ] || die "--out-dir needs a value"; out_dir=$2; shift 2 ;;
        --golden)    [ $# -ge 2 ] || die "--golden needs a value"; golden=$2; shift 2 ;;
        --golden-geo) [ $# -ge 2 ] || die "--golden-geo needs a value"; golden_geo=$2; shift 2 ;;
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
        rm -f "$base.db" "$base.txt" "$base.report.txt" "$base.txt.partial" "$base.report.txt.partial"
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
        [ -f "$base.txt" ] && echo "$preset $c $(sed -n '1s/^file //p' "$base.txt")"
    done
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
    return "$failed"
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
        for kind in txt report.txt; do
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
            fi
            # shellcheck disable=SC2086
            group_count=$(echo $leaders | wc -w | tr -d ' ')
            if [ "$group_count" -gt 1 ]; then
                failed=1
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
                    diff -u "$out_dir/$first-$c.$kind" "$out_dir/$l-$c.$kind" >&2
                done
            elif [ "$group_count" -eq 1 ] && [ -z "$missing" ] && [ "$kind" = txt ]; then
                # shellcheck disable=SC2086
                set -- $leaders
                echo "$me: same $c $(sed -n '1s/^file //p' "$out_dir/$1-$c.txt") ($(echo $pairs | wc -w | tr -d ' ') of $(echo $all_presets | wc -w | tr -d ' ') presets present)"
                set -- $all_presets
            fi
        done
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
