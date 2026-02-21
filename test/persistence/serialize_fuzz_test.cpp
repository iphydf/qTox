/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/persistence/serialize.h"

#include <cstdlib>
#include <cstring>

namespace {

void test_dataToVInt(const uint8_t* data, size_t size)
{
    dataToVInt(QByteArray(reinterpret_cast<const char*>(data), size));
}

void test_vintToData(const uint8_t* data, size_t size)
{
    if (size < sizeof(int)) {
        return;
    }
    int num;
    memcpy(&num, data, sizeof(int));
    vintToData(num);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size == 0)
        return 0;
    switch (data[0] % 2) {
    case 0:
        test_dataToVInt(data + 1, size - 1);
        break;
    case 1:
        test_vintToData(data + 1, size - 1);
        break;
    default:
        break;
    }
    return 0;
}
