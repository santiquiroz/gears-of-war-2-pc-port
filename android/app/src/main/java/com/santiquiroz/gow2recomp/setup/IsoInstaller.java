// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp.setup;

import android.content.ContentResolver;
import android.net.Uri;
import android.os.ParcelFileDescriptor;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Extracts the player's own Xbox 360 disc image (.iso) into the app's game folder. */
public final class IsoInstaller {
    private static final int BUFFER_SIZE = 1024 * 1024;

    private IsoInstaller() {
    }

    public static void install(ContentResolver resolver, Uri isoUri, File gameDir, File stagingDir,
            ProgressListener listener, AtomicBoolean cancelled) throws IOException, GameSetupException {
        try (ParcelFileDescriptor descriptor = resolver.openFileDescriptor(isoUri, "r")) {
            if (descriptor == null) {
                throw new IOException("cannot open the chosen file");
            }
            try (FileInputStream input = new FileInputStream(descriptor.getFileDescriptor());
                    FileChannel channel = input.getChannel()) {
                XdvdfsImage image = XdvdfsImage.open(channelReader(channel), channel.size());
                extract(image, gameDir, stagingDir, listener, cancelled);
            }
        }
    }

    static void extract(XdvdfsImage image, File gameDir, File stagingDir, ProgressListener listener,
            AtomicBoolean cancelled) throws IOException, GameSetupException {
        List<XdvdfsImage.Entry> files = image.listFiles();
        requireMainExecutableAtRoot(files);
        long total = 0L;
        for (XdvdfsImage.Entry file : files) {
            total += file.size;
        }
        GameInstaller.requireFreeSpace(total, stagingDir);
        try {
            GameInstaller.prepareStagingDir(stagingDir);
            long[] progress = {0L, 0L};
            for (XdvdfsImage.Entry file : files) {
                extractOne(image.source(), file, stagingDir, total, progress, listener, cancelled);
            }
            GameInstaller.replaceGameFolder(gameDir, stagingDir);
        } catch (IOException | RuntimeException e) {
            GameInstaller.deleteQuietly(stagingDir);
            throw e;
        }
    }

    private static void requireMainExecutableAtRoot(List<XdvdfsImage.Entry> files) throws GameSetupException {
        List<String> rootNames = new ArrayList<>();
        for (XdvdfsImage.Entry file : files) {
            if (!file.relativePath.contains("/")) {
                rootNames.add(file.relativePath);
            }
        }
        if (!GameFolder.containsMainExecutable(rootNames.toArray(new String[0]))) {
            throw new GameSetupException("default.xex was not found in the disc image."
                    + " Choose the .iso of your own Gears of War 2 disc.");
        }
    }

    // progress[0] = bytes written so far, progress[1] = time of the last progress report.
    private static void extractOne(XdvdfsImage.RandomRead source, XdvdfsImage.Entry file, File stagingDir,
            long total, long[] progress, ProgressListener listener, AtomicBoolean cancelled) throws IOException {
        File target = TargetPath.resolve(stagingDir, file.relativePath);
        GameInstaller.ensureParent(target);
        byte[] buffer = new byte[BUFFER_SIZE];
        try (FileOutputStream out = new FileOutputStream(target)) {
            long done = 0L;
            while (done < file.size) {
                if (cancelled != null && cancelled.get()) {
                    throw new IOException("extraction cancelled");
                }
                int length = (int) Math.min(buffer.length, file.size - done);
                int read = source.read(file.offset + done, buffer, 0, length);
                if (read <= 0) {
                    throw new IOException("unexpected end of the disc image in " + file.relativePath);
                }
                out.write(buffer, 0, read);
                done += read;
                progress[0] += read;
                progress[1] = GameInstaller.emitProgress(listener, progress[0], total, file.relativePath,
                        progress[1], false);
            }
        }
        progress[1] = GameInstaller.emitProgress(listener, progress[0], total, file.relativePath, progress[1], true);
    }

    private static XdvdfsImage.RandomRead channelReader(FileChannel channel) {
        return (position, buffer, offset, length) -> channel.read(ByteBuffer.wrap(buffer, offset, length), position);
    }
}
