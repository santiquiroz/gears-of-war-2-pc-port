// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp.setup;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.fail;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

public class GpuDriverHooksTest {
    @Rule
    public TemporaryFolder temp = new TemporaryFolder();

    private File apk(String... libraries) throws IOException {
        String[] names = new String[libraries.length];
        byte[][] contents = new byte[libraries.length][];
        for (int i = 0; i < libraries.length; i++) {
            names[i] = "lib/arm64-v8a/" + libraries[i];
            contents[i] = ("ELF " + libraries[i]).getBytes(StandardCharsets.US_ASCII);
        }
        File apk = temp.newFile("app.apk");
        Files.write(apk.toPath(), GpuDriverInstallerTest.zip(names, contents));
        return apk;
    }

    @Test
    public void copiesTheHookLibrariesAndReplacesStaleCopies() throws Exception {
        File files = temp.newFolder("files");
        File hookDir = new File(files, GpuDriverHooks.HOOK_FOLDER);
        Files.createDirectories(hookDir.toPath());
        Files.write(new File(hookDir, "libmain_hook.so").toPath(), "old build!!!!!!!!!!!!".getBytes(StandardCharsets.US_ASCII));

        GpuDriverHooks.copy(apk(GpuDriverHooks.LIBRARIES), files);

        for (String name : GpuDriverHooks.LIBRARIES) {
            assertArrayEquals(("ELF " + name).getBytes(StandardCharsets.US_ASCII),
                    Files.readAllBytes(new File(hookDir, name).toPath()));
        }
    }

    @Test
    public void failsWhenTheApkLacksAHook() throws Exception {
        try {
            GpuDriverHooks.copy(apk("libhook_impl.so"), temp.newFolder("files"));
            fail("expected IOException");
        } catch (IOException expected) {
            // The game then starts with the phone's driver.
        }
    }
}
