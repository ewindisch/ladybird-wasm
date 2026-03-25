/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Stubs for GLES3 buffer-mapping functions that are not available in
// Emscripten's WebGL2 implementation (WebGL has no glMapBufferRange).
// The WebGL2RenderingContextImpl already checks the return value for
// nullptr before dereferencing, so returning nullptr is safe.

#include <GLES3/gl3.h>

extern "C" {

void* glMapBufferRange(GLenum /*target*/, GLintptr /*offset*/, GLsizeiptr /*length*/, GLbitfield /*access*/)
{
    // Not available in WebGL2.
    return nullptr;
}

GLboolean glUnmapBuffer(GLenum /*target*/)
{
    // Not available in WebGL2.
    return GL_FALSE;
}

} // extern "C"
