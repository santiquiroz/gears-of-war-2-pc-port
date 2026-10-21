// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp.setup;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

public class GpuDriverInstallerTest {
    private static final byte[] LIBRARY = "ELF turnip".getBytes(StandardCharsets.US_ASCII);
    private static final String META = "{\"name\":\"Turnip\",\"driverVersion\":\"Vulkan 1.4\","
            + "\"libraryName\":\"libvulkan_freedreno.so\"}";

    @Rule
    public TemporaryFolder temp = new TemporaryFolder();

    static byte[] zip(String[] names, byte[][] contents) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ZipOutputStream out = new ZipOutputStream(bytes)) {
            for (int i = 0; i < names.length; i++) {
                out.putNextEntry(new ZipEntry(names[i]));
                out.write(contents[i]);
                out.closeEntry();
            }
        }
        return bytes.toByteArray();
    }

    private static byte[] driverZip(String meta, String libraryEntry) throws IOException {
        return zip(new String[] {"meta.json", libraryEntry},
                new byte[][] {meta.getBytes(StandardCharsets.UTF_8), LIBRARY});
    }

    private void expectRejected(byte[] zip) throws IOException {
        File files = temp.getRoot();
        try {
            GpuDriverInstaller.install(new ByteArrayInputStream(zip), files);
            fail("expected GameSetupException");
        } catch (GameSetupException expected) {
            assertTrue(expected.getMessage().contains("not a GPU driver package"));
        }
        assertNull(GpuDriverInstaller.installed(files));
        assertFalse(new File(files, "gpu_driver.incoming").exists());
    }

    @Test
    public void installsADriverPackageAndNamesIt() throws Exception {
        File files = temp.getRoot();
        GpuDriverInstaller.Driver driver = GpuDriverInstaller.install(
                new ByteArrayInputStream(driverZip(META, "libvulkan_freedreno.so")), files);
        assertEquals("libvulkan_freedreno.so", driver.libraryName);
        assertEquals("Turnip Vulkan 1.4", driver.label);
        assertArrayEquals(LIBRARY, Files.readAllBytes(new File(driver.dir, driver.libraryName).toPath()));
        assertNotNull(GpuDriverInstaller.installed(files));
    }

    @Test
    public void removeGoesBackToThePhoneDriver() throws Exception {
        File files = temp.getRoot();
        GpuDriverInstaller.install(new ByteArrayInputStream(driverZip(META, "libvulkan_freedreno.so")), files);
        GpuDriverInstaller.remove(files);
        assertNull(GpuDriverInstaller.installed(files));
    }

    @Test
    public void rejectsAPackageWithoutTheNamedLibrary() throws Exception {
        expectRejected(driverZip(META, "other.so"));
    }

    @Test
    public void rejectsAPackageWithoutMeta() throws Exception {
        expectRejected(zip(new String[] {"libvulkan_freedreno.so"}, new byte[][] {LIBRARY}));
    }

    @Test
    public void rejectsPathsInsideThePackage() throws Exception {
        expectRejected(driverZip(META, "../libvulkan_freedreno.so"));
    }

    @Test
    public void rejectsALibraryNameThatLeavesTheFolder() throws Exception {
        expectRejected(driverZip(META.replace("libvulkan_freedreno.so", "../../libc.so"), "libvulkan_freedreno.so"));
    }

    @Test
    public void aFailedInstallKeepsThePreviousDriver() throws Exception {
        File files = temp.getRoot();
        GpuDriverInstaller.install(new ByteArrayInputStream(driverZip(META, "libvulkan_freedreno.so")), files);
        try {
            GpuDriverInstaller.install(new ByteArrayInputStream(driverZip(META, "other.so")), files);
            fail("expected GameSetupException");
        } catch (GameSetupException expected) {
            assertNotNull(GpuDriverInstaller.installed(files));
        }
    }
}
