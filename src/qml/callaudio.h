/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

/**
 * @brief Where Android plays calls. Elsewhere the user chooses the devices in their system.
 */
namespace CallAudio {
// Whether the user is calling someone or talking: calls go to the earpiece meanwhile.
void setInCall(bool inCall);
// The loudspeaker instead of the earpiece, until the call ends.
void setLoudspeaker(bool on);
} // namespace CallAudio
