/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "WasmWebView.h"
#include <AK/String.h>
#include <LibCore/EventLoop.h>
#include <emscripten.h>

using namespace Ladybird;

static WasmWebView* g_web_view = nullptr;

extern "C" {

// Forward declarations (required to satisfy -Wmissing-prototypes even for extern "C")
int wasm_initialize(int width, int height);
void wasm_shutdown();
void wasm_resize(int width, int height);
void wasm_mouse_down(int x, int y, int button, int modifiers);
void wasm_mouse_up(int x, int y, int button, int modifiers);
void wasm_mouse_move(int x, int y, int modifiers);
void wasm_scroll(int delta_x, int delta_y);
void wasm_key_down(char const* key, int modifiers);
void wasm_key_up(char const* key, int modifiers);
void wasm_navigate(char const* url);
uint8_t* wasm_get_bitmap_data();
int wasm_get_bitmap_width();
int wasm_get_bitmap_height();
int wasm_get_bitmap_pitch();

EMSCRIPTEN_KEEPALIVE
int wasm_initialize(int width, int height)
{
    auto view_or_error = WasmWebView::create({ width, height });
    if (view_or_error.is_error()) {
        emscripten_log(EM_LOG_ERROR, "Failed to create WasmWebView");
        return -1;
    }
    g_web_view = view_or_error.release_value().leak_ptr();
    emscripten_log(EM_LOG_CONSOLE, "Ladybird WASM initialized: %dx%d", width, height);
    return 0;
}

EMSCRIPTEN_KEEPALIVE
void wasm_shutdown()
{
    delete g_web_view;
    g_web_view = nullptr;
}

EMSCRIPTEN_KEEPALIVE
void wasm_resize(int width, int height)
{
    if (g_web_view)
        g_web_view->set_viewport_size(width, height);
}

EMSCRIPTEN_KEEPALIVE
void wasm_mouse_down(int x, int y, int button, int modifiers)
{
    if (g_web_view)
        g_web_view->handle_mouse_down(x, y, static_cast<unsigned>(button), static_cast<unsigned>(modifiers));
}

EMSCRIPTEN_KEEPALIVE
void wasm_mouse_up(int x, int y, int button, int modifiers)
{
    if (g_web_view)
        g_web_view->handle_mouse_up(x, y, static_cast<unsigned>(button), static_cast<unsigned>(modifiers));
}

EMSCRIPTEN_KEEPALIVE
void wasm_mouse_move(int x, int y, int modifiers)
{
    if (g_web_view)
        g_web_view->handle_mouse_move(x, y, static_cast<unsigned>(modifiers));
}

EMSCRIPTEN_KEEPALIVE
void wasm_scroll(int delta_x, int delta_y)
{
    if (g_web_view)
        g_web_view->handle_mouse_wheel(0, 0, delta_x, delta_y, 0);
}

EMSCRIPTEN_KEEPALIVE
void wasm_key_down(char const* key, int modifiers)
{
    if (g_web_view)
        g_web_view->handle_key_down(key, static_cast<unsigned>(modifiers));
}

EMSCRIPTEN_KEEPALIVE
void wasm_key_up(char const* key, int modifiers)
{
    if (g_web_view)
        g_web_view->handle_key_up(key, static_cast<unsigned>(modifiers));
}

EMSCRIPTEN_KEEPALIVE
void wasm_navigate(char const* url)
{
    if (!g_web_view)
        return;
    auto url_string = MUST(String::from_utf8(StringView { url, __builtin_strlen(url) }));
    emscripten_log(EM_LOG_CONSOLE, "Navigating to: %s", url);
    // Queue the navigation as a deferred task so wasm_navigate returns immediately
    // without ever calling emscripten_sleep.  Keeping wasm_navigate out of the
    // ASYNCIFY call graph prevents it from conflicting with wasm_initialize's
    // ccall({async:true}) ASYNCIFY rewind state.
    Core::EventLoop::current().deferred_invoke([view = g_web_view, url_string = move(url_string)]() mutable {
        view->load_url(url_string);
    });
}

EMSCRIPTEN_KEEPALIVE
uint8_t* wasm_get_bitmap_data()
{
    if (!g_web_view)
        return nullptr;
    auto bm = g_web_view->front_bitmap();
    if (!bm)
        return nullptr;
    return bm->scanline_u8(0);
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_bitmap_width()
{
    if (!g_web_view) return 0;
    auto bm = g_web_view->front_bitmap();
    return bm ? bm->width() : 0;
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_bitmap_height()
{
    if (!g_web_view) return 0;
    auto bm = g_web_view->front_bitmap();
    return bm ? bm->height() : 0;
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_bitmap_pitch()
{
    if (!g_web_view) return 0;
    auto bm = g_web_view->front_bitmap();
    return bm ? bm->pitch() : 0;
}

} // extern "C"
