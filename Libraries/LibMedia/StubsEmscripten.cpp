/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Stub implementations for FFmpeg-dependent LibMedia symbols in the WASM
// build where FFmpeg is not available. Media playback is not supported.

#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/NonnullRefPtr.h>
#include <LibMedia/DecoderError.h>
#include <LibMedia/FFmpeg/FFmpegAudioConverter.h>
#include <LibMedia/FFmpeg/FFmpegAudioDecoder.h>
#include <LibMedia/FFmpeg/FFmpegDemuxer.h>
#include <LibMedia/FFmpeg/FFmpegVideoDecoder.h>

namespace Media::FFmpeg {

// FFmpegDemuxer ---------------------------------------------------------------

DecoderErrorOr<NonnullRefPtr<FFmpegDemuxer>> FFmpegDemuxer::from_stream(NonnullRefPtr<MediaStream> const&)
{
    return DecoderError::with_description(DecoderErrorCategory::NotImplemented, "FFmpeg not available in WASM build"sv);
}

// FFmpegAudioConverter --------------------------------------------------------

ErrorOr<NonnullOwnPtr<FFmpegAudioConverter>> FFmpegAudioConverter::try_create()
{
    return Error::from_string_literal("FFmpegAudioConverter not available in WASM build");
}

FFmpegAudioConverter::~FFmpegAudioConverter() = default;

// FFmpegAudioDecoder ----------------------------------------------------------

DecoderErrorOr<NonnullOwnPtr<FFmpegAudioDecoder>> FFmpegAudioDecoder::try_create(CodecID, Audio::SampleSpecification const&, ReadonlyBytes)
{
    return DecoderError::with_description(DecoderErrorCategory::NotImplemented, "FFmpegAudioDecoder not available in WASM build"sv);
}

FFmpegAudioDecoder::~FFmpegAudioDecoder() = default;

// FFmpegVideoDecoder ----------------------------------------------------------

DecoderErrorOr<NonnullOwnPtr<FFmpegVideoDecoder>> FFmpegVideoDecoder::try_create(CodecID, ReadonlyBytes)
{
    return DecoderError::with_description(DecoderErrorCategory::NotImplemented, "FFmpegVideoDecoder not available in WASM build"sv);
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() = default;

} // namespace Media::FFmpeg
