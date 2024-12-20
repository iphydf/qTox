/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014 Thilo Borgmann <thilo.borgmann@mail.de>
 * Copyright © 2015-2019 by The qTox Project Contributors
 * Copyright © 2024 The TokTok team.
 */

#include "avfoundation.h"

#include <QDebug>
#include <QMutex>
#include <QObject>

#import <AVFoundation/AVFoundation.h>

QVector<QPair<QString, QString>> avfoundation::getDeviceList()
{
    QVector<QPair<QString, QString>> result;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
    const AVAuthorizationStatus authStatus =
        [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if (authStatus != AVAuthorizationStatusDenied && authStatus != AVAuthorizationStatusNotDetermined) {
        qDebug() << "We already have access to the camera.";
    } else {
        qDebug() << "We don't have access to the camera yet; asking user for permission.";
        QMutex mutex;
        QMutex* mutexPtr = &mutex;
        __block BOOL isGranted = false;
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
                                 completionHandler:^(BOOL granted) {
                                   isGranted = granted;
                                   mutexPtr->unlock();
                                 }];
        mutex.lock();
        if (isGranted) {
            qInfo() << "We now have access to the camera.";
        } else {
            qInfo() << "User did not grant us permission to access the camera.";
        }
    }
#endif

    AVCaptureDevice* device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    id objects[] = {device};
    NSUInteger count = sizeof(objects) / sizeof(id);
    NSArray* devices = [NSArray arrayWithObjects:objects count:count];

    for (AVCaptureDevice* device in devices) {
        result.append({QString::fromNSString([device uniqueID]),
                       QString::fromNSString([device localizedName])});
    }

    uint32_t numScreens = 0;
    CGGetActiveDisplayList(0, nullptr, &numScreens);
    if (numScreens > 0) {
        CGDirectDisplayID screens[numScreens];
        CGGetActiveDisplayList(numScreens, screens, &numScreens);
        for (uint32_t i = 0; i < numScreens; i++) {
            result.append({QString("%1 %2").arg(CAPTURE_SCREEN).arg(i),
                           QObject::tr("Capture screen %1").arg(i)});
        }
    }

    return result;
}

QVector<VideoMode> avfoundation::getDeviceModes(QString devName)
{
    QVector<VideoMode> result;

    if (devName.startsWith(CAPTURE_SCREEN)) {
        return result;
    } else {
        NSString* deviceName = [NSString stringWithCString:devName.toUtf8().constData()
                                                  encoding:NSUTF8StringEncoding];
        AVCaptureDevice* device = [AVCaptureDevice deviceWithUniqueID:deviceName];

        if (device == nil) {
            return result;
        }

        for (AVCaptureDeviceFormat* format in [device formats]) {
            CMFormatDescriptionRef formatDescription;
            CMVideoDimensions dimensions;
            formatDescription = static_cast<CMFormatDescriptionRef>(
                [format performSelector:@selector(formatDescription)]);
            dimensions = CMVideoFormatDescriptionGetDimensions(formatDescription);

            for (AVFrameRateRange* range in format.videoSupportedFrameRateRanges) {
                VideoMode mode;
                mode.width = dimensions.width;
                mode.height = dimensions.height;
                mode.FPS = range.maxFrameRate;
                result.append(mode);
            }
        }
    }

    return result;
}
