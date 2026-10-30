/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Queue.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/Palette.h>
#include <LibWeb/Page/InputEvent.h>
#include <LibWeb/Page/Page.h>

namespace Ladybird {

class WasmPageClient final : public Web::PageClient {
    GC_CELL(WasmPageClient, Web::PageClient);
    GC_DECLARE_ALLOCATOR(WasmPageClient);

public:
    static GC::Ref<WasmPageClient> create(JS::VM&, Web::DevicePixelSize initial_size);
    virtual ~WasmPageClient() override;

    virtual Web::Page& page() override { return *m_page; }
    virtual Web::Page const& page() const override { return *m_page; }

    void set_viewport_size(Web::DevicePixelSize);

    // Bitmap access — swapped on page_did_paint; safe to read from JS callback
    RefPtr<Gfx::Bitmap> front_bitmap() const { return m_front_bitmap; }

    // ── Web::PageClient pure virtuals ────────────────────────────────────
    virtual Web::PageId id() const override { return 0; }
    virtual bool is_connection_open() const override { return true; }
    virtual Gfx::Palette palette() const override;
    virtual Web::DevicePixelRect screen_rect() const override;
    virtual double zoom_level() const override { return 1.0; }
    virtual double device_pixel_ratio() const override { return 1.0; }
    virtual double device_pixels_per_css_pixel() const override { return 1.0; }
    virtual Web::CSS::PreferredColorScheme preferred_color_scheme() const override { return Web::CSS::PreferredColorScheme::Auto; }
    virtual Web::CSS::PreferredContrast preferred_contrast() const override { return Web::CSS::PreferredContrast::NoPreference; }
    virtual Web::CSS::PreferredMotion preferred_motion() const override { return Web::CSS::PreferredMotion::NoPreference; }
    virtual size_t screen_count() const override { return 1; }
    virtual Queue<Web::QueuedInputEvent>& input_event_queue() override { return m_input_event_queue; }
    virtual void report_finished_handling_input_event(Web::PageId, u64, Web::EventResult) override { }
    virtual void request_file(Web::FileRequest) override { }
    virtual bool is_headless() const override { return false; }

    virtual void request_frame() override { }

private:
    WasmPageClient(JS::VM&, Web::DevicePixelSize initial_size);

    virtual void visit_edges(JS::Cell::Visitor&) override;

    GC::Ref<Web::Page> m_page;
    Web::DevicePixelSize m_viewport_size;

    // Double-buffered Gfx::Bitmap pair keyed by LibWeb bitmap ids
    i32 m_front_bitmap_id { -1 };
    i32 m_back_bitmap_id  { -1 };
    RefPtr<Gfx::Bitmap> m_front_bitmap;
    RefPtr<Gfx::Bitmap> m_back_bitmap;

    Queue<Web::QueuedInputEvent> m_input_event_queue;

    // Minimal palette backed by a zeroed SystemTheme
    NonnullRefPtr<Gfx::PaletteImpl> m_palette_impl;
};

}
