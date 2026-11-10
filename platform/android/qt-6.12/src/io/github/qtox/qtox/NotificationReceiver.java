// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

package io.github.qtox.qtox;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.RemoteInput;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.util.Log;

/**
 * Does in the process that runs Tox what the user chose in a notification: a reply to a message,
 * or declining a call.
 */
public class NotificationReceiver extends BroadcastReceiver
{
    private static final String REPLY = "io.github.qtox.qtox.REPLY";
    private static final String DECLINE = "io.github.qtox.qtox.DECLINE";
    private static final String TAG_EXTRA = "tag";
    private static final String TEXT_KEY = "text";

    /** Gets what the user wrote in the notification with that tag. */
    static native void nativeReply(String tag, String text);

    /** Gets that the user declined the call that the notification with that tag is about. */
    static native void nativeDecline(String tag);

    /** The receiver in the process that runs Tox: the service's, if this is it. */
    private static Class<? extends NotificationReceiver> inThisProcess()
    {
        return QtoxService.runsHere() ? ServiceNotificationReceiver.class : NotificationReceiver.class;
    }

    /** A message notification's Reply. */
    static Notification.Action reply(Context context, String tag)
    {
        Intent intent = new Intent(context, inThisProcess()).setAction(REPLY).putExtra(TAG_EXTRA, tag);
        // Mutable: Android adds what the user wrote. One per chat.
        PendingIntent pending = PendingIntent.getBroadcast(context, tag.hashCode(), intent,
                                                           PendingIntent.FLAG_UPDATE_CURRENT
                                                               | PendingIntent.FLAG_MUTABLE);
        RemoteInput input =
            new RemoteInput.Builder(TEXT_KEY).setLabel(context.getString(R.string.reply)).build();
        return new Notification.Action
            .Builder(Icon.createWithResource(context, R.drawable.qtox_logo),
                     context.getString(R.string.reply), pending)
            .addRemoteInput(input)
            .build();
    }

    /** A call notification's Decline. */
    static Notification.Action decline(Context context, String tag)
    {
        Intent intent =
            new Intent(context, inThisProcess()).setAction(DECLINE).putExtra(TAG_EXTRA, tag);
        PendingIntent pending = PendingIntent.getBroadcast(context, tag.hashCode(), intent,
                                                           PendingIntent.FLAG_UPDATE_CURRENT
                                                               | PendingIntent.FLAG_IMMUTABLE);
        return new Notification.Action
            .Builder(Icon.createWithResource(context, R.drawable.qtox_logo),
                     context.getString(R.string.decline), pending)
            .build();
    }

    @Override public void onReceive(Context context, Intent intent)
    {
        try {
            if (REPLY.equals(intent.getAction())) {
                Bundle results = RemoteInput.getResultsFromIntent(intent);
                String tag = intent.getStringExtra(TAG_EXTRA);
                CharSequence text = results != null ? results.getCharSequence(TEXT_KEY) : null;
                if (tag != null && text != null) {
                    nativeReply(tag, text.toString());
                }
            } else if (DECLINE.equals(intent.getAction())) {
                String tag = intent.getStringExtra(TAG_EXTRA);
                if (tag != null) {
                    nativeDecline(tag);
                }
            }
        } catch (UnsatisfiedLinkError e) {
            // Android started the process for this alone, without qTox running Tox in it.
            Log.w("qTox", "Tox isn't running");
        }
    }
}
