/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Function.h>
#include <LibCore/Event.h>

namespace Ladybird {

class WasmEventLoop {
public:
    static void initialize();
    static void process_events();
    static void schedule_callback(Function<void()> callback, int delay_ms = 0);

private:
    WasmEventLoop() = default;
};

}
