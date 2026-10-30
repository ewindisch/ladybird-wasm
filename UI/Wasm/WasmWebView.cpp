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
#include <LibWeb/HTML/LocalTraversableNavigable.h>
#include <LibWeb/UIEvents/KeyCode.h>
#include <LibWebCommon/HTML/SessionHistoryEntryDescriptor.h>

namespace Ladybird {

ErrorOr<NonnullOwnPtr<WasmWebView>> WasmWebView::create(Gfx::IntSize initial_size)
{
    auto& vm = Web::Bindings::main_thread_vm();
    auto size = Web::DevicePixelSize { initial_size.width(), initial_size.height() };
    auto page_client = WasmPageClient::create(vm, size);
    // Root the GC cell so it isn't collected while WasmWebView is alive.
    GC::Root<WasmPageClient> root { page_client };
    auto view = TRY(adopt_nonnull_own_or_enomem(new (nothrow) WasmWebView(move(root))));

    // Mark the page visible before the traversable exists so update_the_rendering()
    // does not filter the document out of the first rendering update.
    view->m_page_client->page().set_system_visibility_state(Web::HTML::VisibilityState::Visible);

    // Create the top-level traversable from an initial about:blank session
    // history entry, mirroring WebContent's PageHost::initialize() and
    // LibWebView's Application::create_web_content_client() flow.
    auto& id_allocator = view->m_page_client->cross_process_id_allocator();
    auto root_navigable_id = id_allocator.allocate();
    auto document_state_id = id_allocator.allocate();
    auto initial_document_id = id_allocator.allocate();
    (void)root_navigable_id;

    auto entry = Web::HTML::create_initial_session_history_entry_descriptor(document_state_id, {}, {});
    entry.document_state.origin = URL::Origin::create_opaque();

    Web::HTML::LocalTraversableNavigable::create_a_fresh_top_level_traversable(
        view->m_page_client->page(), move(entry), initial_document_id);

    // Propagate the initial device-pixel viewport size to the Navigable.
    auto* traversable = view->m_page_client->page().local_traversable();
    if (!traversable) {
        emscripten_log(EM_LOG_ERROR, "local traversable not initialized after create_a_fresh_top_level_traversable");
        return Error::from_errno(ENOMEM);
    }
    auto css_size = view->m_page_client->page().device_to_css_size(
        Web::DevicePixelSize { initial_size.width(), initial_size.height() });
    traversable->set_viewport_size(css_size);

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
