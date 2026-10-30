#!/usr/bin/env bash
# build-ladybird-wasm.sh — Build Ladybird as a WASM module for wanix-tor.
#
# Produces three files in $DEST_DIR (default: web/ladybird/ in wanix-tor):
#   index.js   — Emscripten JS loader (createLadybirdModule)
#   index.wasm — compiled WebAssembly
#   index.data — preloaded resource bundle (fonts etc.)
#
# Usage:
#   ./Meta/scripts/build-ladybird-wasm.sh [--skia <dir>] [--dest <dir>] [--jobs N]
#
#   --skia <dir>  Path to prebuilt Skia (must contain libskia.a and include/).
#                 Defaults to Build/skia-wasm inside the repo.
#                 Build with: ./Meta/scripts/build-skia-wasm.sh
#
#   --dest <dir>  Where to copy the output files.
#                 Defaults to ../../wanix-tor/web/ladybird relative to the repo.
#
#   --jobs N      Parallel build jobs. Defaults to nproc.
#
# Prerequisites:
#   - EMSDK env var set (or emcc in PATH)
#   - cmake >= 3.25, ninja
#   - Skia prebuilt (see --skia)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# ── Defaults ─────────────────────────────────────────────────────────────────
SKIA_PREBUILT_DIR="$REPO_ROOT/third_party/skia/out/wasm-cpu"
DEST_DIR="$REPO_ROOT/../../../wanix-tor/web/ladybird"
JOBS="$(nproc)"

# ── Parse args ───────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --skia) SKIA_PREBUILT_DIR="$2"; shift 2 ;;
        --dest) DEST_DIR="$2"; shift 2 ;;
        --jobs) JOBS="$2"; shift 2 ;;
        *) echo "Unknown arg: $1" >&2; exit 1 ;;
    esac
done

DEST_DIR="$(realpath -m "$DEST_DIR")"

# ── Validate Skia ─────────────────────────────────────────────────────────────
if [[ ! -f "$SKIA_PREBUILT_DIR/libskia.a" ]]; then
    echo "error: libskia.a not found at $SKIA_PREBUILT_DIR/libskia.a" >&2
    echo "       Build it first with: ./Meta/scripts/build-skia-wasm.sh" >&2
    echo "       Or pass --skia <dir>" >&2
    exit 1
fi
echo ">>> Using Skia prebuilt: $SKIA_PREBUILT_DIR"

# ── Emscripten ────────────────────────────────────────────────────────────────
if ! command -v emcc &>/dev/null; then
    if [[ -n "${EMSDK:-}" ]]; then
        # shellcheck source=/dev/null
        source "$EMSDK/emsdk_env.sh" --build=Release
    else
        echo "error: emcc not found. Set EMSDK to your emsdk root or source emsdk_env.sh." >&2
        exit 1
    fi
fi
echo ">>> Using emcc: $(emcc --version 2>&1 | head -1)"

# ── CMake configure ───────────────────────────────────────────────────────────
BUILD_DIR="$REPO_ROOT/Build/wasm"
echo ">>> Configuring Ladybird WASM build ..."

# ── Verify CA bundle integrity ────────────────────────────────────────────────
CACERT="$REPO_ROOT/Base/res/cacert.pem"
EXPECTED_HASH="b6e66569cc3d438dd5abe514d0df50005d570bfc96c14dca8f768d020cb96171"
if [[ -f "$CACERT" ]]; then
    ACTUAL_HASH=$(sha256sum "$CACERT" | cut -d' ' -f1)
    if [[ "$ACTUAL_HASH" != "$EXPECTED_HASH" ]]; then
        echo "ERROR: cacert.pem hash mismatch!" >&2
        echo "  Expected: $EXPECTED_HASH" >&2
        echo "  Actual:   $ACTUAL_HASH" >&2
        echo "  If updating the CA bundle, update EXPECTED_HASH in this script." >&2
        exit 1
    fi
    echo ">>> cacert.pem integrity verified"
else
    echo "WARNING: $CACERT not found — TLS connections will fail at runtime." >&2
fi

# Configure through vcpkg with the wasm32-emscripten triplet (dependencies are
# resolved by vcpkg; the emscripten platform module is chain-loaded). Requires
# CMake >= 4.1 (upstream preset files are version 9) and a bootstrapped vcpkg
# at Build/vcpkg-wasm (see Meta/Utils/build_vcpkg.py).
cd "$REPO_ROOT"
if [[ ! -f "$REPO_ROOT/Build/vcpkg-wasm/scripts/buildsystems/vcpkg.cmake" ]]; then
    echo ">>> Bootstrapping vcpkg (Build/vcpkg-wasm) ..."
    python3 -c "import sys, pathlib; sys.path.append('Meta'); from Utils.build_vcpkg import build_vcpkg; build_vcpkg(pathlib.Path('$REPO_ROOT/Build/vcpkg-wasm').resolve())"
fi
cmake \
    -B "$BUILD_DIR" \
    -S "$REPO_ROOT" \
    -DCMAKE_TOOLCHAIN_FILE="$REPO_ROOT/Build/vcpkg-wasm/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET=wasm32-emscripten \
    -DVCPKG_OVERLAY_TRIPLETS="$REPO_ROOT/Meta/CMake/vcpkg" \
    -DVCPKG_CHAIN_LOAD_TOOLCHAIN_FILE="$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_QT=OFF

# ── Build ─────────────────────────────────────────────────────────────────────
# Emscripten's --js-library files are not tracked as ninja dependencies.
# Touch main.cpp to force a re-link when tor-bridge.js changes.
JS_LIB="$REPO_ROOT/UI/Wasm/Resources/tor-bridge.js"
MAIN_CPP="$REPO_ROOT/UI/Wasm/main.cpp"
if [[ -f "$JS_LIB" && -f "$MAIN_CPP" && "$JS_LIB" -nt "$BUILD_DIR/bin/index.js" ]]; then
    touch "$MAIN_CPP"
fi
echo ">>> Building ladybird-wasm (-j$JOBS) ..."
cmake --build "$BUILD_DIR" --target ladybird-wasm -- -j"$JOBS"

# ── Copy to destination ───────────────────────────────────────────────────────
STAGED_DIR="$BUILD_DIR/ladybird"
if [[ ! -f "$STAGED_DIR/index.js" ]]; then
    echo "error: Expected staged output at $STAGED_DIR/index.js — build may have failed." >&2
    exit 1
fi

echo ">>> Copying WASM artifacts to $DEST_DIR ..."
mkdir -p "$DEST_DIR"
cp "$STAGED_DIR/index.js"   "$DEST_DIR/index.js"
cp "$STAGED_DIR/index.wasm" "$DEST_DIR/index.wasm"
cp "$STAGED_DIR/index.data" "$DEST_DIR/index.data"

echo ""
echo "=== Ladybird WASM build complete ==="
for f in index.js index.wasm index.data; do
    echo "    $DEST_DIR/$f  ($(du -sh "$DEST_DIR/$f" | cut -f1))"
done
echo ""
echo "Run build-web to regenerate SRI hashes:"
echo "    cd $REPO_ROOT/../../wanix-tor && ./build.sh"
