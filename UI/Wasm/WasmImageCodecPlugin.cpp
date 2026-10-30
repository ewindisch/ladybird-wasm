/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "WasmImageCodecPlugin.h"
#include <LibCore/Promise.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/ImageFormats/ImageDecoder.h>

namespace Ladybird {

NonnullRefPtr<Core::Promise<Web::Platform::DecodedImage>>
WasmImageCodecPlugin::decode_image(
    ReadonlyBytes bytes,
    Function<ErrorOr<void>(Web::Platform::DecodedImage&)> on_resolved,
    Function<void(Error&)> on_rejected)
{
    auto promise = Core::Promise<Web::Platform::DecodedImage>::construct();
    if (on_resolved)
        promise->on_resolution = move(on_resolved);
    if (on_rejected)
        promise->on_rejection = move(on_rejected);

    auto decoder_or_error = Gfx::ImageDecoder::try_create_for_raw_bytes(bytes);
    if (decoder_or_error.is_error() || !decoder_or_error.value()) {
        auto error = decoder_or_error.is_error()
            ? decoder_or_error.release_error()
            : Error::from_string_literal("Unknown image format");
        promise->reject(move(error));
        return promise;
    }

    auto decoder = decoder_or_error.release_value();
    Web::Platform::DecodedImage decoded_image;
    decoded_image.is_animated = decoder->is_animated();
    decoded_image.loop_count = decoder->loop_count();
    decoded_image.frame_count = decoder->frame_count();

    for (size_t i = 0; i < decoder->frame_count(); ++i) {
        auto frame_or_error = decoder->frame(i);
        if (frame_or_error.is_error()) {
            auto error = frame_or_error.release_error();
            promise->reject(move(error));
            return promise;
        }
        auto frame = frame_or_error.release_value();
        decoded_image.frames.empend(move(frame.image), static_cast<size_t>(frame.duration));
    }

    promise->resolve(move(decoded_image));
    return promise;
}

ErrorOr<Web::Platform::DecodedFirstFrame>
WasmImageCodecPlugin::decode_first_frame_synchronously(ReadonlyBytes bytes)
{
    auto decoder_or_error = Gfx::ImageDecoder::try_create_for_raw_bytes(bytes);
    if (decoder_or_error.is_error())
        return decoder_or_error.release_error();
    auto decoder = decoder_or_error.release_value();
    if (!decoder)
        return Error::from_string_literal("Unknown image format");

    auto frame_or_error = decoder->frame(0);
    if (frame_or_error.is_error())
        return frame_or_error.release_error();
    auto frame = frame_or_error.release_value();

    return Web::Platform::DecodedFirstFrame {
        .bitmap = move(frame.image),
        .color_space = frame.color_space,
    };
}

}
