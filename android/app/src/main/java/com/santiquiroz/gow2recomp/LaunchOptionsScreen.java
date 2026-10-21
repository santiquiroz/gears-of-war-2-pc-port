// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp;

import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.graphics.Rect;
import android.net.Uri;
import android.util.TypedValue;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import com.santiquiroz.gow2recomp.setup.GpuDriverInstaller;
import com.santiquiroz.gow2recomp.update.UpdateChecker;
import com.santiquiroz.gow2recomp.update.UpdateInfo;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.List;

final class LaunchOptionsScreen {
    interface Actions {
        void play(boolean safeMode);

        void chooseGpuDriver();
    }

    private final Activity activity;
    private final File dataDir;
    private final Actions actions;
    private final ScrollView root;
    private final TextView status;
    private final TextView updateStatus;
    private final Button updateButton;
    private final TextView gpuDriver;
    private final Button gpuDriverRemove;
    private final List<Choice> choices = new ArrayList<>();
    private String original = "";
    private boolean loaded;

    LaunchOptionsScreen(Activity activity, File dataDir, boolean crashed, Actions actions) {
        this.activity = activity;
        this.dataDir = dataDir;
        this.actions = actions;
        status = text("", 16);
        status.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        updateStatus = text("", 16);
        updateButton = button(R.string.update_check_now, v -> checkForUpdates());
        gpuDriver = text("", 16);
        gpuDriverRemove = button(R.string.launch_gpu_driver_remove, v -> removeGpuDriver());
        root = new ScrollView(activity);
        root.addView(column(crashed));
        root.setOnApplyWindowInsetsListener((view, insets) -> {
            Rect safe = GameSetupScreen.safeArea(insets);
            view.setPadding(safe.left, safe.top, safe.right, safe.bottom);
            return insets;
        });
        refresh();
    }

    View view() {
        return root;
    }

    private LinearLayout column(boolean crashed) {
        LinearLayout column = new LinearLayout(activity);
        column.setOrientation(LinearLayout.VERTICAL);
        int padding = dp(32);
        column.setPadding(padding, padding, padding, padding);
        column.addView(text(activity.getString(crashed
                ? R.string.launch_crash_title : R.string.launch_title), 24));
        addChoice(column, R.string.launch_gpu_sparse, "vulkan_sparse_shared_memory",
                new int[]{R.string.launch_auto, R.string.launch_on, R.string.launch_off},
                new String[]{null, "true", "false"});
        addChoice(column, R.string.launch_resolution, "resolution_scale",
                new int[]{R.string.launch_auto, R.string.launch_1x, R.string.launch_2x},
                new String[]{null, "1", "2"});
        addChoice(column, R.string.launch_filter, "present_effect",
                new int[]{R.string.launch_auto, R.string.launch_bilinear, R.string.launch_fsr,
                        R.string.launch_cas, R.string.launch_sgsr},
                new String[]{null, "\"bilinear\"", "\"fsr\"", "\"cas\"", "\"sgsr\""});
        addChoice(column, R.string.launch_unlock_fps, "gow2_unlock_fps",
                new int[]{R.string.launch_auto, R.string.launch_on, R.string.launch_off},
                new String[]{null, "true", "false"});
        addChoice(column, R.string.launch_shadows, "gow2_gfx_light_environment_shadows",
                new int[]{R.string.launch_auto, R.string.launch_on, R.string.launch_off},
                new String[]{null, "\"on\"", "\"off\""});
        column.addView(text(activity.getString(R.string.launch_shadows_hint), 14));
        addGpuDriverRow(column);
        column.addView(button(R.string.launch_play, v -> start(false)), spaced());
        column.addView(button(R.string.launch_safe_mode, v -> start(true)), spaced());
        column.addView(button(R.string.launch_reset, v -> reset()), spaced());
        column.addView(button(R.string.launch_share_log, v -> shareLog()), spaced());
        column.addView(updateStatus, spaced());
        column.addView(updateButton, spaced());
        column.addView(status, spaced());
        refreshUpdateLine();
        return column;
    }

