#!/bin/sh
# Fetch the pinned Godot editor and verify it (gdext/README.md, CLAUDE.md
# "Godot extension and viewer"). The build never downloads anything (hard
# rule 5); this script, run by hand or by a CI workflow step, is the only
# thing that does, and it checks every file against the SHA-512 digests
# pinned next to it in godot-<version>.sha512.
#
#   tools/ci/fetch-godot.sh [--tools-dir DIR] [--platform linux|windows]
#   tools/ci/fetch-godot.sh --version
#
# Installs into <tools dir>/godot-<version>/, the folder gdext/CMakeLists.txt
# searches. The default tools dir is the one CMake looks in:
# $HOME/.cache/the-unmapped/tools on Linux,
# %LOCALAPPDATA%\the-unmapped\tools on Windows (Git Bash). It is not moved by
# UNMAPPED_BUILD_ROOT; for another folder pass --tools-dir and give CMake the
# printed path with -DUNMAPPED_GODOT=<path>.
#
# Linux:   Godot_v<version>_linux.x86_64
# Windows: Godot_v<version>_win64.exe and Godot_v<version>_win64_console.exe
#          (the console one is printed: it is the one the headless check runs)
#
# If every file is already installed and matches its digest, nothing is
# downloaded or written. If an installed file does not match, the script
# stops without touching it. A verified archive already in
# godot-<version>/dl/ is used instead of downloading it again; an archive
# this script downloads is deleted after extraction.
#
# Prints the editor's path (forward slashes) as the only line on standard
# output; progress goes to standard error.
# --version prints the pinned version, for CI cache keys.
#
# POSIX sh; runs under Git Bash on Windows. Needs curl, sha512sum (or
# shasum), and unzip or Python 3 to extract.
# Exit status: 0 success, 1 download or verification failed, 2 usage error.

set -u

VERSION=4.7.2-stable
RELEASE_URL=https://github.com/godotengine/godot/releases/download/$VERSION

me=fetch-godot

say() { echo "$me: $*" >&2; }
fail() { echo "$me: error: $*" >&2; exit 1; }
usage_error() { echo "$me: error: $*" >&2; sed -n '8,9p' "$0" | sed 's/^# *//' >&2; exit 2; }

script_dir=$(cd "$(dirname "$0")" && pwd -P) || exit 2
sums=$script_dir/godot-$VERSION.sha512
[ -f "$sums" ] || fail "pinned checksums $sums not found"

is_windows=no
case $(uname -s 2>/dev/null) in
    MINGW* | MSYS* | CYGWIN*) is_windows=yes ;;
esac

native_path() {
    if [ "$is_windows" = yes ] && command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    else
        printf '%s\n' "$1"
    fi
}

tools_dir=
platform=
while [ $# -gt 0 ]; do
    case $1 in
        --tools-dir) [ $# -ge 2 ] || usage_error "--tools-dir needs a value"; tools_dir=$2; shift 2 ;;
        --platform)  [ $# -ge 2 ] || usage_error "--platform needs a value"; platform=$2; shift 2 ;;
        --version)   echo "$VERSION"; exit 0 ;;
        --help | -h) sed -n '2,36p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 0 ;;
        *) usage_error "unknown argument $1" ;;
    esac
done

if [ -z "$platform" ]; then
    case $(uname -s 2>/dev/null) in
        Linux) platform=linux ;;
        MINGW* | MSYS* | CYGWIN*) platform=windows ;;
        *) fail "unsupported host $(uname -s); pass --platform linux or windows" ;;
    esac
fi
case $platform in
    linux)
        archive=Godot_v${VERSION}_linux.x86_64.zip
        members=Godot_v${VERSION}_linux.x86_64
        editor=Godot_v${VERSION}_linux.x86_64
        ;;
    windows)
        archive=Godot_v${VERSION}_win64.exe.zip
        members="Godot_v${VERSION}_win64.exe Godot_v${VERSION}_win64_console.exe"
        editor=Godot_v${VERSION}_win64_console.exe
        ;;
    *) usage_error "--platform must be linux or windows, not $platform" ;;
esac

if [ -z "$tools_dir" ]; then
    if [ "$is_windows" = yes ]; then
        [ -n "${LOCALAPPDATA:-}" ] || fail "LOCALAPPDATA is not set; pass --tools-dir"
        tools_dir=$(native_path "$LOCALAPPDATA")/the-unmapped/tools
    else
        [ -n "${HOME:-}" ] || fail "HOME is not set; pass --tools-dir"
        tools_dir=$HOME/.cache/the-unmapped/tools
    fi
