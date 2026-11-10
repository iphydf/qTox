/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "audio/src/backend/voicegate.h"

#include <QTest>

#include <array>

namespace {
constexpr uint32_t frameMs = 20;

// A frame of a constant level.
std::array<int16_t, 960> frame(int16_t level)
{
    std::array<int16_t, 960> samples;
    samples.fill(level);
    return samples;
}

qreal process(VoiceGate& gate, int16_t level)
{
    auto samples = frame(level);
    return gate.process(samples.data(), samples.size(), frameMs);
}
} // namespace

class TestVoiceGate : public QObject
{
    Q_OBJECT
private slots:
    void gainAmplifiesAndClips();
    void openWithoutThreshold();
    void holdsAfterVoice();
};

void TestVoiceGate::gainAmplifiesAndClips()
{
    VoiceGate gate;
    gate.setGain(20);
    auto samples = frame(1000);
    gate.process(samples.data(), samples.size(), frameMs);
    QCOMPARE(samples[0], 10000);

    samples = frame(10000);
    gate.process(samples.data(), samples.size(), frameMs);
    QCOMPARE(samples[0], std::numeric_limits<int16_t>::max());

    gate.setGain(100);
    QCOMPARE(gate.gain(), VoiceGate::maxGain);
}

void TestVoiceGate::openWithoutThreshold()
{
    VoiceGate gate;
    QCOMPARE(process(gate, 0), 0.0);
    QVERIFY(gate.isOpen());
}

void TestVoiceGate::holdsAfterVoice()
{
    VoiceGate gate;
    gate.setThreshold(0.1);
    QCOMPARE(process(gate, 100), 0.0);
    QVERIFY(!gate.isOpen());

    QVERIFY(process(gate, 10000) > 0.1);
    QVERIFY(gate.isOpen());
    // Quiet frames go out for 250 ms after the voice, with their volume.
    for (int i = 0; i < 12; ++i) {
        QVERIFY(process(gate, 100) > 0);
        QVERIFY(gate.isOpen());
    }
    QCOMPARE(process(gate, 100), 0.0);
    QVERIFY(!gate.isOpen());
}

QTEST_GUILESS_MAIN(TestVoiceGate)
#include "voicegate_test.moc"
