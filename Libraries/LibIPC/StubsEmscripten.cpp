/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Stub implementations for IPC::TransportSocket symbols referenced by LibWeb's
// auto-generated IPC serialization code in the EMSCRIPTEN/WASM build where
// WasmPageClient runs in-process with no IPC transport (no AF_UNIX sockets).
// File.cpp and Message.cpp are compiled normally; only TransportSocket needs stubs.

#include <LibThreading/Thread.h>
#include <LibIPC/TransportSocket.h>

namespace IPC {

// TransportSocket is not usable without AF_UNIX sockets; provide no-op stubs.
TransportSocket::~TransportSocket() = default;

void TransportSocket::wait_until_readable()
{
}

void TransportSocket::close()
{
}

void TransportSocket::close_after_sending_all_pending_messages()
{
}

void TransportSocket::set_up_read_hook(Function<void()>)
{
}

bool TransportSocket::is_open() const
{
    return false;
}

TransportSocket::ShouldShutdown TransportSocket::read_as_many_messages_as_possible_without_blocking(Function<void(Message&&)>&&)
{
    return ShouldShutdown::Yes;
}

ErrorOr<TransportHandle> TransportSocket::release_for_transfer()
{
    return Error::from_string_literal("TransportSocket not available in WASM build");
}

ErrorOr<TransportSocket::Paired> TransportSocket::create_paired()
{
    return Error::from_string_literal("TransportSocket not available in WASM build");
}

}
