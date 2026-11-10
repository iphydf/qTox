// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

package io.github.qtox.qtox;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;

/**
 * Plays calls as Android plays phone calls: on the earpiece unless the user wants the
 * loudspeaker, with other apps quiet meanwhile.
 */
public final class CallAudio
{
    // Since Android 8, while the user is in a call.
    private static AudioFocusRequest focus;

    private CallAudio() {}

    /** Called by the service's main(): whether the user is calling someone or talking. */
    public static void setInCall(Context context, boolean inCall)
    {
        AudioManager audio = context.getSystemService(AudioManager.class);
        if (inCall) {
            audio.setMode(AudioManager.MODE_IN_COMMUNICATION);
            requestFocus(audio);
        } else {
            setLoudspeaker(context, false);
            audio.setMode(AudioManager.MODE_NORMAL);
            abandonFocus(audio);
        }
    }

    /** Called by the service's main(): the loudspeaker, or the earpiece. */
    public static void setLoudspeaker(Context context, boolean on)
    {
        AudioManager audio = context.getSystemService(AudioManager.class);
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            audio.setSpeakerphoneOn(on);
            return;
        }
        AudioDeviceInfo speaker = null;
        for (AudioDeviceInfo device : audio.getAvailableCommunicationDevices()) {
            if (device.getType() == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER) {
                speaker = device;
            }
        }
        if (on && speaker != null) {
            audio.setCommunicationDevice(speaker);
        } else {
            audio.clearCommunicationDevice();
        }
    }

    private static void requestFocus(AudioManager audio)
    {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            audio.requestAudioFocus(null, AudioManager.STREAM_VOICE_CALL,
                                    AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
            return;
        }
        if (focus == null) {
            focus = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT)
                        .setAudioAttributes(new AudioAttributes.Builder()
                                                .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                                                .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                                                .build())
                        .build();
            audio.requestAudioFocus(focus);
        }
    }

    private static void abandonFocus(AudioManager audio)
    {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            audio.abandonAudioFocus(null);
            return;
        }
        if (focus != null) {
            audio.abandonAudioFocusRequest(focus);
            focus = null;
        }
    }
}
