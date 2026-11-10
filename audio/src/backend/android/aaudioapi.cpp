/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "audio/src/backend/android/aaudioapi.h"

#ifdef Q_OS_ANDROID

#include <QDebug>

#include <dlfcn.h>

namespace {
template <typename Function>
bool load(void* library, Function& function, const char* name)
{
    function = reinterpret_cast<Function>(dlsym(library, name));
    if (function == nullptr) {
        qWarning() << "AAudio lacks" << name;
        return false;
    }
    return true;
}

bool load(void* library, AAudioApi& api)
{
    return load(library, api.createStreamBuilder, "AAudio_createStreamBuilder")
           && load(library, api.builderSetDirection, "AAudioStreamBuilder_setDirection")
           && load(library, api.builderSetSampleRate, "AAudioStreamBuilder_setSampleRate")
           && load(library, api.builderSetChannelCount, "AAudioStreamBuilder_setChannelCount")
           && load(library, api.builderSetFormat, "AAudioStreamBuilder_setFormat")
           && load(library, api.builderSetBufferCapacityInFrames,
                   "AAudioStreamBuilder_setBufferCapacityInFrames")
           && load(library, api.builderSetUsage, "AAudioStreamBuilder_setUsage")
           && load(library, api.builderSetContentType, "AAudioStreamBuilder_setContentType")
           && load(library, api.builderSetInputPreset, "AAudioStreamBuilder_setInputPreset")
           && load(library, api.builderSetDataCallback, "AAudioStreamBuilder_setDataCallback")
           && load(library, api.builderSetErrorCallback, "AAudioStreamBuilder_setErrorCallback")
           && load(library, api.builderOpenStream, "AAudioStreamBuilder_openStream")
           && load(library, api.builderDelete, "AAudioStreamBuilder_delete")
           && load(library, api.streamRequestStart, "AAudioStream_requestStart")
           && load(library, api.streamRequestStop, "AAudioStream_requestStop")
           && load(library, api.streamClose, "AAudioStream_close")
           && load(library, api.streamRead, "AAudioStream_read")
           && load(library, api.streamWrite, "AAudioStream_write")
           && load(library, api.streamSetBufferSizeInFrames, "AAudioStream_setBufferSizeInFrames")
           && load(library, api.streamGetBufferCapacityInFrames, "AAudioStream_getBufferCapacityInFrames")
           && load(library, api.streamGetState, "AAudioStream_getState")
           && load(library, api.convertResultToText, "AAudio_convertResultToText");
}
} // namespace

const AAudioApi* AAudioApi::get()
{
    static const AAudioApi* const api = []() -> const AAudioApi* {
        // Never closed: the functions are used until qTox exits.
        void* const library = dlopen("libaaudio.so", RTLD_NOW);
        if (library == nullptr) {
            qInfo() << "No AAudio:" << dlerror();
            return nullptr;
        }
        static AAudioApi loaded;
        if (!load(library, loaded)) {
            dlclose(library);
            return nullptr;
        }
        return &loaded;
    }();
    return api;
}

#endif // Q_OS_ANDROID
