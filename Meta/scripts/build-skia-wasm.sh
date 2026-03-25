#!/usr/bin/env bash
# build-skia-wasm.sh — Build a CPU-only Skia static library for Emscripten.
#
# Outputs: $SKIA_OUT_DIR/libskia.a  +  $SKIA_OUT_DIR/include/
#
# Usage:
#   ./Meta/scripts/build-skia-wasm.sh [--out <dir>]
#
#   --out <dir>   Where to write libskia.a and include/ headers.
#                 Defaults to  Build/skia-wasm  relative to the repo root.
#
# Prerequisites:
#   - EMSDK env var set (or emsdk sourced so emcc is in PATH)
#   - python3
#   - git
#   - GN binary ships inside Skia (fetched automatically by git-sync-deps)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
# GN binary is bundled inside Skia — only available after clone + sync-deps
GN="$REPO_ROOT/third_party/skia/bin/gn"

# ── Parse args ──────────────────────────────────────────────────────────────
SKIA_OUT_DIR="$REPO_ROOT/Build/skia-wasm"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --out) SKIA_OUT_DIR="$2"; shift 2 ;;
        *) echo "Unknown arg: $1" >&2; exit 1 ;;
    esac
done

# ── Emscripten ───────────────────────────────────────────────────────────────
if ! command -v emcc &>/dev/null; then
    if [[ -n "${EMSDK:-}" ]]; then
        # shellcheck source=/dev/null
        source "$EMSDK/emsdk_env.sh" --build=Release
    else
        echo "error: emcc not found. Set EMSDK to your emsdk root or source emsdk_env.sh." >&2
        exit 1
    fi
fi
EMCC_BIN="$(command -v emcc)"
EMROOT="$(dirname "$EMCC_BIN")"
echo ">>> Using emcc: $EMCC_BIN ($(emcc --version 2>&1 | head -1))"

# ── Skia source ──────────────────────────────────────────────────────────────
SKIA_SRC_DIR="$REPO_ROOT/third_party/skia"

if [[ ! -d "$SKIA_SRC_DIR/.git" ]]; then
    echo ">>> Cloning Skia into third_party/skia ..."
    # chrome/m136 is a recent stable milestone; pin this to match vcpkg baseline
    # when vcpkg gains a wasm32-emscripten Skia port.
    git clone --depth=1 --branch chrome/m136 \
        https://skia.googlesource.com/skia.git \
        "$SKIA_SRC_DIR"
else
    echo ">>> Skia source found at $SKIA_SRC_DIR"
fi

cd "$SKIA_SRC_DIR"

# Sync third-party Skia dependencies (freetype, harfbuzz embedded in Skia, etc.)
echo ">>> Syncing Skia third-party deps (python3 tools/git-sync-deps) ..."
python3 tools/git-sync-deps

# ── GN build args ────────────────────────────────────────────────────────────
# CPU-only Skia: no WebGL, no Ganesh GPU backend, no Vulkan/Metal.
# Keep FreeType + HarfBuzz for text rendering (used by LibGfx).
# Codecs (JPEG/PNG/WebP/avif) are handled separately by LibGfx's own deps
# via vcpkg — disable them here to keep libskia.a small.
BUILD_DIR="$SKIA_SRC_DIR/out/wasm-cpu"
mkdir -p "$BUILD_DIR"

"$GN" gen "$BUILD_DIR" --args="
    target_os = \"wasm\"
    target_cpu = \"wasm\"
    is_official_build = true
    cc = \"$EMROOT/emcc\"
    cxx = \"$EMROOT/em++\"
    ar = \"$EMROOT/emar\"
    extra_cflags = [\"-fno-rtti\", \"-I${SKIA_SRC_DIR}/third_party/externals/zlib\"]

    skia_enable_ganesh = false
    skia_enable_graphite = false
    skia_use_gl = false
    skia_use_webgl = false
    skia_use_vulkan = false
    skia_use_metal = false
    skia_use_dawn = false
    skia_use_angle = false

    skia_use_freetype = true
    skia_use_harfbuzz = true
    skia_use_system_freetype2 = false
    skia_use_icu = false
    skia_use_fontconfig = false
    skia_use_x11 = false

    skia_use_zlib = true
    skia_use_system_zlib = false
    skia_use_libwebp_encode = false
    skia_use_libwebp_decode = false
    skia_use_libjpeg_turbo_encode = false
    skia_use_libjpeg_turbo_decode = false
    skia_use_libpng_encode = false
    skia_use_libpng_decode = false
    skia_use_expat = false

    skia_enable_svg = false
    skia_enable_pdf = false
    skia_enable_skottie = false
    skia_enable_tools = false
    skia_enable_gpu = false
"

echo ">>> Building Skia (ninja -C $BUILD_DIR skia) ..."
ninja -C "$BUILD_DIR" skia

# ── Stage output ─────────────────────────────────────────────────────────────
mkdir -p "$SKIA_OUT_DIR/include"

echo ">>> Copying libskia.a ..."
cp "$BUILD_DIR/libskia.a" "$SKIA_OUT_DIR/libskia.a"

echo ">>> Copying Skia headers ..."
# Copy include/ contents directly so <core/SkCanvas.h> etc. resolve correctly.
# skia.cmake sets INTERFACE_INCLUDE_DIRECTORIES to SKIA_PREBUILT_DIR/include,
# and LibGfx uses #include <core/SkCanvas.h> (no skia/ prefix).
cp -r "$SKIA_SRC_DIR/include/." "$SKIA_OUT_DIR/include/"

echo ""
echo "=== Skia WASM build complete ==="
echo "    libskia.a : $SKIA_OUT_DIR/libskia.a  ($(du -sh "$SKIA_OUT_DIR/libskia.a" | cut -f1))"
echo "    headers   : $SKIA_OUT_DIR/include/skia/"
echo ""
echo "Pass to cmake as: -DSKIA_PREBUILT_DIR=$SKIA_OUT_DIR"