    private void addGpuDriverRow(LinearLayout column) {
        column.addView(text(activity.getString(R.string.launch_gpu_driver), 16), spaced());
        column.addView(gpuDriver);
        column.addView(button(R.string.launch_gpu_driver_choose, v -> actions.chooseGpuDriver()), spaced());
        column.addView(gpuDriverRemove, spaced());
        column.addView(text(activity.getString(R.string.launch_gpu_driver_hint), 14));
        refreshGpuDriver();
    }

    void refreshGpuDriver() {
        GpuDriverInstaller.Driver driver = GpuDriverInstaller.installed(activity.getFilesDir());
        gpuDriver.setText(driver != null ? driver.label : activity.getString(R.string.launch_gpu_driver_system));
        gpuDriverRemove.setEnabled(driver != null);
    }

    void showStatus(String message) {
        status.setText(message);
    }

    private void removeGpuDriver() {
        GpuDriverInstaller.remove(activity.getFilesDir());
        refreshGpuDriver();
    }

    private void addChoice(LinearLayout column, int label, String key, int[] labels,
            String[] values) {
        TextView title = text(activity.getString(label), 16);
        Spinner spinner = new Spinner(activity);
        spinner.setId(View.generateViewId());
        title.setLabelFor(spinner.getId());
        spinner.setContentDescription(activity.getString(label));
        String[] entries = new String[labels.length];
        for (int i = 0; i < labels.length; i++) {
            entries[i] = activity.getString(labels[i]);
        }
        ArrayAdapter<String> adapter = new ArrayAdapter<>(activity,
                android.R.layout.simple_spinner_item, entries);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(adapter);
        choices.add(new Choice(key, values, spinner));
        column.addView(title, spaced());
        column.addView(spinner);
    }

