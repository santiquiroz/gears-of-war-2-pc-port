// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, Santiago Quiroz Upegui and the gow2-recomp contributors
package com.santiquiroz.gow2recomp;

import android.app.Activity;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.os.Build;
import android.util.TypedValue;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import com.santiquiroz.gow2recomp.setup.ByteSize;

import java.io.File;

final class GameSetupScreen {
    interface Actions {
        void onChooseFolder();

        void onChooseIso();

        void onCheckAgain();
    }

    static final int PROGRESS_SCALE = 1000;
    private static final int PADDING_DP = 32;
    private static final int GAP_DP = 16;
    private static final int TITLE_SP = 24;
    private static final int BODY_SP = 16;

    private final Activity activity;
    private final Button chooseButton;
    private final Button isoButton;
    private final Button checkButton;
    private final ProgressBar progressBar;
    private final TextView statusText;
    private final ScrollView root;

    GameSetupScreen(Activity activity, File gameDir, Actions actions) {
        this.activity = activity;
        chooseButton = button(R.string.setup_choose_folder, v -> actions.onChooseFolder());
        isoButton = button(R.string.setup_choose_iso, v -> actions.onChooseIso());
        checkButton = button(R.string.setup_check_again, v -> actions.onCheckAgain());
        progressBar = new ProgressBar(activity, null, android.R.attr.progressBarStyleHorizontal);
        statusText = text("", BODY_SP);
        root = new ScrollView(activity);
        root.addView(column(gameDir));
        applySafeAreaPadding();
        showChoice("");
    }

    // targetSdk 36 is always edge-to-edge, so pad the root by bars and cutout.
    private void applySafeAreaPadding() {
        root.setOnApplyWindowInsetsListener((view, insets) -> {
            Rect safe = safeArea(insets);
            view.setPadding(safe.left, safe.top, safe.right, safe.bottom);
            return insets;
        });
    }

    static Rect safeArea(WindowInsets insets) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            android.graphics.Insets bars = insets.getInsets(
                    WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            return new Rect(bars.left, bars.top, bars.right, bars.bottom);
        }
        return legacySafeArea(insets);
    }

    @SuppressWarnings("deprecation")
    private static Rect legacySafeArea(WindowInsets insets) {
        int left = insets.getSystemWindowInsetLeft();
        int top = insets.getSystemWindowInsetTop();
        int right = insets.getSystemWindowInsetRight();
        int bottom = insets.getSystemWindowInsetBottom();
        DisplayCutout cutout = insets.getDisplayCutout();
        if (cutout == null) {
            return new Rect(left, top, right, bottom);
        }
        return new Rect(Math.max(left, cutout.getSafeInsetLeft()),
                Math.max(top, cutout.getSafeInsetTop()),
                Math.max(right, cutout.getSafeInsetRight()),
                Math.max(bottom, cutout.getSafeInsetBottom()));
    }

    View view() {
        return root;
    }

    void showChoice(String message) {
        setButtonsEnabled(true);
        progressBar.setVisibility(View.GONE);
        statusText.setText(message);
        if (message == null || message.isEmpty()) {
            statusText.setVisibility(View.GONE);
        } else {
            statusText.setVisibility(View.VISIBLE);
        }
        chooseButton.requestFocus();
    }

    void showCopying() {
        setButtonsEnabled(false);
        progressBar.setIndeterminate(true);
        progressBar.setVisibility(View.VISIBLE);
        statusText.setVisibility(View.VISIBLE);
        statusText.setText(R.string.setup_copying);
    }

    void showProgress(long copiedBytes, long totalBytes, String relativePath) {
        progressBar.setIndeterminate(false);
        progressBar.setMax(PROGRESS_SCALE);
        progressBar.setProgress(scaledProgress(copiedBytes, totalBytes));
        statusText.setVisibility(View.VISIBLE);
        statusText.setText(activity.getString(R.string.setup_progress,
                ByteSize.format(copiedBytes), ByteSize.format(totalBytes), relativePath));
    }

    static int scaledProgress(long copiedBytes, long totalBytes) {
        if (totalBytes <= 0) {
            return PROGRESS_SCALE;
        }
        long scaled = Math.max(0, copiedBytes) * PROGRESS_SCALE / totalBytes;
        return (int) Math.min(PROGRESS_SCALE, scaled);
    }

    private LinearLayout column(File gameDir) {
        LinearLayout column = new LinearLayout(activity);
        column.setOrientation(LinearLayout.VERTICAL);
        int padding = dp(PADDING_DP);
        column.setPadding(padding, padding, padding, padding);
        column.addView(text(activity.getString(R.string.setup_title), TITLE_SP));
        column.addView(buttonRow(), spaced());
        column.addView(progressBar, spaced());
        column.addView(statusText, spaced());
        column.addView(text(activity.getString(R.string.setup_explanation), BODY_SP), spaced());
        column.addView(pathText(gameDir), spaced());
        return column;
    }

    private LinearLayout buttonRow() {
        LinearLayout row = new LinearLayout(activity);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.addView(chooseButton);
        row.addView(isoButton);
        row.addView(checkButton);
        return row;
    }

    private Button button(int labelRes, View.OnClickListener listener) {
        Button button = new Button(activity);
        button.setText(labelRes);
        button.setOnClickListener(listener);
        return button;
    }

    private TextView text(String value, int sizeSp) {
        TextView view = new TextView(activity);
        view.setText(value);
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, sizeSp);
        return view;
    }

    private TextView pathText(File gameDir) {
        TextView view = text(gameDir.getAbsolutePath(), BODY_SP);
        view.setTypeface(Typeface.MONOSPACE);
        view.setTextIsSelectable(true);
        return view;
    }

    private LinearLayout.LayoutParams spaced() {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        params.topMargin = dp(GAP_DP);
        return params;
    }

    private void setButtonsEnabled(boolean enabled) {
        chooseButton.setEnabled(enabled);
        isoButton.setEnabled(enabled);
        checkButton.setEnabled(enabled);
    }

    private int dp(int value) {
        return Math.round(value * activity.getResources().getDisplayMetrics().density);
    }
}
