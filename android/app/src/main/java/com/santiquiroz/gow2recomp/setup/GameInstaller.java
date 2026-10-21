// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp.setup;

import android.content.ContentResolver;
import android.net.Uri;
import android.provider.DocumentsContract;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public final class GameInstaller {
    private static final int BUFFER_SIZE = 1024 * 1024;
    private static final long PROGRESS_INTERVAL_NANOS = 100_000_000L;

    private GameInstaller() {
    }

    public static void install(ContentResolver resolver, Uri treeUri, File gameDir,
            File stagingDir, ProgressListener listener, AtomicBoolean cancelled)
            throws IOException, GameSetupException {
        List<TreeEntry> entries = DocumentTreeReader.listFiles(resolver, treeUri);
        requireMainExecutableAtRoot(entries);
        long total = totalSize(entries);
        requireFreeSpace(total, stagingDir);
        try {
            prepareStagingDir(stagingDir);
            copyAll(resolver, treeUri, entries, stagingDir, total, listener, cancelled);
            replaceGameFolder(gameDir, stagingDir);
        } catch (IOException | RuntimeException e) {
            deleteQuietly(stagingDir);
            throw e;
        }
    }

    private static void requireMainExecutableAtRoot(List<TreeEntry> entries)
            throws GameSetupException {
        List<String> rootNames = new ArrayList<>();
        for (TreeEntry entry : entries) {
            if (!entry.relativePath().contains("/")) {
                rootNames.add(entry.relativePath());
            }
        }
        if (!GameFolder.containsMainExecutable(rootNames.toArray(new String[0]))) {
            throw new GameSetupException("default.xex was not found in the chosen folder."
                    + " Choose the folder that contains default.xex"
                    + " (your own extracted copy of the game).");
        }
    }

    private static long totalSize(List<TreeEntry> entries) {
        long total = 0L;
        for (TreeEntry entry : entries) {
            total += entry.sizeBytes();
        }
        return total;
    }

    static void requireFreeSpace(long total, File stagingDir) throws GameSetupException {
        File parent = stagingDir.getParentFile();
        if (parent == null) {
            parent = stagingDir;
        }
        if (!parent.exists()) {
            parent.mkdirs();
        }
        long free = parent.getUsableSpace();
        if (total > free) {
            throw new GameSetupException("Not enough free space: the game needs "
                    + ByteSize.format(total) + " and only " + ByteSize.format(free) + " is free.");
        }
    }

    static void prepareStagingDir(File stagingDir) throws IOException {
        if (stagingDir.exists()) {
            deleteRecursively(stagingDir);
        }
        if (!stagingDir.mkdirs()) {
            throw new IOException("cannot create " + stagingDir);
        }
    }

    private static void copyAll(ContentResolver resolver, Uri treeUri, List<TreeEntry> entries,
            File stagingDir, long total, ProgressListener listener, AtomicBoolean cancelled)
            throws IOException {
        ProgressState progress = new ProgressState();
        for (TreeEntry entry : entries) {
            copyOneFile(resolver, treeUri, entry, stagingDir, total, progress, listener, cancelled);
        }
    }

    private static void copyOneFile(ContentResolver resolver, Uri treeUri, TreeEntry entry,
            File stagingDir, long total, ProgressState progress,
            ProgressListener listener, AtomicBoolean cancelled) throws IOException {
        File target = TargetPath.resolve(stagingDir, entry.relativePath());
        ensureParent(target);
        Uri sourceUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, entry.documentId());
        try (InputStream in = resolver.openInputStream(sourceUri);
                FileOutputStream out = new FileOutputStream(target)) {
            if (in == null) {
                throw new IOException("cannot open " + entry.relativePath());
            }
            pumpStream(in, out, entry, total, progress, listener, cancelled);
        }
    }

    static void ensureParent(File target) throws IOException {
        File parent = target.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs() && !parent.exists()) {
            throw new IOException("cannot create " + parent);
        }
    }

    private static void pumpStream(InputStream in, FileOutputStream out, TreeEntry entry,
            long total, ProgressState progress,
            ProgressListener listener, AtomicBoolean cancelled) throws IOException {
        byte[] buffer = new byte[BUFFER_SIZE];
        long copiedFile = 0L;
        while (true) {
            if (cancelled != null && cancelled.get()) {
                throw new IOException("copy cancelled");
            }
            int read = in.read(buffer);
            if (read == -1) {
                break;
            }
            out.write(buffer, 0, read);
            copiedFile += read;
            progress.copiedTotal += read;
            progress.lastEmit = emitProgress(listener, progress.copiedTotal, total,
                    entry.relativePath(), progress.lastEmit, false);
        }
        progress.lastEmit = emitProgress(listener, progress.copiedTotal, total,
                entry.relativePath(), progress.lastEmit, true);
        checkSize(entry, copiedFile);
    }

    private static void checkSize(TreeEntry entry, long copiedFile) throws IOException {
        if (entry.sizeBytes() > 0 && copiedFile != entry.sizeBytes()) {
            throw new IOException("size mismatch for " + entry.relativePath());
        }
    }

    static long emitProgress(ProgressListener listener, long copied, long total,
            String relativePath, long lastEmit, boolean force) {
        if (listener == null) {
            return lastEmit;
        }
        long now = System.nanoTime();
        if (!force && now - lastEmit < PROGRESS_INTERVAL_NANOS) {
            return lastEmit;
        }
        listener.onProgress(copied, total, relativePath);
        return now;
    }

    static void replaceGameFolder(File gameDir, File stagingDir) throws IOException {
        if (gameDir.exists()) {
            deleteRecursively(gameDir);
        }
        if (!stagingDir.renameTo(gameDir)) {
            throw new IOException("cannot move " + stagingDir + " to " + gameDir);
        }
    }

    private static final class ProgressState {
        long copiedTotal;
        long lastEmit;
    }

    private static void deleteRecursively(File file) throws IOException {
        Path path = file.toPath();
        if (!Files.exists(path, java.nio.file.LinkOption.NOFOLLOW_LINKS)) {
            return;
        }
        Files.walkFileTree(path, new SimpleFileVisitor<Path>() {
            @Override
            public FileVisitResult visitFile(Path f, BasicFileAttributes attrs)
                    throws IOException {
                Files.deleteIfExists(f);
                return FileVisitResult.CONTINUE;
            }

            @Override
            public FileVisitResult postVisitDirectory(Path dir, IOException exc)
                    throws IOException {
                if (exc != null) {
                    throw exc;
                }
                Files.deleteIfExists(dir);
                return FileVisitResult.CONTINUE;
            }

            @Override
            public FileVisitResult preVisitDirectory(Path dir, BasicFileAttributes attrs)
                    throws IOException {
                if (Files.isSymbolicLink(dir)) {
                    Files.deleteIfExists(dir);
                    return FileVisitResult.SKIP_SUBTREE;
                }
                return FileVisitResult.CONTINUE;
            }
        });
    }

    static void deleteQuietly(File file) {
        try {
            deleteRecursively(file);
        } catch (IOException | RuntimeException ignored) {
            // Best effort cleanup; the original exception is rethrown.
        }
    }
}
