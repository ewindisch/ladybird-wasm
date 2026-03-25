/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/NonnullOwnPtr.h>
#include <AK/Optional.h>
#include <AK/OwnPtr.h>
#include <LibCore/EventLoop.h>

namespace Ladybird {

class WasmApplication {
public:
    static WasmApplication& the();

    static ErrorOr<void> initialize();
    static void shutdown();

    Core::EventLoop& event_loop() { return *m_event_loop; }

    void quit() { m_should_quit = true; }
    bool should_quit() const { return m_should_quit; }

private:
    WasmApplication();
    ~WasmApplication();

    static WasmApplication* s_the;

    OwnPtr<Core::EventLoop> m_event_loop;
    bool m_should_quit { false };
};

}
