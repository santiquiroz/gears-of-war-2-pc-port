// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp.update;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * A GitHub release worth offering to the user: its tag, its release page, and the direct
 * download of the {@code -android-arm64.apk} asset when the release carries one.
 *
 * <p>Parsing is self-contained (no new dependencies): {@link #parse} reads one release object
 * with {@code tag_name}, {@code html_url}, {@code draft}, {@code prerelease} and
 * {@code assets} ([{name, browser_download_url}]).
 */
public final class UpdateInfo {
    static final String APK_ASSET_SUFFIX = "-android-arm64.apk";

    private final String tag;
    private final String pageUrl;
    private final String apkUrl;

    public UpdateInfo(String tag, String pageUrl, String apkUrl) {
        this.tag = tag;
        this.pageUrl = pageUrl;
        this.apkUrl = apkUrl;
    }

    public String tag() {
        return tag;
    }

    public String pageUrl() {
        return pageUrl;
    }

    /** The direct APK download, or null when the release carries no Android asset. */
    public String apkUrl() {
        return apkUrl;
    }

    /** The APK download when present, otherwise the release page. */
    public String downloadUrl() {
        return apkUrl != null ? apkUrl : pageUrl;
    }

    /**
     * True when this release is newer than {@code currentVersion} and carries an APK. Releases
     * for other platforms only are not updates for the phone.
     */
    public boolean isUpdateFor(String currentVersion) {
        return apkUrl != null && !apkUrl.isEmpty()
                && VersionCompare.isNewerThan(tag, currentVersion);
    }

    /**
     * Parses one GitHub release body. Returns null for drafts, prereleases and malformed
     * bodies, so callers keep the last known result silently.
     */
    public static UpdateInfo parse(String json) {
        if (json == null) {
            return null;
        }
        try {
            Object root = Json.read(json);
            if (!(root instanceof Map)) {
                return null;
            }
            Map<?, ?> release = (Map<?, ?>) root;
            if (isTrue(release.get("draft")) || isTrue(release.get("prerelease"))) {
                return null;
            }
            String tag = text(release.get("tag_name"));
            String pageUrl = text(release.get("html_url"));
            if (tag == null || tag.isEmpty() || pageUrl == null || pageUrl.isEmpty()) {
                return null;
            }
            return new UpdateInfo(tag, pageUrl, androidAsset(release.get("assets")));
        } catch (RuntimeException e) {
            return null;
        }
    }

    private static String androidAsset(Object assets) {
        if (!(assets instanceof List)) {
            return null;
        }
        for (Object entry : (List<?>) assets) {
            if (!(entry instanceof Map)) {
                continue;
            }
            Map<?, ?> asset = (Map<?, ?>) entry;
            String name = text(asset.get("name"));
            String url = text(asset.get("browser_download_url"));
            if (name != null && name.endsWith(APK_ASSET_SUFFIX) && url != null && !url.isEmpty()) {
                return url;
            }
        }
        return null;
    }

    private static boolean isTrue(Object value) {
        return value instanceof Boolean && (Boolean) value;
    }

    private static String text(Object value) {
        return value instanceof String ? (String) value : null;
    }

    @Override
    public boolean equals(Object other) {
        if (this == other) {
            return true;
        }
        if (!(other instanceof UpdateInfo)) {
            return false;
        }
        UpdateInfo that = (UpdateInfo) other;
        return Objects.equals(tag, that.tag)
                && Objects.equals(pageUrl, that.pageUrl)
                && Objects.equals(apkUrl, that.apkUrl);
    }

    @Override
    public int hashCode() {
        return Objects.hash(tag, pageUrl, apkUrl);
    }

    @Override
    public String toString() {
        return "UpdateInfo{tag='" + tag + "', pageUrl='" + pageUrl + "', apkUrl='" + apkUrl + "'}";
    }

    /** Minimal JSON reader for the release payload subset (objects, arrays, strings, numbers,
     * booleans, null). Anything outside JSON grammar fails with a RuntimeException. */
    private static final class Json {
        private final String text;
        private int pos;

        private Json(String text) {
            this.text = text;
        }

        static Object read(String text) {
            Json reader = new Json(text);
            Object value = reader.readValue();
            reader.skipGaps();
            if (reader.pos != text.length()) {
                throw new IllegalArgumentException("trailing data after the JSON value");
            }
            return value;
        }

        private Object readValue() {
            skipGaps();
            if (pos >= text.length()) {
                throw new IllegalArgumentException("unexpected end of the JSON value");
            }
            char c = text.charAt(pos);
            switch (c) {
                case '{':
                    return readObject();
                case '[':
                    return readArray();
                case '"':
                    return readString();
                case 't':
                    return readLiteral("true", Boolean.TRUE);
                case 'f':
                    return readLiteral("false", Boolean.FALSE);
                case 'n':
                    return readLiteral("null", null);
                default:
                    return readNumber();
            }
        }

        private Map<String, Object> readObject() {
            Map<String, Object> object = new LinkedHashMap<>();
            pos++;
            skipGaps();
            if (consume('}')) {
                return object;
            }
            while (true) {
                skipGaps();
                if (pos >= text.length() || text.charAt(pos) != '"') {
                    throw new IllegalArgumentException("expected a string key");
                }
                String key = readString();
                skipGaps();
                if (!consume(':')) {
                    throw new IllegalArgumentException("expected ':' after the key");
                }
                object.put(key, readValue());
                skipGaps();
                if (consume('}')) {
                    return object;
                }
                if (!consume(',')) {
                    throw new IllegalArgumentException("expected ',' between entries");
                }
            }
        }

        private List<Object> readArray() {
            List<Object> array = new ArrayList<>();
            pos++;
            skipGaps();
            if (consume(']')) {
                return array;
            }
            while (true) {
                array.add(readValue());
                skipGaps();
                if (consume(']')) {
                    return array;
                }
                if (!consume(',')) {
                    throw new IllegalArgumentException("expected ',' between items");
                }
            }
        }

        private String readString() {
            StringBuilder value = new StringBuilder();
            pos++;
            while (true) {
                if (pos >= text.length()) {
                    throw new IllegalArgumentException("unterminated string");
                }
                char c = text.charAt(pos++);
                if (c == '"') {
                    return value.toString();
                }
                if (c != '\\') {
                    value.append(c);
                    continue;
                }
                if (pos >= text.length()) {
                    throw new IllegalArgumentException("unterminated escape");
                }
                char escaped = text.charAt(pos++);
                switch (escaped) {
                    case '"':
                        value.append('"');
                        break;
                    case '\\':
                        value.append('\\');
                        break;
                    case '/':
                        value.append('/');
                        break;
                    case 'b':
                        value.append('\b');
                        break;
                    case 'f':
                        value.append('\f');
                        break;
                    case 'n':
                        value.append('\n');
                        break;
                    case 'r':
                        value.append('\r');
                        break;
                    case 't':
                        value.append('\t');
                        break;
                    case 'u':
                        value.append(readUnicodeEscape());
                        break;
                    default:
                        throw new IllegalArgumentException("bad escape '\\" + escaped + "'");
                }
            }
        }

        private char readUnicodeEscape() {
            if (pos + 4 > text.length()) {
                throw new IllegalArgumentException("truncated unicode escape");
            }
            int code = 0;
            for (int i = 0; i < 4; i++) {
                int digit = Character.digit(text.charAt(pos++), 16);
                if (digit < 0) {
                    throw new IllegalArgumentException("bad unicode escape");
                }
                code = code * 16 + digit;
            }
            return (char) code;
        }

        private Object readLiteral(String word, Object value) {
            if (!text.startsWith(word, pos)) {
                throw new IllegalArgumentException("bad literal");
            }
            pos += word.length();
            return value;
        }

        private Number readNumber() {
            int start = pos;
            while (pos < text.length() && "-+0123456789.eE".indexOf(text.charAt(pos)) >= 0) {
                pos++;
            }
            if (start == pos) {
                throw new IllegalArgumentException("unexpected character '" + text.charAt(pos) + "'");
            }
            try {
                return Double.valueOf(text.substring(start, pos));
            } catch (NumberFormatException e) {
                throw new IllegalArgumentException("bad number", e);
            }
        }

        private void skipGaps() {
            while (pos < text.length()) {
                char c = text.charAt(pos);
                if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                    return;
                }
                pos++;
            }
        }

        private boolean consume(char expected) {
            if (pos < text.length() && text.charAt(pos) == expected) {
                pos++;
                return true;
            }
            return false;
        }
    }
}
