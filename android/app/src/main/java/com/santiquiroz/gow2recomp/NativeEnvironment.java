// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp;

import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;

import com.santiquiroz.gow2recomp.setup.GpuDriverHooks;
import com.santiquiroz.gow2recomp.setup.GpuDriverInstaller;

import java.io.File;
import java.io.IOException;

final class NativeEnvironment {
    private static final String TAG = "Gow2Activity";
    // The runtime reads it only when net_local_ip is empty, so a configured address still wins.
    private static final String LOCAL_IP_VAR = "REX_DETECTED_LOCAL_IP";
    // Read by android/gamelibs/gow2_gpu_driver.cpp when the game creates its Vulkan instance.
    private static final String DRIVER_DIR_VAR = "GOW2_GPU_DRIVER_DIR";
    private static final String DRIVER_NAME_VAR = "GOW2_GPU_DRIVER_NAME";
    private static final String HOOK_DIR_VAR = "GOW2_GPU_HOOK_DIR";

    private NativeEnvironment() {
    }

    static void apply(GamePaths paths) {
        set("HOME", paths.dataDir());
        // ReXGlue keeps per-user data in $XDG_DATA_HOME/<app name> (GetUserFolder, filesystem_posix.cpp).
        set("XDG_DATA_HOME", paths.dataDir());
        // Android has no /tmp; std::filesystem::temp_directory_path() reads TMPDIR.
        set("TMPDIR", paths.cacheDir());
        // The runtime keeps its config and logs beside the executable, which for an app is /system/bin.
        set("REX_EXECUTABLE_FOLDER", paths.dataDir());
        // SDL reads its hints from the environment. The runtime's window is resizable and names no
        // orientation, which makes SDL allow every orientation and the phone turns the game upright.
        set("SDL_ORIENTATIONS", "LandscapeLeft LandscapeRight");
        applyLocalIp();
    }

    /** Names the custom GPU driver of the launch options for the game, or the phone's own driver. */
    static void applyGpuDriver(File filesDir, File apk, boolean useSystemDriver) {
        GpuDriverInstaller.Driver driver = useSystemDriver ? null : GpuDriverInstaller.installed(filesDir);
        File hookDir = driver != null ? copyHooks(apk, filesDir) : null;
        if (hookDir == null) {
            unset(DRIVER_DIR_VAR);
            unset(DRIVER_NAME_VAR);
            unset(HOOK_DIR_VAR);
            return;
        }
        set(DRIVER_DIR_VAR, driver.dir.getAbsolutePath() + "/");
        set(DRIVER_NAME_VAR, driver.libraryName);
        set(HOOK_DIR_VAR, hookDir);
    }

    private static File copyHooks(File apk, File filesDir) {
        try {
            return GpuDriverHooks.copy(apk, filesDir);
        } catch (IOException | RuntimeException e) {
            Log.e(TAG, "cannot prepare the GPU driver hooks; using the phone's driver", e);
            return null;
        }
    }

    private static void unset(String name) {
        try {
            Os.unsetenv(name);
        } catch (ErrnoException e) {
            Log.e(TAG, "cannot unset " + name, e);
        }
    }

    private static void applyLocalIp() {
        String localIp = LocalNetworkAddress.detect();
        if (localIp != null) {
            set(LOCAL_IP_VAR, localIp);
        } else {
            Log.w(TAG, "no Wi-Fi or hotspot IPv4 address; System Link uses the runtime's default");
        }
    }

    private static void set(String name, File dir) {
        set(name, dir.getAbsolutePath());
    }

    private static void set(String name, String value) {
        try {
            Os.setenv(name, value, true);
            Log.i(TAG, name + "=" + value);
        } catch (ErrnoException e) {
            Log.e(TAG, "cannot set " + name, e);
        }
    }
}
