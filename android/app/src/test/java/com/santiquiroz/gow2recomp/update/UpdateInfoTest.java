// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp.update;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class UpdateInfoTest {
    private static final String PAGE_URL =
            "https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/tag/v0.2.3";
    private static final String APK_URL =
            "https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/download/v0.2.3/gow2-recomp-v0.2.3-android-arm64.apk";
    private static final String WINDOWS_URL =
            "https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/download/v0.2.3/gow2-recomp-v0.2.3-windows-x64.zip";

    private static final String RELEASE = "{"
            + "\"tag_name\":\"v0.2.3\","
            + "\"html_url\":\"" + PAGE_URL + "\","
            + "\"draft\":false,\"prerelease\":false,"
            + "\"assets\":["
            + "{\"name\":\"gow2-recomp-v0.2.3-windows-x64.zip\","
            + "\"browser_download_url\":\"" + WINDOWS_URL + "\"},"
            + "{\"name\":\"gow2-recomp-v0.2.3-android-arm64.apk\","
            + "\"browser_download_url\":\"" + APK_URL + "\"}"
            + "]}";

    @Test
    public void parsesReleaseAndPicksTheAndroidAsset() {
        UpdateInfo info = UpdateInfo.parse(RELEASE);

        assertEquals("v0.2.3", info.tag());
        assertEquals(PAGE_URL, info.pageUrl());
        assertEquals(APK_URL, info.apkUrl());
        assertEquals(APK_URL, info.downloadUrl());
    }

    @Test
    public void withoutAnApkAssetTheDownloadFallsBackToTheReleasePage() {
        String json = "{\"tag_name\":\"v0.2.3\",\"html_url\":\"" + PAGE_URL + "\","
                + "\"draft\":false,\"prerelease\":false,"
                + "\"assets\":[{\"name\":\"notes.txt\","
                + "\"browser_download_url\":\"https://example.com/notes.txt\"}]}";

        UpdateInfo info = UpdateInfo.parse(json);

        assertEquals("v0.2.3", info.tag());
        assertNull(info.apkUrl());
        assertEquals(PAGE_URL, info.downloadUrl());
    }

    @Test
    public void missingAssetsFallBackToTheReleasePage() {
        String json = "{\"tag_name\":\"v0.2.3\",\"html_url\":\"" + PAGE_URL + "\","
                + "\"draft\":false,\"prerelease\":false}";

        UpdateInfo info = UpdateInfo.parse(json);

        assertNull(info.apkUrl());
        assertEquals(PAGE_URL, info.downloadUrl());
    }

    @Test
    public void aNewerReleaseWithAnApkIsAnUpdate() {
        UpdateInfo info = UpdateInfo.parse(RELEASE);

        assertTrue(info.isUpdateFor("0.2.2"));
        assertFalse(info.isUpdateFor("0.2.3"));
        assertFalse(info.isUpdateFor("0.3.0"));
    }

    @Test
    public void aNewerReleaseWithoutAnApkIsNotAnUpdate() {
        UpdateInfo pcOnly = new UpdateInfo("v0.4.2", PAGE_URL, null);

        assertFalse(pcOnly.isUpdateFor("0.3.18"));
        assertFalse(new UpdateInfo("v0.4.2", PAGE_URL, "").isUpdateFor("0.3.18"));
    }

    @Test
    public void draftsAreIgnored() {
        assertNull(UpdateInfo.parse(RELEASE.replace("\"draft\":false", "\"draft\":true")));
    }

    @Test
    public void prereleasesAreIgnored() {
        assertNull(UpdateInfo.parse(RELEASE.replace("\"prerelease\":false", "\"prerelease\":true")));
    }

    @Test
    public void malformedBodiesAreIgnored() {
        assertNull(UpdateInfo.parse(null));
        assertNull(UpdateInfo.parse(""));
        assertNull(UpdateInfo.parse("not json"));
        assertNull(UpdateInfo.parse("{}"));
        assertNull(UpdateInfo.parse("{\"tag_name\":\"v0.2.3\"}"));
    }
}
