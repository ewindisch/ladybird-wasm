/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Platform/ImageCodecPlugin.h>

namespace Ladybird {

// In-process image codec plugin for the WASM port.
// Decodes images synchronously using LibGfx::ImageDecoder (no IPC subprocess).
class WasmImageCodecPlugin final : public Web::Platform::ImageCodecPlugin {
public:
    WasmImageCodecPlugin() = default;
    virtual ~WasmImageCodecPlugin() override = default;

    virtual NonnullRefPtr<Core::Promise<Web::Platform::DecodedImage>> decode_image(
        ReadonlyBytes,
        ESCAPING Function<ErrorOr<void>(Web::Platform::DecodedImage&)> on_resolved,
        ESCAPING Function<void(Error&)> on_rejected) override;

    virtual void request_animation_frames(i64, u32, u32) override { }
    virtual void stop_animation_decode(i64) override { }
};

}
