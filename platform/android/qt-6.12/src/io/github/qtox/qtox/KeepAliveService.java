// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

package io.github.qtox.qtox;

import android.app.Activity;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;

/**
 * Keeps Tox connected while other apps are in front, for the Widgets user interface, which runs
 * Tox in its own process: Android freezes an app in the background without a foreground service.
 * INotifier::showStatus() sets what its notification says.
 */
public class KeepAliveService extends Service
{
    /** Called by the user interface. */
    public static void start(Activity activity)
    {
        QtoxService.startInForeground(activity, KeepAliveService.class);
    }

    /** Called by the user interface. */
    public static void stop(Context context)
    {
        context.stopService(new Intent(context, KeepAliveService.class));
    }

    @Override public void onCreate()
    {
        super.onCreate();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            startForeground(QtoxService.SERVICE_NOTIFICATION_ID, QtoxService.statusNotification(this),
                            ServiceInfo.FOREGROUND_SERVICE_TYPE_REMOTE_MESSAGING);
        } else {
            startForeground(QtoxService.SERVICE_NOTIFICATION_ID, QtoxService.statusNotification(this));
        }
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId)
    {
        // Tox stopped with the process: nothing to restart without the user interface.
        return START_NOT_STICKY;
    }

    @Override public IBinder onBind(Intent intent)
    {
        return null;
    }
}
