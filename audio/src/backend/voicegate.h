/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QtGlobal>

#include <atomic>
#include <cstddef>
#include <cstdint>

/**
 * @brief Amplifies captured frames and decides which of them are sent: those at or above the
 *        threshold, and those that follow one within the hold time.
 *
 * The gain and the threshold may change on any thread.
 */
class VoiceGate
{
public:
    static constexpr qreal minGain = -30;
    static constexpr qreal maxGain = 30;
    static constexpr qreal minThreshold = 0.0;
    static constexpr qreal maxThreshold = 0.4;

    qreal gain() const;
    // In dB, between minGain and maxGain.
    void setGain(qreal dB);

    qreal threshold() const;
    // A normalised volume.
    void setThreshold(qreal normalisedThreshold);

    // Amplifies a frame lasting @p durationMs and returns its volume, between 0 and 1, or 0 while
    // the gate is closed.
    qreal process(int16_t* samples, size_t count, uint32_t durationMs);
    bool isOpen() const;

private:
    static constexpr uint32_t holdMs = 250;

    std::atomic<qreal> gain_{0};
    std::atomic<qreal> gainFactor_{1};
    std::atomic<qreal> threshold_{0};
    // How long ago the last frame at or above the threshold ended, up to the hold time.
    uint32_t quietMs = holdMs;
};
