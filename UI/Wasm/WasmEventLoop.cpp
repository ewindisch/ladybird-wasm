/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "WasmEventLoop.h"
#include "WasmApplication.h"
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <emscripten.h>
#include <emscripten/html5.h>

namespace Ladybird {

static void main_loop_iteration()
{
    if (WasmApplication::the().should_quit()) {
        emscripten_cancel_main_loop();
        return;
    }

    // Core::Timer (timerfd) does not work in Emscripten, so we drive the HTML
    // event loop directly from the browser's animation frame instead of relying
    // on Core::Timer to call schedule()/process().
    auto& html_loop = Web::HTML::main_thread_event_loop();
    html_loop.queue_task_to_update_the_rendering();
    html_loop.process();

    WasmEventLoop::process_events();
}

void WasmEventLoop::initialize()
{
    // Drive the Core::EventLoop from the browser's animation frame.
    emscripten_set_main_loop(main_loop_iteration, 0, 0);
}

void WasmEventLoop::process_events()
{
    WasmApplication::the().event_loop().pump(Core::EventLoop::WaitMode::PollForEvents);
}

void WasmEventLoop::schedule_callback(Function<void()> callback, int delay_ms)
{
    struct CallbackHolder {
        Function<void()> fn;
    };
    auto* holder = new CallbackHolder { move(callback) };
    emscripten_async_call(
        [](void* arg) {
            auto* h = static_cast<CallbackHolder*>(arg);
            h->fn();
            delete h;
        },
        holder,
        delay_ms);
}

}