fi
tools_dir=$(native_path "$tools_dir")
case $tools_dir in
    /* | [A-Za-z]:/*) ;;
    *) usage_error "--tools-dir must be an absolute path: $tools_dir" ;;
esac
# The editor's cache and the downloads must not land in the repository.
# Physical path of the nearest existing ancestor, plus the rest.
source_dir=$(cd "$script_dir/../.." && pwd -P) || exit 2
p=$tools_dir
rest=
while [ ! -d "$p" ]; do
    rest="/${p##*/}$rest"
    p=${p%/*}
    [ -n "$p" ] || p=/
done
case "$(cd "$p" && pwd -P)$rest/" in
    "$source_dir"/*) usage_error "refusing to install inside the source tree: $tools_dir" ;;
esac
mkdir -p "$tools_dir" || fail "cannot create $tools_dir"

install_dir=$tools_dir/godot-$VERSION

# Pinned digest of file name $1 (empty if not pinned).
pinned() {
    tr -d '\r' <"$sums" | awk -v n="$1" '$1 !~ /^#/ && $2 == n { print $1; exit }'
}

# SHA-512 of file $1.
digest() {
    if command -v sha512sum >/dev/null 2>&1; then
        sha512sum "$1" | awk '{ print $1 }' | tr -d '\\'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 512 "$1" | awk '{ print $1 }'
    else
        fail "neither sha512sum nor shasum is available"
    fi
}

# Succeeds if file $1 matches the pinned digest of name $2.
verified() {
    want=$(pinned "$2")
    [ -n "$want" ] || fail "no pinned digest for $2 in $sums"
    [ -f "$1" ] && [ "$(digest "$1")" = "$want" ]
}

# 1. Already installed?
installed=yes
for m in $members; do
    if [ -e "$install_dir/$m" ]; then
        verified "$install_dir/$m" "$m" ||
            fail "$install_dir/$m exists but does not match its pinned SHA-512 in $sums. Remove it (and the rest of $install_dir) and run again."
    else
        installed=no
    fi
done
if [ "$installed" = yes ]; then
    say "Godot $VERSION ($platform) already installed and verified in $install_dir"
    native_path "$install_dir/$editor"
    exit 0
fi

# 2. The archive: a verified copy in dl/, or a fresh download.
mkdir -p "$install_dir/dl" || fail "cannot create $install_dir/dl"
zip=$install_dir/dl/$archive
downloaded=no
if [ -f "$zip" ] && verified "$zip" "$archive"; then
    say "using the verified archive already in $zip"
else
    command -v curl >/dev/null 2>&1 || fail "curl is needed to download $archive"
    rm -f "$zip" "$zip.partial"
    say "downloading $RELEASE_URL/$archive"
    curl -fL --retry 3 --retry-delay 5 --connect-timeout 30 -sS -o "$zip.partial" "$RELEASE_URL/$archive" ||
        { rm -f "$zip.partial"; fail "download of $archive failed"; }
    if ! verified "$zip.partial" "$archive"; then
        got=$(digest "$zip.partial")
        rm -f "$zip.partial"
        fail "$archive does not match its pinned SHA-512 (got $got); nothing installed"
    fi
    mv -f "$zip.partial" "$zip" || fail "cannot move $zip.partial"
    downloaded=yes
    say "verified $archive (SHA-512 pinned in $sums)"
fi

# 3. Extract into a scratch folder, verify every file, then move into place.
stage=$install_dir/.extract.$$
rm -rf "$stage"
mkdir -p "$stage" || fail "cannot create $stage"
trap 'rm -rf "$stage"' EXIT

extracted=no
if command -v unzip >/dev/null 2>&1; then
    # shellcheck disable=SC2086
    unzip -q -o "$zip" $members -d "$stage" && extracted=yes
fi
if [ "$extracted" = no ]; then
    for py in python3 python; do
        if command -v "$py" >/dev/null 2>&1 &&
            "$py" -I -c 'import sys; sys.exit(sys.version_info[0] != 3)' 2>/dev/null; then
            "$py" -I -m zipfile -e "$(native_path "$zip")" "$(native_path "$stage")" && extracted=yes
            break
        fi
    done
fi
[ "$extracted" = yes ] || fail "cannot extract $zip (need unzip or Python 3)"

for m in $members; do
    verified "$stage/$m" "$m" || fail "$m extracted from $archive does not match its pinned SHA-512; nothing installed"
done
for m in $members; do
    chmod +x "$stage/$m" 2>/dev/null
    mv -f "$stage/$m" "$install_dir/$m" || fail "cannot move $m into $install_dir"
done
if [ "$downloaded" = yes ]; then
    rm -f "$zip"
    rmdir "$install_dir/dl" 2>/dev/null
fi
say "installed Godot $VERSION ($platform) in $install_dir"
native_path "$install_dir/$editor"
