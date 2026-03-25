/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Stub implementations for Skia symbols that are referenced from libskia.a
// but not compiled in because the WASM build has skia_use_libpng_encode=false.

#include <core/SkPixmap.h>
#include <encode/SkPngEncoder.h>

namespace SkPngEncoder {

bool Encode(SkWStream*, const SkPixmap&, const Options&)
{
    // PNG encoding not available in WASM 2D-canvas-only build.
    return false;
}

sk_sp<SkData> Encode(GrDirectContext*, const SkImage*, const Options&)
{
    return nullptr;
}

std::unique_ptr<SkEncoder> Make(SkWStream*, const SkPixmap&, const Options&)
{
    return nullptr;
}

} // namespace SkPngEncoder
