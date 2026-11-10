/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "audio/src/backend/voicegate.h"

#include <QtMath>

#include <cmath>
#include <limits>

namespace {
void applyGain(int16_t* buffer, size_t bufferSize, qreal gainFactor)
{
    for (size_t i = 0; i < bufferSize; ++i) {
        // gain amplification with clipping to 16-bit boundaries
        buffer[i] =
            qBound<int16_t>(std::numeric_limits<int16_t>::min(), qRound(buffer[i] * gainFactor),
                            std::numeric_limits<int16_t>::max());
    }
}

/**
 * @brief Calculates the volume of an audio buffer
 *
 * @return normalized volume between 0-1
 */
qreal getVolume(const int16_t* buffer, size_t samples)
{
    constexpr qreal sqrt2 = 1.41421356237; // std::numbers::sqrt2
    // calculate volume as the root mean squared of amplitudes in the sample
    qreal sumOfSquares = 0;
    for (size_t i = 0; i < samples; i++) {
        auto sample = static_cast<qreal>(buffer[i]) / std::numeric_limits<int16_t>::max();
        sumOfSquares += std::pow(sample, 2);
    }
    const qreal rms = std::sqrt(sumOfSquares / samples);
    // our calculated normalized volume could possibly be above 1 because our RMS assumes a sinusoidal wave
    const qreal normalizedVolume = std::min(rms * sqrt2, 1.0);
    return normalizedVolume;
}
} // namespace

qreal VoiceGate::gain() const
{
    return gain_;
}

void VoiceGate::setGain(qreal dB)
{
    const qreal bounded = qBound(minGain, dB, maxGain);
    gain_ = bounded;
    gainFactor_ = qPow(10.0, (bounded / 20.0));
}

qreal VoiceGate::threshold() const
{
    return threshold_;
}

void VoiceGate::setThreshold(qreal normalisedThreshold)
{
    threshold_ = normalisedThreshold;
}

qreal VoiceGate::process(int16_t* samples, size_t count, uint32_t durationMs)
{
    applyGain(samples, count, gainFactor_);

    const qreal volume = getVolume(samples, count);
    if (volume >= threshold_) {
        quietMs = 0;
        return volume;
    }
    if (quietMs < holdMs) {
        quietMs += durationMs;
    }
    return isOpen() ? volume : 0;
}

bool VoiceGate::isOpen() const
{
    return quietMs < holdMs;
}
