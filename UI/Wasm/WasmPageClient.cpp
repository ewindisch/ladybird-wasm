/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "WasmPageClient.h"
#include <LibCore/AnonymousBuffer.h>
#include <LibGfx/SystemTheme.h>
#include <LibGC/Heap.h>
#include <LibWeb/HTML/TraversableNavigable.h>
#include <emscripten.h>

namespace Ladybird {

GC_DEFINE_ALLOCATOR(WasmPageClient);

GC::Ref<WasmPageClient> WasmPageClient::create(JS::VM& vm, Web::DevicePixelSize initial_size)
{
    return vm.heap().allocate<WasmPageClient>(vm, initial_size);
}

WasmPageClient::WasmPageClient(JS::VM& vm, Web::DevicePixelSize initial_size)
    : m_page(Web::Page::create(vm, *this))
    , m_viewport_size(initial_size)
    , m_palette_impl(Gfx::PaletteImpl::create_with_anonymous_buffer(
          // A zeroed SystemTheme gives sensible defaults for a dark browser view.
          MUST(Core::AnonymousBuffer::create_with_size(sizeof(Gfx::SystemTheme)))))
{
    // Rendering is now driven directly from WasmEventLoop::main_loop_iteration()
    // (the Emscripten RAF callback) rather than a Core::Timer, since timerfd
    // does not work in Emscripten.
}

WasmPageClient::~WasmPageClient() = default;

void WasmPageClient::visit_edges(JS::Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_page);
}

void WasmPageClient::set_viewport_size(Web::DevicePixelSize size)
{
    if (m_viewport_size == size)
        return;
    m_viewport_size = size;
    m_page->set_window_size(size);
    // Also update the Navigable viewport so BackingStoreManager resizes correctly.
    if (m_page->top_level_traversable_is_initialized())
        m_page->top_level_traversable()->set_viewport_size(m_page->device_to_css_size(size));
}

Gfx::Palette WasmPageClient::palette() const
{
    return Gfx::Palette(*m_palette_impl);
}

Web::DevicePixelRect WasmPageClient::screen_rect() const
{
    return { 0, 0, m_viewport_size.width(), m_viewport_size.height() };
}

// NOTE: Frame presentation moved to the compositor architecture upstream
// (LibWeb/Compositor + the Compositor service). Until the WASM build has an
// in-process rasterizer wired to CompositorHost (see the sync MR notes), no
// frames are produced here and the canvas stays blank.

}
