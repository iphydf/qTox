/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "android.h" // IWYU pragma: keep

#ifdef Q_OS_ANDROID
#include "src/video/videomode.h"

#include <QObject>

#include <algorithm>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <media/NdkImage.h>
#include <memory>

namespace {
// The most frames a second asked of a camera: FFmpeg's android_camera asks for 30 by default.
constexpr int32_t maxFps = 30;

/**
 * @brief Whether FFmpeg delivers frames of this width intact.
 *
 * FFmpeg 7.1's android_camera pads each plane's rows to 32 bytes, and its rawvideo decoder reads
 * planar YUV without padding, so only widths that need none come out straight: the chroma rows,
 * half as wide, too. Later FFmpeg doesn't pad them.
 */
bool unpadded(int32_t width)
{
    return width % 64 == 0;
}

struct ManagerDeleter
{
    void operator()(ACameraManager* manager) const
    {
        ACameraManager_delete(manager);
    }
};

struct IdListDeleter
{
    void operator()(ACameraIdList* ids) const
    {
        ACameraManager_deleteCameraIdList(ids);
    }
};

struct MetadataDeleter
{
    void operator()(ACameraMetadata* metadata) const
    {
        ACameraMetadata_free(metadata);
    }
};

using Manager = std::unique_ptr<ACameraManager, ManagerDeleter>;
using IdList = std::unique_ptr<ACameraIdList, IdListDeleter>;
using Metadata = std::unique_ptr<ACameraMetadata, MetadataDeleter>;

IdList cameraIds(ACameraManager* manager)
{
    ACameraIdList* ids = nullptr;
    if (ACameraManager_getCameraIdList(manager, &ids) != ACAMERA_OK) {
        return {};
    }
    return IdList{ids};
}

Metadata characteristics(ACameraManager* manager, const char* id)
{
    ACameraMetadata* metadata = nullptr;
    if (ACameraManager_getCameraCharacteristics(manager, id, &metadata) != ACAMERA_OK) {
        return {};
    }
    return Metadata{metadata};
}

// An acamera_metadata_enum_android_lens_facing_t, or -1.
int lensFacing(const ACameraMetadata* metadata)
{
    ACameraMetadata_const_entry entry{};
    if (ACameraMetadata_getConstEntry(metadata, ACAMERA_LENS_FACING, &entry) != ACAMERA_OK
        || entry.count == 0) {
        return -1;
    }
    return entry.data.u8[0];
}

QString describe(int facing)
{
    switch (facing) {
    case ACAMERA_LENS_FACING_FRONT:
        return QObject::tr("Front camera");
    case ACAMERA_LENS_FACING_BACK:
        return QObject::tr("Back camera");
    default:
        return QObject::tr("External camera");
    }
}

int order(int facing)
{
    switch (facing) {
    case ACAMERA_LENS_FACING_FRONT:
        return 0;
    case ACAMERA_LENS_FACING_BACK:
        return 1;
    default:
        return 2;
    }
}

// The camera's fastest frame rate up to maxFps.
float frameRate(const ACameraMetadata* metadata)
{
    ACameraMetadata_const_entry ranges{};
    if (ACameraMetadata_getConstEntry(metadata, ACAMERA_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, &ranges)
        != ACAMERA_OK) {
        return maxFps;
    }
    int32_t best = 0;
    // Each range is a minimum and a maximum.
    for (uint32_t i = 0; i + 1 < ranges.count; i += 2) {
        best = std::max(best, std::min(ranges.data.i32[i + 1], maxFps));
    }
    return best > 0 ? static_cast<float>(best) : static_cast<float>(maxFps);
}
} // namespace

QVector<QPair<QString, QString>> androidcamera::getDeviceList()
{
    const Manager manager{ACameraManager_create()};
    const IdList ids = cameraIds(manager.get());
    if (ids == nullptr) {
        return {};
    }

    struct Camera
    {
        int index;
        int facing;
    };
    QVector<Camera> cameras;
    for (int i = 0; i < ids->numCameras; ++i) {
        const Metadata metadata = characteristics(manager.get(), ids->cameraIds[i]);
        cameras.append({i, metadata == nullptr ? -1 : lensFacing(metadata.get())});
    }
    std::stable_sort(cameras.begin(), cameras.end(), [](const Camera& a, const Camera& b) {
        return order(a.facing) < order(b.facing);
    });

    QVector<QPair<QString, QString>> devices;
    for (const Camera& camera : cameras) {
        devices.append({QString::number(camera.index), describe(camera.facing)});
    }
    return devices;
}

QVector<VideoMode> androidcamera::getDeviceModes(const QString& devName)
{
    const Manager manager{ACameraManager_create()};
    const IdList ids = cameraIds(manager.get());
    bool isIndex = false;
    const int index = devName.toInt(&isIndex);
    if (ids == nullptr || !isIndex || index < 0 || index >= ids->numCameras) {
        return {};
    }
    const Metadata metadata = characteristics(manager.get(), ids->cameraIds[index]);
    if (metadata == nullptr) {
        return {};
    }

    ACameraMetadata_const_entry configs{};
    if (ACameraMetadata_getConstEntry(metadata.get(),
                                      ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &configs)
        != ACAMERA_OK) {
        return {};
    }
    const float fps = frameRate(metadata.get());
    QVector<VideoMode> modes;
    // Each configuration is a format, a width, a height and whether it's an input.
    for (uint32_t i = 0; i + 3 < configs.count; i += 4) {
        const int32_t* config = configs.data.i32 + i;
        if (config[0] == AIMAGE_FORMAT_YUV_420_888
            && config[3] == ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT
            && unpadded(config[1])) {
            modes.append(VideoMode{config[1], config[2], 0, 0, fps});
        }
    }
    return modes;
}
#endif