    private void refresh() {
        loaded = false;
        try {
            File file = settings();
            original = file.exists()
                    ? new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8) : "";
            for (Choice choice : choices) {
                choice.load(original);
            }
            loaded = true;
            status.setText("");
        } catch (IOException | RuntimeException e) {
            showError(R.string.launch_read_failed, e);
        }
    }

    private void start(boolean safeMode) {
        if (!loaded) {
            refresh();
        }
        if (!loaded) {
            return;
        }
        try {
            String updated = selectedSettings();
            if (safeMode) {
                updated = safeSettings(updated);
            }
            if (!updated.equals(original)) {
                writeSettings(updated);
                original = updated;
            }
        } catch (IOException | RuntimeException e) {
            showError(R.string.launch_write_failed, e);
            return;
        }
        actions.play(safeMode);
    }

    private String selectedSettings() {
        String updated = original;
        for (Choice choice : choices) {
            updated = choice.apply(updated);
        }
        return updated;
    }

    private static String safeSettings(String toml) {
        String updated = TomlRows.set(toml, "vulkan_sparse_shared_memory", "false");
        updated = TomlRows.set(updated, "resolution_scale", "1");
        updated = TomlRows.set(updated, "present_effect", "\"bilinear\"");
        return TomlRows.set(updated, "gow2_gfx_light_environment_shadows", "\"off\"");
    }

    private void writeSettings(String updated) throws IOException {
        Files.createDirectories(dataDir.toPath());
        File temporary = File.createTempFile("gow2-settings-", ".tmp", dataDir);
        try {
            Files.write(temporary.toPath(), updated.getBytes(StandardCharsets.UTF_8));
            Files.move(temporary.toPath(), settings().toPath(), StandardCopyOption.REPLACE_EXISTING);
        } finally {
            Files.deleteIfExists(temporary.toPath());
        }
    }

    private void reset() {
        try {
            File file = settings();
            if (file.exists()) {
                Files.move(file.toPath(), new File(dataDir, "gow2.toml.bak").toPath(),
                        StandardCopyOption.REPLACE_EXISTING);
            }
        } catch (IOException | RuntimeException e) {
            showError(R.string.launch_reset_failed, e);
            return;
        }
        refresh();
    }

    private void shareLog() {
        try {
            File log = LastLogProvider.newest(dataDir);
            if (log == null) {
                status.setText(R.string.launch_no_log);
                return;
            }
            Uri uri = LastLogProvider.uriFor(activity, log);
            Intent send = new Intent(Intent.ACTION_SEND);
            send.setType("text/plain");
            send.putExtra(Intent.EXTRA_STREAM, uri);
            send.setClipData(ClipData.newRawUri(log.getName(), uri));
            send.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            activity.startActivity(Intent.createChooser(send,
                    activity.getString(R.string.launch_share_title)));
        } catch (IOException | RuntimeException e) {
            showError(R.string.launch_share_failed, e);
        }
    }

    private File settings() {
        return new File(dataDir, "gow2.toml");
    }

    private void showError(int message, Exception error) {
        status.setText(activity.getString(message, error.getMessage()));
    }

    private void refreshUpdateLine() {
        UpdateInfo pending =
                UpdateChecker.pendingUpdate(activity, BuildConfig.VERSION_NAME);
        if (pending != null) {
            updateStatus.setText(activity.getString(R.string.update_available, pending.tag()));
        } else {
            updateStatus.setText(R.string.update_up_to_date);
        }
    }

    private void checkForUpdates() {
        updateButton.setEnabled(false);
        updateStatus.setText(R.string.update_checking);
        UpdateChecker.refreshNow(activity.getApplicationContext(), BuildConfig.VERSION_NAME,
                (ok, info, newer) -> activity.runOnUiThread(() -> {
                    updateButton.setEnabled(true);
                    if (!ok) {
                        updateStatus.setText(R.string.update_check_failed);
                    } else if (newer && info != null) {
                        updateStatus.setText(
                                activity.getString(R.string.update_available, info.tag()));
                    } else {
                        updateStatus.setText(R.string.update_up_to_date);
                    }
                }));
    }

    private Button button(int label, View.OnClickListener listener) {
        Button button = new Button(activity);
        button.setText(label);
        button.setOnClickListener(listener);
        return button;
    }

    private TextView text(String value, int size) {
        TextView text = new TextView(activity);
        text.setText(value);
        text.setTextSize(TypedValue.COMPLEX_UNIT_SP, size);
        return text;
    }

    private LinearLayout.LayoutParams spaced() {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        params.topMargin = dp(16);
        return params;
    }

    private int dp(int value) {
        return Math.round(value * activity.getResources().getDisplayMetrics().density);
    }

    private static final class Choice {
        private final String key;
        private final String[] values;
        private final Spinner spinner;
        private int initialSelection;
        private boolean edited;

        Choice(String key, String[] values, Spinner spinner) {
            this.key = key;
            this.values = values;
            this.spinner = spinner;
            spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    if (position != initialSelection) {
                        edited = true;
                    }
                }

                @Override
                public void onNothingSelected(AdapterView<?> parent) {
                }
            });
        }

        void load(String toml) {
            String current = TomlRows.get(toml, key);
            initialSelection = selectionOf(current);
            edited = false;
            spinner.setSelection(initialSelection);
        }

        private int selectionOf(String current) {
            for (int i = 1; i < values.length; i++) {
                if (values[i].equals(current)) {
                    return i;
                }
            }
            return 0;
        }

        String apply(String toml) {
            int selection = spinner.getSelectedItemPosition();
            // Unknown literals look like Auto until changed, but must survive an untouched row.
            if (!edited && selection == initialSelection) {
                return toml;
            }
            String value = values[selection];
            return value == null ? TomlRows.remove(toml, key) : TomlRows.set(toml, key, value);
        }
    }
}
