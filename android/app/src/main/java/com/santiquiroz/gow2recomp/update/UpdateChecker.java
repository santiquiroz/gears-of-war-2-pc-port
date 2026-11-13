// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp.update;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

/**
 * Checks the GitHub Releases feed for a newer APK. Checks run on a background thread and
 * never delay the game; at most one fetch happens every 12 hours, and any offline or
 * server error keeps the last stored result silently.
 */
public final class UpdateChecker {
    static final String RELEASE_URL =
            "https://api.github.com/repos/santiquiroz/gears-of-war-2-pc-port/releases/latest";
    static final long CHECK_INTERVAL_MS = 12L * 60 * 60 * 1000;
    static final int TIMEOUT_MS = 8000;

    private static final String TAG = "UpdateChecker";
    private static final String PREFS_NAME = "gow2_update_check";
    private static final String KEY_CHECKED_AT_MS = "checked_at_ms";
    private static final String KEY_TAG = "tag";
    private static final String KEY_PAGE_URL = "page_url";
    private static final String KEY_APK_URL = "apk_url";
    private static final String KEY_SKIPPED_TAG = "skipped_tag";
    private static final int MAX_BODY_BYTES = 256 * 1024;
    private static final Object LOCK = new Object();

    /** Receives a forced-check result on a background thread; post to the UI thread first. */
    public interface Listener {
        void onResult(boolean ok, UpdateInfo info, boolean newer);
    }

    private UpdateChecker() {
    }

    /** Fetches the latest release in a background thread when the last check is over 12 h old. */
    public static void refreshIfStale(Context context, String currentVersion) {
        if (System.currentTimeMillis() - prefs(context).getLong(KEY_CHECKED_AT_MS, 0)
                < CHECK_INTERVAL_MS) {
            return;
        }
        refreshInBackground(context, currentVersion, null);
    }

    /** Fetches the latest release in a background thread, ignoring the 12 h throttle. */
    public static void refreshNow(Context context, String currentVersion, Listener listener) {
        refreshInBackground(context, currentVersion, listener);
    }

    /**
     * Returns the stored release when it is newer than {@code currentVersion}, carries an APK
     * and was not skipped, otherwise null.
     */
    public static UpdateInfo pendingUpdate(Context context, String currentVersion) {
        SharedPreferences prefs = prefs(context);
        String tag = prefs.getString(KEY_TAG, null);
        if (tag == null || tag.isEmpty() || tag.equals(prefs.getString(KEY_SKIPPED_TAG, null))) {
            return null;
        }
        String pageUrl = prefs.getString(KEY_PAGE_URL, null);
        if (pageUrl == null || pageUrl.isEmpty()) {
            return null;
        }
        UpdateInfo stored = new UpdateInfo(tag, pageUrl, prefs.getString(KEY_APK_URL, null));
        return stored.isUpdateFor(currentVersion) ? stored : null;
    }

    /** Remembers that the user dismissed this tag; a newer tag still shows again. */
    public static void skipVersion(Context context, String tag) {
        prefs(context).edit().putString(KEY_SKIPPED_TAG, tag).apply();
    }

    private static void refreshInBackground(
            Context context, String currentVersion, Listener listener) {
        Context app = context.getApplicationContext();
        Thread thread = new Thread(() -> check(app, currentVersion, listener));
        thread.setDaemon(true);
        thread.start();
    }

    private static void check(Context context, String currentVersion, Listener listener) {
        boolean ok = false;
        UpdateInfo info = null;
        try {
            synchronized (LOCK) {
                info = download(currentVersion);
            }
            ok = true;
            if (info != null) {
                storeRelease(context, info);
            }
            prefs(context).edit().putLong(KEY_CHECKED_AT_MS, System.currentTimeMillis()).apply();
        } catch (IOException | RuntimeException e) {
            Log.i(TAG, "update check failed, keeping the last result", e);
        }
        if (listener != null) {
            try {
                listener.onResult(ok, info, info != null && info.isUpdateFor(currentVersion));
            } catch (RuntimeException e) {
                Log.w(TAG, "update listener failed", e);
            }
        }
    }

    private static UpdateInfo download(String currentVersion) throws IOException {
        HttpURLConnection connection = (HttpURLConnection) new URL(RELEASE_URL).openConnection();
        connection.setConnectTimeout(TIMEOUT_MS);
        connection.setReadTimeout(TIMEOUT_MS);
        connection.setRequestProperty("User-Agent", "gow2-recomp-android/" + currentVersion);
        connection.setRequestProperty("Accept", "application/vnd.github+json");
        try {
            if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) {
                throw new IOException("unexpected status " + connection.getResponseCode());
            }
            return UpdateInfo.parse(readBody(connection));
        } finally {
            connection.disconnect();
        }
    }

    private static String readBody(HttpURLConnection connection) throws IOException {
        try (InputStream in = connection.getInputStream();
                ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[8192];
            int total = 0;
            int read;
            while ((read = in.read(buffer)) != -1) {
                total += read;
                if (total > MAX_BODY_BYTES) {
                    throw new IOException("release body is too large");
                }
                out.write(buffer, 0, read);
            }
            return new String(out.toByteArray(), StandardCharsets.UTF_8);
        }
    }

    private static void storeRelease(Context context, UpdateInfo info) {
        SharedPreferences.Editor editor = prefs(context).edit();
        editor.putString(KEY_TAG, info.tag());
        editor.putString(KEY_PAGE_URL, info.pageUrl());
        if (info.apkUrl() != null) {
            editor.putString(KEY_APK_URL, info.apkUrl());
        } else {
            editor.remove(KEY_APK_URL);
        }
        editor.apply();
    }

    private static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
    }
}
