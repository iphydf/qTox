/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include "util/interface.h"

#include <QRect>
#include <QString>

class IVideoSettings
{
public:
    IVideoSettings() = default;
    virtual ~IVideoSettings();
    IVideoSettings(const IVideoSettings&) = default;
    IVideoSettings& operator=(const IVideoSettings&) = default;
    IVideoSettings(IVideoSettings&&) = default;
    IVideoSettings& operator=(IVideoSettings&&) = default;

    virtual QString getVideoDev() const = 0;
    virtual void setVideoDev(const QString& deviceSpecifier) = 0;

    virtual QRect getScreenRegion() const = 0;
    virtual void setScreenRegion(const QRect& value) = 0;

    virtual bool getScreenGrabbed() const = 0;
    virtual void setScreenGrabbed(bool value) = 0;

    virtual QRect getCamVideoRes() const = 0;
    virtual void setCamVideoRes(QRect newValue) = 0;

    virtual float getCamVideoFPS() const = 0;
    virtual void setCamVideoFPS(float newValue) = 0;

    DECLARE_SIGNAL(videoDevChanged, const QString& device);
    DECLARE_SIGNAL(screenRegionChanged, const QRect& region);
    DECLARE_SIGNAL(screenGrabbedChanged, bool enabled);
    DECLARE_SIGNAL(camVideoResChanged, const QRect& region);
    DECLARE_SIGNAL(camVideoFPSChanged, unsigned short fps);
};
