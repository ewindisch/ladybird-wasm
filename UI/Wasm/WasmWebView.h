/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "WasmPageClient.h"
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <LibGC/Root.h>
#include <LibGfx/Size.h>

namespace Ladybird {

// WasmWebView is the thin C++ façade exposed to WasmExports.cpp.
// It owns (via GC::Root) a WasmPageClient that implements Web::PageClient
// directly in-process — no IPC, no subprocess.
class WasmWebView {
public:
    static ErrorOr<NonnullOwnPtr<WasmWebView>> create(Gfx::IntSize initial_size);
    ~WasmWebView() = default;

    void load_url(String const& url);
    void set_viewport_size(int width, int height);

    void handle_mouse_down(int x, int y, unsigned button, unsigned modifiers);
    void handle_mouse_up(int x, int y, unsigned button, unsigned modifiers);
    void handle_mouse_move(int x, int y, unsigned modifiers);
    void handle_mouse_wheel(int x, int y, int delta_x, int delta_y, unsigned modifiers);
    void handle_key_down(char const* key_utf8, unsigned modifiers);
    void handle_key_up(char const* key_utf8, unsigned modifiers);

    // Access the front bitmap for blit — valid after ladybirdOnPaint fires.
    RefPtr<Gfx::Bitmap> front_bitmap() const { return m_page_client->front_bitmap(); }

private:
    explicit WasmWebView(GC::Root<WasmPageClient>);

    GC::Root<WasmPageClient> m_page_client;
};

}
