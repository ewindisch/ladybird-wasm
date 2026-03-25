/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "WasmWebView.h"
#include <AK/String.h>
#include <emscripten.h>
#include <LibGC/Root.h>
#include <LibURL/Parser.h>
#include <LibURL/URL.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/HTML/TraversableNavigable.h>
#include <LibWeb/UIEvents/KeyCode.h>

namespace Ladybird {

ErrorOr<NonnullOwnPtr<WasmWebView>> WasmWebView::create(Gfx::IntSize initial_size)
{
    auto& vm = Web::Bindings::main_thread_vm();
    auto size = Web::DevicePixelSize { initial_size.width(), initial_size.height() };
    auto page_client = WasmPageClient::create(vm, size);
    // Root the GC cell so it isn't collected while WasmWebView is alive.
    GC::Root<WasmPageClient> root { page_client };
    auto view = TRY(adopt_nonnull_own_or_enomem(new (nothrow) WasmWebView(move(root))));

    // Initialize the top-level traversable so event handlers and paint work correctly.
    // Without this, Page::top_level_traversable() returns a null GC::Ptr, causing
    // a null dereference into zeroed WASM memory and VERIFY(m_ptr) to fire in
    // EventHandler::m_drag_and_drop_event_handler on the first input event or paint.
    //
    // create_a_fresh_top_level_traversable returns
    // AK::Variant<WebIDL::SimpleException, ...> which is not convertible to
    // AK::Error via TRY(), so we use release_value_but_fixme_should_propagate_errors()
    // and guard the subsequent calls with top_level_traversable_is_initialized().
    Web::HTML::TraversableNavigable::create_a_fresh_top_level_traversable(
        view->m_page_client->page(), URL::about_blank())
        .release_value_but_fixme_should_propagate_errors();

    // Guard: if traversable creation failed silently, calling top_level_traversable()
    // without this check would deref a null GC::Ptr, causing an indirect call through
    // address 0 → RuntimeError: index out of bounds in the WASM function table.
    if (!view->m_page_client->page().top_level_traversable_is_initialized()) {
        emscripten_log(EM_LOG_ERROR, "top_level_traversable not initialized after create_a_fresh_top_level_traversable");
        return Error::from_errno(ENOMEM);
    }

    // Propagate the initial device-pixel viewport size to the Navigable.
    // BackingStoreManager::resize_backing_stores_if_needed checks viewport_rect().size()
    // which comes from Navigable::m_viewport_size (CSS pixels).  Without this call
    // the navigable viewport stays at 0×0, backing stores are never allocated and
    // page_did_paint is never invoked.
    auto css_size = view->m_page_client->page().device_to_css_size(
        Web::DevicePixelSize { initial_size.width(), initial_size.height() });
    view->m_page_client->page().top_level_traversable()->set_viewport_size(css_size);

    // Mark the document visible so update_the_rendering() does not skip it.
    // Document::m_visibility_state defaults to Hidden; without this call the
    // RAF callback fires but EventLoop::update_the_rendering() filters out the
    // document and page_did_paint is never invoked.
    // set_system_visibility_state queues a UserInteraction task that calls
    // update_the_visibility_state(Visible) — it runs before the first rendering
    // task because the UserInteraction task is queued here and processed on the
    // first EventLoop::process() call from the RAF main loop.
    view->m_page_client->page().top_level_traversable()->set_system_visibility_state(
        Web::HTML::VisibilityState::Visible);

    return view;
}

WasmWebView::WasmWebView(GC::Root<WasmPageClient> page_client)
    : m_page_client(move(page_client))
{
}

void WasmWebView::load_url(String const& url)
{
    auto parsed = URL::Parser::basic_parse(url);
    if (!parsed.has_value())
        parsed = URL::Parser::basic_parse(MUST(String::formatted("https://{}", url)));
    if (parsed.has_value())
        m_page_client->page().load(*parsed);
}

void WasmWebView::set_viewport_size(int width, int height)
{
    m_page_client->set_viewport_size({ width, height });
}

void WasmWebView::handle_mouse_down(int x, int y, unsigned button, unsigned modifiers)
{
    Web::DevicePixelPoint pos { x, y };
    m_page_client->page().handle_mousedown(pos, pos, button, 1u << button, modifiers, 1);
}

void WasmWebView::handle_mouse_up(int x, int y, unsigned button, unsigned modifiers)
{
    Web::DevicePixelPoint pos { x, y };
    m_page_client->page().handle_mouseup(pos, pos, button, 1u << button, modifiers);
}

void WasmWebView::handle_mouse_move(int x, int y, unsigned modifiers)
{
    Web::DevicePixelPoint pos { x, y };
    m_page_client->page().handle_mousemove(pos, pos, 0, modifiers);
}

void WasmWebView::handle_mouse_wheel(int x, int y, int delta_x, int delta_y, unsigned modifiers)
{
    Web::DevicePixelPoint pos { x, y };
    m_page_client->page().handle_mousewheel(pos, pos, 0, 0, modifiers,
        Web::DevicePixels { delta_x }, Web::DevicePixels { delta_y });
}

void WasmWebView::handle_key_down(char const* key_utf8, unsigned modifiers)
{
    auto key = Web::UIEvents::key_code_from_string(StringView { key_utf8, __builtin_strlen(key_utf8) });
    u32 code_point = (StringView { key_utf8, __builtin_strlen(key_utf8) }.length() == 1)
        ? static_cast<u8>(key_utf8[0]) : 0;
    m_page_client->page().handle_keydown(key, modifiers, code_point, false);
}

void WasmWebView::handle_key_up(char const* key_utf8, unsigned modifiers)
{
    auto key = Web::UIEvents::key_code_from_string(StringView { key_utf8, __builtin_strlen(key_utf8) });
    u32 code_point = (StringView { key_utf8, __builtin_strlen(key_utf8) }.length() == 1)
        ? static_cast<u8>(key_utf8[0]) : 0;
    m_page_client->page().handle_keyup(key, modifiers, code_point, false);
}

}
