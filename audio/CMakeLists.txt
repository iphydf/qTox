# cmake-format: off
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright © 2020 by The qTox Project Contributors
# Copyright © 2024-2025 The TokTok team
# cmake-format: on

set(SOURCE_FILES
    "include/audio/audio.h"
    "include/audio/iaudiocontrol.h"
    "include/audio/iaudiosettings.h"
    "include/audio/iaudiosink.h"
    "include/audio/iaudiosource.h"
    "src/iaudiosettings.cpp"
    "src/iaudiosink.cpp"
    "src/audio.cpp"
    "src/backend/android/aaudioapi.cpp"
    "src/backend/android/aaudioapi.h"
    "src/backend/android/androidaudio.cpp"
    "src/backend/android/androidaudio.h"
    "src/backend/android/androidsink.cpp"
    "src/backend/android/androidsink.h"
    "src/backend/android/androidsource.cpp"
    "src/backend/android/androidsource.h"
    "src/backend/alsink.cpp"
    "src/backend/alsink.h"
    "src/backend/alsource.cpp"
    "src/backend/alsource.h"
    "src/backend/openal.cpp"
    "src/backend/openal.h"
    "src/backend/voicegate.cpp"
    "src/backend/voicegate.h")

add_library(audio_library STATIC ${SOURCE_FILES} resources/audio_res.qrc)

# We need this directory, and users of our library will need it too
target_include_directories(audio_library PUBLIC include/)

target_link_libraries(audio_library PUBLIC util_library)
target_link_libraries(audio_library PUBLIC Qt6::Core)
target_link_libraries(audio_library PUBLIC ${OPENAL_LIBRARIES})
# AndroidAudio loads libaaudio.
target_link_libraries(audio_library PRIVATE ${CMAKE_DL_LIBS})
target_link_libraries(audio_library PRIVATE qtox::warnings)
