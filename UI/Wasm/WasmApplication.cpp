/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "WasmApplication.h"
#include "WasmEventLoop.h"
#include <AK/Error.h>

namespace Ladybird {

WasmApplication* WasmApplication::s_the = nullptr;

WasmApplication::WasmApplication()
{
    s_the = this;
    m_event_loop = make<Core::EventLoop>();
}

WasmApplication::~WasmApplication()
{
    s_the = nullptr;
}

WasmApplication& WasmApplication::the()
{
    VERIFY(s_the);
    return *s_the;
}

ErrorOr<void> WasmApplication::initialize()
{
    if (s_the)
        return Error::from_string_literal("WasmApplication already initialized");

    new WasmApplication();
    WasmEventLoop::initialize();

    return {};
}

void WasmApplication::shutdown()
{
    if (s_the) {
        delete s_the;
        s_the = nullptr;
    }
}

}
