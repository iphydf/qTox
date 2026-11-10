// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

package io.github.qtox.qtox;

import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.graphics.Insets;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsAnimation;
import androidx.core.content.FileProvider;
import java.io.File;
import java.util.List;
import org.qtproject.qt.android.bindings.QtActivity;

/**
 * The user interface. Tox and everything worth saving live in QtoxService's process.
 */
public class QtoxActivity extends QtActivity
{
    private static final String DESTINATION_SCHEME = "qtox";

    // What the system covers at the window's edges beyond Qt's safe area, in pixels: left, top,
    // right, bottom.
    private static final Object insetsLock = new Object();
    private static int[] insets = {0, 0, 0, 0};
    private static boolean insetsWanted = false;

    // Its insets come from the animation while the keyboard opens or closes.
    private boolean keyboardAnimating = false;

    private static native void insetsChanged(int left, int top, int right, int bottom);

    /**
     * Opens the user interface at a page: a remotemodels.h model name.
     *
     * The name is the intent's data, so that each page's PendingIntent is a different one.
     */
    static Intent open(Context context, String destination)
    {
        return new Intent(context, QtoxActivity.class)
            .setData(Uri.fromParts(DESTINATION_SCHEME, destination, null));
    }

    /** Called by the user interface's main(): the page open() asked for, or null. */
    public static String destination(Intent intent)
    {
        Uri data = intent.getData();
        if (data == null || !DESTINATION_SCHEME.equals(data.getScheme()) || isOld(intent)) {
            return null;
        }
        return data.getSchemeSpecificPart();
    }

    /** Called by the user interface's main(): the file that another app shared, or null. */
    public static String sharedFile(Intent intent)
    {
        Uri file = sharedStream(intent);
        return file != null ? file.toString() : null;
    }

    /** Called by the user interface's main(): the text that another app shared, or null. */
    public static String sharedText(Intent intent)
    {
        if (!Intent.ACTION_SEND.equals(intent.getAction()) || isOld(intent)
            || sharedStream(intent) != null) {
            return null;
        }
        CharSequence text = intent.getCharSequenceExtra(Intent.EXTRA_TEXT);
        return text != null ? text.toString() : null;
    }

    private static Uri sharedStream(Intent intent)
    {
        if (!Intent.ACTION_SEND.equals(intent.getAction()) || isOld(intent)) {
            return null;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            return intent.getParcelableExtra(Intent.EXTRA_STREAM, Uri.class);
        }
        return intent.getParcelableExtra(Intent.EXTRA_STREAM);
    }

    // Android reuses the first intent when the user reopens qTox from the recent apps.
    private static boolean isOld(Intent intent)
    {
        return (intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0;
    }

    /**
     * Called by the user interface: opens a file of the app's in the app for its type.
     *
     * Android refuses file: URIs to other apps, so the file goes through the FileProvider that
     * Qt's manifest declares.
     *
     * @return Whether an app opens it.
     */
    public static boolean openFile(Context context, String path)
    {
        try {
            Uri uri = providerUri(context, path);
            context.startActivity(new Intent(Intent.ACTION_VIEW)
                                      .setDataAndType(uri, context.getContentResolver().getType(uri))
                                      .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                                                | Intent.FLAG_ACTIVITY_NEW_TASK));
            return true;
        } catch (IllegalArgumentException | ActivityNotFoundException e) {
            // Not shared by the FileProvider, or no app for its type.
            return false;
        }
    }

