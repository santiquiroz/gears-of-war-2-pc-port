// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp.setup;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Installs a custom Vulkan driver package in the AdrenoTools format (a .zip holding meta.json and
 * the driver library, as Turnip builds are published) into the app's private files folder, where
 * the game can load it.
 */
public final class GpuDriverInstaller {
    public static final String DRIVER_FOLDER = "gpu_driver";
    private static final String STAGING_FOLDER = "gpu_driver.incoming";
    private static final String META = "meta.json";
    private static final int MAX_ENTRIES = 64;
    private static final long MAX_BYTES = 256L * 1024 * 1024;
    private static final int BUFFER_SIZE = 64 * 1024;

    /** An installed driver: its folder, the library to load from it and a name to show. */
    public static final class Driver {
        public final File dir;
        public final String libraryName;
        public final String label;

        Driver(File dir, String libraryName, String label) {
            this.dir = dir;
            this.libraryName = libraryName;
            this.label = label;
        }
    }

    private GpuDriverInstaller() {
    }

    public static Driver installed(File filesDir) {
        try {
            return read(new File(filesDir, DRIVER_FOLDER));
        } catch (IOException | GameSetupException e) {
            return null;
        }
    }

    public static Driver install(InputStream zip, File filesDir) throws IOException, GameSetupException {
        File staging = new File(filesDir, STAGING_FOLDER);
        try {
            GameInstaller.prepareStagingDir(staging);
            extract(zip, staging);
            read(staging);
            GameInstaller.replaceGameFolder(new File(filesDir, DRIVER_FOLDER), staging);
        } catch (IOException | GameSetupException | RuntimeException e) {
            GameInstaller.deleteQuietly(staging);
            throw e;
        }
        return read(new File(filesDir, DRIVER_FOLDER));
    }

    public static void remove(File filesDir) {
        GameInstaller.deleteQuietly(new File(filesDir, DRIVER_FOLDER));
    }

    private static void extract(InputStream zip, File target) throws IOException, GameSetupException {
        long total = 0L;
        int entries = 0;
        byte[] buffer = new byte[BUFFER_SIZE];
        try (ZipInputStream in = new ZipInputStream(zip)) {
            for (ZipEntry entry = in.getNextEntry(); entry != null; entry = in.getNextEntry()) {
                if (entry.isDirectory()) {
                    continue;
                }
                if (++entries > MAX_ENTRIES || !SafeName.isSafe(entry.getName())) {
                    throw notADriver();
                }
                total = copyEntry(in, new File(target, entry.getName()), total, buffer);
            }
        }
    }

    private static long copyEntry(InputStream in, File file, long total, byte[] buffer)
            throws IOException, GameSetupException {
        try (FileOutputStream out = new FileOutputStream(file)) {
            for (int read = in.read(buffer); read > 0; read = in.read(buffer)) {
                total += read;
                if (total > MAX_BYTES) {
                    throw notADriver();
                }
                out.write(buffer, 0, read);
            }
        }
        return total;
    }

    private static Driver read(File dir) throws IOException, GameSetupException {
        File meta = new File(dir, META);
        if (!meta.isFile()) {
            throw notADriver();
        }
        try {
            JSONObject json = new JSONObject(new String(Files.readAllBytes(meta.toPath()), StandardCharsets.UTF_8));
            String library = json.getString("libraryName");
            if (!SafeName.isSafe(library) || !library.endsWith(".so") || !new File(dir, library).isFile()) {
                throw notADriver();
            }
            String label = (json.optString("name", library) + " " + json.optString("driverVersion", "")).trim();
            return new Driver(dir, library, label);
        } catch (JSONException e) {
            throw notADriver();
        }
    }

    private static GameSetupException notADriver() {
        return new GameSetupException("This is not a GPU driver package: choose a .zip with meta.json and"
                + " the driver library at its top level (for example a Turnip build for AdrenoTools).");
    }
}
