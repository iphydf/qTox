/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2016-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "capslock.h" // IWYU pragma: keep, associated

#ifdef QTOX_PLATFORM_EXT
#include <QtCore/qsystemdetection.h>

#ifdef Q_OS_MACOS
#include <QDebug>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>

bool Platform::capsLockEnabled()
{
    static io_service_t service{};

    if (service == 0) {
        mach_port_t main_port;
        if (__builtin_available(macOS 12.0, *)) {
            IOMainPort(MACH_PORT_NULL, &main_port);
        } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            IOMasterPort(MACH_PORT_NULL, &main_port);
#pragma clang diagnostic pop
        }
        auto* const mdict = IOServiceMatching(kIOHIDSystemClass);
        service = IOServiceGetMatchingService(main_port, mdict);
    }

    if (service == 0) {
        qWarning("IOServiceGetMatchingService() failed");
        return false;
    }

    io_connect_t ioc;
    auto kr = IOServiceOpen(service, mach_task_self(), kIOHIDParamConnectType, &ioc);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) {
        qWarning("IOServiceOpen() failed: %x", kr);
        return false;
    }

    bool state;
    kr = IOHIDGetModifierLockState(ioc, kIOHIDCapsLockState, &state);
    if (kr != KERN_SUCCESS) {
        IOServiceClose(ioc);
        qWarning("IOHIDGetModifierLockState() failed: %x", kr);
        return false;
    }
    IOServiceClose(ioc);
    return state;
}
#endif
#endif