    /**
     * Called by the user interface: lets the user choose an app to send a file of the app's to.
     *
     * @return Whether the FileProvider shares the file.
     */
    public static boolean shareFile(Context context, String path)
    {
        try {
            Uri uri = providerUri(context, path);
            Intent send = new Intent(Intent.ACTION_SEND)
                              .setType(context.getContentResolver().getType(uri))
                              .putExtra(Intent.EXTRA_STREAM, uri)
                              .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            // Through the chooser, Android grants what the ClipData names.
            send.setClipData(ClipData.newRawUri(null, uri));
            context.startActivity(Intent.createChooser(send, null)
                                      .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                                                | Intent.FLAG_ACTIVITY_NEW_TASK));
            return true;
        } catch (IllegalArgumentException e) {
            // Not shared by the FileProvider.
            return false;
        }
    }

    private static Uri providerUri(Context context, String path)
    {
        return FileProvider.getUriForFile(context, context.getPackageName() + ".qtprovider",
                                          new File(path));
    }

    /** Lets the user choose an app to send the text to. */
    public static void share(Context context, String text)
    {
        Intent send =
            new Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, text);
        context.startActivity(Intent.createChooser(send, null).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
    }

    /**
     * Called by the user interface: whether it takes insetsChanged() calls, starting with the
     * insets now.
     */
    public static void sendInsets(boolean wanted)
    {
        synchronized (insetsLock) {
            insetsWanted = wanted;
            if (wanted) {
                insetsChanged(insets[0], insets[1], insets[2], insets[3]);
            }
        }
    }

    private static void setInsets(int left, int top, int right, int bottom)
    {
        synchronized (insetsLock) {
            insets = new int[] {left, top, right, bottom};
            if (insetsWanted) {
                insetsChanged(left, top, right, bottom);
            }
        }
    }

    private static void setInsets(Insets covered)
    {
        setInsets(covered.left, covered.top, covered.right, covered.bottom);
    }

    @Override public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        // The window keeps its size when the keyboard opens: Qt draws a resized window a few
        // frames late, and shows the old frame stretched meanwhile.
        View content = findViewById(android.R.id.content);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            followInsets(content);
        } else {
            followKeyboard(content);
        }
    }

    /** Follows the keyboard as it moves. */
    private void followInsets(View content)
    {
        // Under the system's bars, as Qt draws. Qt does this only up to Android 15: newer Android
        // does it itself, but only for apps that target Android 15 or newer, and qTox doesn't.
        getWindow().setDecorFitsSystemWindows(false);
        // Android gives the keyboard's final insets when it starts moving.
        content.setOnApplyWindowInsetsListener((view, windowInsets) -> {
            if (!keyboardAnimating) {
                setInsets(beyondSafeArea(windowInsets));
            }
            return windowInsets;
        });
        content.setWindowInsetsAnimationCallback(
            new WindowInsetsAnimation.Callback(WindowInsetsAnimation.Callback.DISPATCH_MODE_STOP) {
                @Override public void onPrepare(WindowInsetsAnimation animation)
                {
                    if ((animation.getTypeMask() & WindowInsets.Type.ime()) != 0) {
                        keyboardAnimating = true;
                    }
                }

                @Override
                public WindowInsets onProgress(WindowInsets windowInsets,
                                               List<WindowInsetsAnimation> animations)
                {
                    if (keyboardAnimating) {
                        setInsets(beyondSafeArea(windowInsets));
                    }
                    return windowInsets;
                }

                @Override public void onEnd(WindowInsetsAnimation animation)
                {
                    if ((animation.getTypeMask() & WindowInsets.Type.ime()) != 0) {
                        keyboardAnimating = false;
                        setInsets(beyondSafeArea(content.getRootWindowInsets()));
                    }
                }
            });
    }

    /** What the keyboard covers beyond Qt's safe area, which is clear of bars and cutouts. */
    private static Insets beyondSafeArea(WindowInsets windowInsets)
    {
        final int safeArea = WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout();
        final Insets covered = windowInsets.getInsets(safeArea | WindowInsets.Type.ime());
        return Insets.max(Insets.subtract(covered, windowInsets.getInsets(safeArea)), Insets.NONE);
    }

    /** Before Android 11, which doesn't animate insets: where the keyboard ends up. */
    @SuppressWarnings("deprecation") private void followKeyboard(View content)
    {
        content.getViewTreeObserver().addOnGlobalLayoutListener(() -> {
            Rect visible = new Rect();
            content.getWindowVisibleDisplayFrame(visible);
            int[] location = new int[2];
            content.getLocationOnScreen(location);
            // The navigation bar is in Qt's safe area.
            WindowInsets windowInsets = content.getRootWindowInsets();
            int bar = windowInsets != null ? windowInsets.getStableInsetBottom() : 0;
            setInsets(0, 0, 0, Math.max(0, location[1] + content.getHeight() - visible.bottom - bar));
        });
    }

    @Override protected void onDestroy()
    {
        // Qt's onDestroy() waits for main() to return, which it never does when Android removes
        // the activity while the user interface is running, and then exit() aborts in Android's
        // renderer.
        if (!isChangingConfigurations()) {
            Process.killProcess(Process.myPid());
        }
        super.onDestroy();
    }
}
