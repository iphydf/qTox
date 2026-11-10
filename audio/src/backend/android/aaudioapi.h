/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QtGlobal>

#ifdef Q_OS_ANDROID

#include <aaudio/AAudio.h>

/**
 * @brief The AAudio functions qTox uses, from libaaudio.so.
 *
 * qTox runs on Android versions without AAudio, so it can't link to it. The types are written out
 * because the NDK marks the functions unavailable below their Android version.
 */
struct AAudioApi
{
    // Null before Android 9, which lacks the usages and presets for calls.
    static const AAudioApi* get();

    aaudio_result_t (*createStreamBuilder)(AAudioStreamBuilder** builder);
    void (*builderSetDirection)(AAudioStreamBuilder* builder, aaudio_direction_t direction);
    void (*builderSetSampleRate)(AAudioStreamBuilder* builder, int32_t sampleRate);
    void (*builderSetChannelCount)(AAudioStreamBuilder* builder, int32_t channelCount);
    void (*builderSetFormat)(AAudioStreamBuilder* builder, aaudio_format_t format);
    void (*builderSetBufferCapacityInFrames)(AAudioStreamBuilder* builder, int32_t numFrames);
    void (*builderSetUsage)(AAudioStreamBuilder* builder, aaudio_usage_t usage);
    void (*builderSetContentType)(AAudioStreamBuilder* builder, aaudio_content_type_t contentType);
    void (*builderSetInputPreset)(AAudioStreamBuilder* builder, aaudio_input_preset_t inputPreset);
    void (*builderSetDataCallback)(AAudioStreamBuilder* builder, AAudioStream_dataCallback callback,
                                   void* userData);
    void (*builderSetErrorCallback)(AAudioStreamBuilder* builder,
                                    AAudioStream_errorCallback callback, void* userData);
    aaudio_result_t (*builderOpenStream)(AAudioStreamBuilder* builder, AAudioStream** stream);
    aaudio_result_t (*builderDelete)(AAudioStreamBuilder* builder);

    aaudio_result_t (*streamRequestStart)(AAudioStream* stream);
    aaudio_result_t (*streamRequestStop)(AAudioStream* stream);
    aaudio_result_t (*streamClose)(AAudioStream* stream);
    aaudio_result_t (*streamRead)(AAudioStream* stream, void* buffer, int32_t numFrames,
                                  int64_t timeoutNanoseconds);
    aaudio_result_t (*streamWrite)(AAudioStream* stream, const void* buffer, int32_t numFrames,
                                   int64_t timeoutNanoseconds);
    aaudio_result_t (*streamSetBufferSizeInFrames)(AAudioStream* stream, int32_t numFrames);
    int32_t (*streamGetBufferCapacityInFrames)(AAudioStream* stream);
    aaudio_stream_state_t (*streamGetState)(AAudioStream* stream);

    const char* (*convertResultToText)(aaudio_result_t returnCode);
};

#endif // Q_OS_ANDROID
