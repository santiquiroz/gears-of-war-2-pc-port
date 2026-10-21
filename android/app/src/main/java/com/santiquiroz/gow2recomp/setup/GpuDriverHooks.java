// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp.setup;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * Copies libadrenotools' hook libraries out of the APK. The APK keeps its libraries inside the
 * archive (no extraction), and libadrenotools loads its hooks, and the C++ library they need, from
 * a real folder.
 */
public final class GpuDriverHooks {
    public static final String HOOK_FOLDER = "gpu_driver_hooks";
    static final String[] LIBRARIES = {"libhook_impl.so", "libmain_hook.so", "libc++_shared.so"};
    private static final String APK_LIBRARY_DIR = "lib/arm64-v8a/";

    private GpuDriverHooks() {
    }

    public static File copy(File apk, File filesDir) throws IOException {
        File hookDir = new File(filesDir, HOOK_FOLDER);
        Files.createDirectories(hookDir.toPath());
        try (ZipFile zip = new ZipFile(apk)) {
            for (String name : LIBRARIES) {
                copyOne(zip, name, hookDir);
            }
        }
        return hookDir;
    }

    private static void copyOne(ZipFile zip, String name, File hookDir) throws IOException {
        ZipEntry entry = zip.getEntry(APK_LIBRARY_DIR + name);
        if (entry == null) {
            throw new IOException(name + " is missing from the APK");
        }
        File target = new File(hookDir, name);
        if (target.length() == entry.getSize() && crc32(target) == entry.getCrc()) {
            return;
        }
        File temporary = new File(hookDir, name + ".tmp");
        try (InputStream in = zip.getInputStream(entry)) {
            Files.copy(in, temporary.toPath(), StandardCopyOption.REPLACE_EXISTING);
        }
        Files.move(temporary.toPath(), target.toPath(), StandardCopyOption.REPLACE_EXISTING);
    }

    private static long crc32(File file) throws IOException {
        CRC32 crc = new CRC32();
        crc.update(Files.readAllBytes(file.toPath()));
        return crc.getValue();
    }
}
