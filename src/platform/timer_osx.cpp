/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2006 by Richard Laager
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution (which can be found at
 * <https://hg.pidgin.im/pidgin/main/file/13e4ae613a6a/COPYRIGHT> ).
 */

#include "src/platform/timer.h" // IWYU pragma: keep, associated

#ifdef QTOX_PLATFORM_EXT
#include <QtCore/qsystemdetection.h>

#ifdef Q_OS_MACOS
#include <QDebug>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>

uint32_t Platform::getIdleTime()
{
    // https://hg.pidgin.im/pidgin/main/file/13e4ae613a6a/pidgin/gtkidle.c
    // relevant code introduced to Pidgin in:
    // https://hg.pidgin.im/pidgin/main/diff/8ff1c408ef3e/src/gtkidle.c
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
        return 0;
    }

    const auto* const property =
        IORegistryEntryCreateCFProperty(service, CFSTR("HIDIdleTime"), kCFAllocatorDefault, 0);
    uint64_t idleTime_ns = 0;
    CFNumberGetValue(static_cast<CFNumberRef>(property), kCFNumberSInt64Type, &idleTime_ns);
    CFRelease(property);

    return idleTime_ns / 1000000;
}
#endif
#endif
