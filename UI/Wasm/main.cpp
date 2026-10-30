/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "WasmApplication.h"
#include "WasmImageCodecPlugin.h"
#include <AK/Platform.h>
#include <LibGfx/Font/FontDatabase.h>
#include <LibGfx/Font/PathFontProvider.h>
#include <LibMain/Main.h>
#include <LibWeb/Bindings/AgentType.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/Loader/ResourceLoader.h>
#include <LibWeb/Platform/EventLoopPlugin.h>
#include <LibWeb/Platform/FontPlugin.h>
#include <LibWeb/Platform/ImageCodecPlugin.h>
#include <emscripten.h>
#ifdef AK_OS_EMSCRIPTEN
#    include <fcntl.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

ErrorOr<int> ladybird_main(Main::Arguments)
{
    // Install platform plugins before any LibWeb subsystem uses them.
    Web::Platform::EventLoopPlugin::install(*new Web::Platform::EventLoopPlugin);
    Web::Platform::ImageCodecPlugin::install(*new Ladybird::WasmImageCodecPlugin);

    // Initialize the GC/JS VM — must happen before any WasmPageClient is created.
    Web::Bindings::initialize_main_thread_vm(Web::Bindings::AgentType::SimilarOriginWindow);


#ifdef AK_OS_EMSCRIPTEN
    // Initialize ResourceLoader without a RequestClient: the WASM networking
    // layer (tor-bridge.js / $LadybirdHTTP) carries actual network loads; see
    // the AK_OS_EMSCRIPTEN branch in ResourceLoader::load().
    Web::ResourceLoader::initialize(Web::Bindings::main_thread_vm().heap());
#endif

#ifdef AK_OS_EMSCRIPTEN
    // Emscripten: MappedFile::map() may fail on MEMFS (mmap of preloaded files is unreliable).
    // Install the PathFontProvider here (so we can load fonts before FontPlugin queries them),
    // then pass the already-installed provider to FontPlugin to prevent a double-install.
    auto& path_font_provider = static_cast<Gfx::PathFontProvider&>(
        Gfx::FontDatabase::the().install_system_font_provider(make<Gfx::PathFontProvider>()));

    {
        static char const* const bundled_fonts[] = {
            "/res/fonts/SerenitySans-Regular.ttf",
            "/res/fonts/NotoEmoji.ttf",
            nullptr,
        };

        for (char const* const* p = bundled_fonts; *p; ++p) {
            struct stat st {};
            if (::stat(*p, &st) != 0)
                continue;
            auto size = static_cast<size_t>(st.st_size);
            auto buf_or_err = ByteBuffer::create_uninitialized(size);
            if (buf_or_err.is_error())
                continue;
            auto buf = buf_or_err.release_value();
            int fd = ::open(*p, O_RDONLY);
            if (fd < 0)
                continue;
            ssize_t nread = ::read(fd, buf.data(), size);
            ::close(fd);
            if (nread != static_cast<ssize_t>(size))
                continue;
            path_font_provider.load_font_from_bytes(move(buf));
            emscripten_log(EM_LOG_CONSOLE, "Loaded font: %s", *p);
        }
    }

    // FontPlugin depends on the VM being initialized (font directories use Core::Resource).
    // Use layout-test mode (true): only SerenitySans/NotoEmoji are bundled.
    // Pass path_font_provider so FontPlugin skips its own install_system_font_provider call.
    Web::Platform::FontPlugin::install(*new Web::Platform::FontPlugin(true, &path_font_provider));
#else
    Web::Platform::FontPlugin::install(*new Web::Platform::FontPlugin(false));
#endif

    TRY(Ladybird::WasmApplication::initialize());

    emscripten_log(EM_LOG_CONSOLE, "Ladybird WASM runtime initialized");
    emscripten_log(EM_LOG_CONSOLE, "Waiting for JavaScript to call wasm_initialize()...");

    return 0;
}
