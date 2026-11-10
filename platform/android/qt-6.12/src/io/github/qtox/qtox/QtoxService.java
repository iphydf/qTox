// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

package io.github.qtox.qtox;

import android.Manifest;
import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import org.qtproject.qt.android.bindings.QtService;

/**
 * Keeps Tox connected while the user interface is closed, by running qTox's main() with
 * --service in its own process.
 */
public class QtoxService extends QtService
{
    private static final String SERVICE_CHANNEL_ID = "service";
    private static final String MESSAGES_CHANNEL_ID = "messages";
    private static final int SERVICE_NOTIFICATION_ID = 1;
    // The others have a tag each, and this ID.
    private static final int TAGGED_NOTIFICATION_ID = 0;

    // The service, once Android created it.
    private static QtoxService instance;
    // What the ongoing notification says.
    private static String status;

    /** Called by the user interface's main(). */
    public static void start(Activity activity)
    {
        Intent intent = new Intent(activity, QtoxService.class);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            activity.startForegroundService(intent);
        } else {
            activity.startService(intent);
        }

        // Android 13 and newer hide the service's notification without it.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
            && activity.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)
                   != PackageManager.PERMISSION_GRANTED) {
            activity.runOnUiThread(
                () -> activity.requestPermissions(new String[] {Manifest.permission.POST_NOTIFICATIONS}, 0));
        }
    }

    // What the user can do from a notification, as INotifier::Actions numbers them.
    private static final int REPLY = 1;
    private static final int ANSWER_OR_DECLINE = 2;
    // The page that answers the call: QmlApp's answerRequest.
    private static final String ANSWER = "answer";

    /** Called by NotificationReceiver: what the user wrote in a notification that let them. */
    static native void nativeReply(String tag, String text);

    /** Called by NotificationReceiver: the user declined the call. */
    static native void nativeDecline();

    /**
     * Called by the service's main(): replaces the notification with the same tag. Its actions
     * are what the user can do from it besides opening it.
     */
    public static void showNotification(Context context, String tag, String title, String text,
                                        byte[] icon, int actions)
    {
        Notification.Builder builder =
            newBuilder(context, MESSAGES_CHANNEL_ID, R.string.messages_channel,
                       NotificationManager.IMPORTANCE_HIGH)
                .setSmallIcon(R.drawable.qtox_logo)
                .setContentTitle(title)
                .setContentText(text)
                .setStyle(new Notification.BigTextStyle().bigText(text))
                .setCategory(Notification.CATEGORY_MESSAGE)
                .setAutoCancel(true)
                .setContentIntent(PendingIntent.getActivity(context, 0, QtoxActivity.open(context, tag),
                                                            PendingIntent.FLAG_IMMUTABLE));
        if (icon.length > 0) {
            builder.setLargeIcon(BitmapFactory.decodeByteArray(icon, 0, icon.length));
        }
        if (actions == REPLY) {
            builder.addAction(NotificationReceiver.reply(context, tag));
        } else if (actions == ANSWER_OR_DECLINE) {
            builder.setCategory(Notification.CATEGORY_CALL)
                .setAutoCancel(false)
                .setOngoing(true)
                .addAction(NotificationReceiver.decline(context))
                .addAction(new Notification.Action
                               .Builder(Icon.createWithResource(context, R.drawable.qtox_logo),
                                        context.getString(R.string.answer),
                                        PendingIntent.getActivity(context, 1,
                                                                  QtoxActivity.open(context, ANSWER),
                                                                  PendingIntent.FLAG_IMMUTABLE))
                               .build());
        }
        context.getSystemService(NotificationManager.class)
            .notify(tag, TAGGED_NOTIFICATION_ID, builder.build());
    }

    /** Called by the service's main(). */
    public static void cancelNotification(Context context, String tag)
    {
        context.getSystemService(NotificationManager.class).cancel(tag, TAGGED_NOTIFICATION_ID);
    }

    @Override public void onCreate()
    {
        instance = this;
        status = getString(R.string.service_starting);
        // Android stops a service started by startForegroundService() that isn't soon in the
        // foreground, and Qt takes a while to start.
        goForeground(false, false);
        super.onCreate();
    }

    @Override public void onDestroy()
    {
        instance = null;
        super.onDestroy();
    }

    private void goForeground(boolean microphone, boolean camera)
    {
        Notification notification = statusNotification(this, status);
        int recording = 0;
        if (microphone) {
            recording |= ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
        }
        if (camera) {
            recording |= ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            startForeground(SERVICE_NOTIFICATION_ID, notification,
                            ServiceInfo.FOREGROUND_SERVICE_TYPE_REMOTE_MESSAGING | recording);
        } else if (recording != 0 && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            startForeground(SERVICE_NOTIFICATION_ID, notification, recording);
        } else {
            startForeground(SERVICE_NOTIFICATION_ID, notification);
        }
    }

    /**
     * Called by the service's main(): Android 11 and newer let a foreground service record sound
     * and video only while it says so.
     */
    public static void setRecording(boolean microphone, boolean camera)
    {
        QtoxService service = instance;
        if (service == null) {
            return;
        }
        service.runOnMain(() -> {
            try {
                service.goForeground(microphone, camera);
                return;
            } catch (SecurityException e) {
                // Without the permission, or from the background: the friend doesn't see or hear
                // the user.
                Log.w("qTox", "Can't record: " + e.getMessage());
            }
            if (camera) {
                // The sound, at least.
                setRecording(microphone, false);
            }
        });
    }

    private void runOnMain(Runnable action)
    {
        new Handler(Looper.getMainLooper()).post(action);
    }

    /** Called by the service's main(): what the ongoing notification says. */
    public static void showStatus(Context context, String newStatus)
    {
        status = newStatus;
        context.getSystemService(NotificationManager.class)
            .notify(SERVICE_NOTIFICATION_ID, statusNotification(context, newStatus));
    }

    private static Notification statusNotification(Context context, String status)
    {
        return newBuilder(context, SERVICE_CHANNEL_ID, R.string.service_channel,
                          NotificationManager.IMPORTANCE_LOW)
            .setSmallIcon(R.drawable.qtox_logo)
            .setContentTitle(context.getApplicationInfo().loadLabel(context.getPackageManager()))
            .setContentText(status)
            .setContentIntent(PendingIntent.getActivity(context, 0,
                                                        new Intent(context, QtoxActivity.class),
                                                        PendingIntent.FLAG_IMMUTABLE))
            .setOngoing(true)
            // Android otherwise bundles it with the messages.
            .setGroup(SERVICE_CHANNEL_ID)
            .build();
    }

    private static Notification.Builder newBuilder(Context context, String channelId,
                                                   int channelName, int importance)
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            context.getSystemService(NotificationManager.class)
                .createNotificationChannel(
                    new NotificationChannel(channelId, context.getString(channelName), importance));
            return new Notification.Builder(context, channelId);
        }
        return new Notification.Builder(context);
    }
}
