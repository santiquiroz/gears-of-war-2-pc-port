// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp.setup;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.List;

public class XdvdfsImageTest {
    private static final int SECTOR = XdvdfsImage.SECTOR;
    private static final byte[] XEX = "XEX2 fake executable".getBytes(StandardCharsets.US_ASCII);
    private static final byte[] DATA = "packaged data".getBytes(StandardCharsets.US_ASCII);

    @Rule
    public TemporaryFolder temp = new TemporaryFolder();

    // A minimal image: volume at sector 32, root directory at 33 (default.xex, GearGame/),
    // GearGame directory at 34 (Startup.xxx), file data at 35 and 36.
    private static byte[] image(long base, String rootFileName) {
        int sectors = 40;
        byte[] b = new byte[(int) (base + (long) sectors * SECTOR)];
        int volume = (int) (base + 32L * SECTOR);
        put(b, volume, "MICROSOFT*XBOX*MEDIA".getBytes(StandardCharsets.US_ASCII));
        putU32(b, volume + 20, 33);
        putU32(b, volume + 24, SECTOR);
        int root = (int) (base + 33L * SECTOR);
        // Entry at 0: rootFileName, right child at dword 8 (byte 32).
        entry(b, root, 0xFFFF, 8, 35, XEX.length, 0x00, rootFileName);
        entry(b, root + 32, 0xFFFF, 0xFFFF, 34, SECTOR, 0x10, "GearGame");
        int sub = (int) (base + 34L * SECTOR);
        entry(b, sub, 0xFFFF, 0xFFFF, 36, DATA.length, 0x00, "Startup.xxx");
        put(b, (int) (base + 35L * SECTOR), XEX);
        put(b, (int) (base + 36L * SECTOR), DATA);
        return b;
    }

    private static void entry(byte[] b, int at, int left, int right, long sector, long size, int attrs, String name) {
        b[at] = (byte) left;
        b[at + 1] = (byte) (left >> 8);
        b[at + 2] = (byte) right;
        b[at + 3] = (byte) (right >> 8);
        putU32(b, at + 4, sector);
        putU32(b, at + 8, size);
        b[at + 12] = (byte) attrs;
        byte[] n = name.getBytes(StandardCharsets.ISO_8859_1);
        b[at + 13] = (byte) n.length;
        put(b, at + 14, n);
    }

    private static void put(byte[] b, int at, byte[] data) {
        System.arraycopy(data, 0, b, at, data.length);
    }

    private static void putU32(byte[] b, int at, long v) {
        for (int i = 0; i < 4; i++) {
            b[at + i] = (byte) (v >> (8 * i));
        }
    }

    private static XdvdfsImage.RandomRead reader(byte[] b) {
        return (position, buffer, offset, length) -> {
            if (position >= b.length) {
                return -1;
            }
            int n = (int) Math.min(length, b.length - position);
            System.arraycopy(b, (int) position, buffer, offset, n);
            return n;
        };
    }

    @Test
    public void listsFilesOfAPlainImage() throws Exception {
        byte[] b = image(0, "default.xex");
        List<XdvdfsImage.Entry> files = XdvdfsImage.open(reader(b), b.length).listFiles();
        assertEquals(2, files.size());
        assertTrue(files.stream().anyMatch(f -> f.relativePath.equals("default.xex") && f.size == XEX.length));
        assertTrue(files.stream().anyMatch(f -> f.relativePath.equals("GearGame/Startup.xxx")));
    }

    @Test
    public void findsTheGamePartitionOfAnXgd3Dump() throws Exception {
        byte[] b = image(0x2080000L, "default.xex");
        assertEquals(2, XdvdfsImage.open(reader(b), b.length).listFiles().size());
    }

    @Test
    public void extractsExactBytesAndReplacesTheGameFolder() throws Exception {
        byte[] b = image(0, "default.xex");
        File game = new File(temp.getRoot(), "game");
        File staging = new File(temp.getRoot(), "game.incoming");
        IsoInstaller.extract(XdvdfsImage.open(reader(b), b.length), game, staging, null, null);
        assertArrayEquals(XEX, Files.readAllBytes(new File(game, "default.xex").toPath()));
        assertArrayEquals(DATA, Files.readAllBytes(new File(game, "GearGame/Startup.xxx").toPath()));
        assertFalse(staging.exists());
    }

    @Test
    public void rejectsAFileThatIsNotADiscImage() throws Exception {
        byte[] b = new byte[40 * SECTOR];
        try {
            XdvdfsImage.open(reader(b), b.length);
            fail("expected GameSetupException");
        } catch (GameSetupException expected) {
            assertTrue(expected.getMessage().contains("not an Xbox 360 disc image"));
        }
    }

    @Test
    public void rejectsAnImageWithoutDefaultXex() throws Exception {
        byte[] b = image(0, "other.xex");
        File game = new File(temp.getRoot(), "game");
        try {
            IsoInstaller.extract(XdvdfsImage.open(reader(b), b.length), game, new File(temp.getRoot(), "s"), null, null);
            fail("expected GameSetupException");
        } catch (GameSetupException expected) {
            assertFalse(game.exists());
        }
    }

    @Test
    public void rejectsUnsafeNames() throws Exception {
        byte[] b = image(0, "..");
        try {
            XdvdfsImage.open(reader(b), b.length).listFiles();
            fail("expected GameSetupException");
        } catch (GameSetupException expected) {
            assertTrue(expected.getMessage().contains("damaged"));
        }
    }
}
