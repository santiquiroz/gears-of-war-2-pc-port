// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp.setup;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Reads the file tree of an Xbox 360 disc image (XDVDFS). */
public final class XdvdfsImage {
    public static final int SECTOR = 2048;
    private static final long VOLUME_SECTOR = 32;
    private static final byte[] MAGIC = "MICROSOFT*XBOX*MEDIA".getBytes(StandardCharsets.US_ASCII);
    // Where the game partition starts: a plain image (xdvdfs, extract-xiso), then the XGD1, XGD2 and
    // XGD3 layouts of a full disc dump.
    private static final long[] PARTITION_OFFSETS = {0L, 0x18300000L, 0xFD90000L, 0x2080000L};
    private static final int ATTR_DIRECTORY = 0x10;
    private static final int NO_CHILD = 0xFFFF;
    private static final int MAX_DEPTH = 32;
    private static final int MAX_DIRECTORY_BYTES = 64 * 1024 * 1024;

    /** Positional reads from the image; returns the bytes read, fewer only at the end. */
    public interface RandomRead {
        int read(long position, byte[] buffer, int offset, int length) throws IOException;
    }

    /** One file of the image: its path with '/' separators, where its data starts and its size. */
    public static final class Entry {
        public final String relativePath;
        public final long offset;
        public final long size;

        Entry(String relativePath, long offset, long size) {
            this.relativePath = relativePath;
            this.offset = offset;
            this.size = size;
        }
    }

    private final RandomRead source;
    private final long imageSize;
    private final long base;
    private final long rootSector;
    private final long rootSize;

    private XdvdfsImage(RandomRead source, long imageSize, long base, long rootSector, long rootSize) {
        this.source = source;
        this.imageSize = imageSize;
        this.base = base;
        this.rootSector = rootSector;
        this.rootSize = rootSize;
    }

    public static XdvdfsImage open(RandomRead source, long imageSize) throws IOException, GameSetupException {
        for (long base : PARTITION_OFFSETS) {
            long volume = base + VOLUME_SECTOR * SECTOR;
            if (volume + SECTOR > imageSize) {
                continue;
            }
            byte[] header = readFully(source, volume, 32);
            if (startsWithMagic(header)) {
                return new XdvdfsImage(source, imageSize, base, u32(header, 20), u32(header, 24));
            }
        }
        throw new GameSetupException("This file is not an Xbox 360 disc image (no XDVDFS volume found)."
                + " Choose the .iso of your own Gears of War 2 disc.");
    }

    public List<Entry> listFiles() throws IOException, GameSetupException {
        List<Entry> files = new ArrayList<>();
        Set<Long> visitedDirectories = new HashSet<>();
        walkDirectory(rootSector, rootSize, "", 0, files, visitedDirectories);
        return files;
    }

    public RandomRead source() {
        return source;
    }

    private void walkDirectory(long sector, long size, String prefix, int depth, List<Entry> files,
            Set<Long> visitedDirectories) throws IOException, GameSetupException {
        if (size == 0) {
            return;
        }
        if (depth > MAX_DEPTH || size > MAX_DIRECTORY_BYTES || !visitedDirectories.add(sector)) {
            throw corrupt();
        }
        byte[] table = readFully(source, checkedOffset(sector, size), (int) size);
        Deque<Integer> pending = new ArrayDeque<>();
        Set<Integer> seen = new HashSet<>();
        pending.push(0);
        while (!pending.isEmpty()) {
            int at = pending.pop();
            if (!seen.add(at)) {
                throw corrupt();
            }
            readEntry(table, at, prefix, depth, files, visitedDirectories, pending);
        }
    }

    private void readEntry(byte[] table, int at, String prefix, int depth, List<Entry> files,
            Set<Long> visitedDirectories, Deque<Integer> pending) throws IOException, GameSetupException {
        if (at + 14 > table.length) {
            throw corrupt();
        }
        int left = u16(table, at);
        int right = u16(table, at + 2);
        long sector = u32(table, at + 4);
        long size = u32(table, at + 8);
        int attributes = table[at + 12] & 0xFF;
        int nameLength = table[at + 13] & 0xFF;
        if (left == NO_CHILD && right == NO_CHILD && nameLength == 0) {
            return;
        }
        if (at + 14 + nameLength > table.length) {
            throw corrupt();
        }
        String name = new String(table, at + 14, nameLength, StandardCharsets.ISO_8859_1);
        if (!SafeName.isSafe(name)) {
            throw corrupt();
        }
        String path = prefix.isEmpty() ? name : prefix + "/" + name;
        if ((attributes & ATTR_DIRECTORY) != 0) {
            walkDirectory(sector, size, path, depth + 1, files, visitedDirectories);
        } else {
            files.add(new Entry(path, checkedOffset(sector, size), size));
        }
        pushChild(left, pending);
        pushChild(right, pending);
    }

    private static void pushChild(int dwordOffset, Deque<Integer> pending) {
        if (dwordOffset != NO_CHILD && dwordOffset != 0) {
            pending.push(dwordOffset * 4);
        }
    }

    private long checkedOffset(long sector, long size) throws GameSetupException {
        long offset = base + sector * SECTOR;
        if (offset < 0 || size < 0 || offset + size > imageSize) {
            throw corrupt();
        }
        return offset;
    }

    private static GameSetupException corrupt() {
        return new GameSetupException("The disc image is damaged or not a supported Xbox 360 image.");
    }

    static byte[] readFully(RandomRead source, long position, int length) throws IOException {
        byte[] buffer = new byte[length];
        int done = 0;
        while (done < length) {
            int read = source.read(position + done, buffer, done, length - done);
            if (read <= 0) {
                throw new IOException("unexpected end of the disc image");
            }
            done += read;
        }
        return buffer;
    }

    private static boolean startsWithMagic(byte[] header) {
        for (int i = 0; i < MAGIC.length; i++) {
            if (header[i] != MAGIC[i]) {
                return false;
            }
        }
        return true;
    }

    private static int u16(byte[] b, int at) {
        return (b[at] & 0xFF) | ((b[at + 1] & 0xFF) << 8);
    }

    private static long u32(byte[] b, int at) {
        return (b[at] & 0xFFL) | ((b[at + 1] & 0xFFL) << 8) | ((b[at + 2] & 0xFFL) << 16)
                | ((b[at + 3] & 0xFFL) << 24);
    }
}
