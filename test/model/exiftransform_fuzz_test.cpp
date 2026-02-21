/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/model/exiftransform.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

namespace {

void test_getOrientation(const uint8_t* data, size_t size)
{
    const ExifTransform::Orientation orientation =
        ExifTransform::getOrientation(QByteArray(reinterpret_cast<const char*>(data), size));
    assert(orientation >= ExifTransform::Orientation::TopLeft
           && orientation <= ExifTransform::Orientation::LeftBottom);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString&) {
        // Ignore messages
    });

    test_getOrientation(data, size);
    return 0;
}
