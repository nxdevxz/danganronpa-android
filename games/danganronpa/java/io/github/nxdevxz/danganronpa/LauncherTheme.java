package io.github.nxdevxz.danganronpa;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

/** Shared launcher palette; does not change the game's renderer or touch overlay. */
final class LauncherTheme {
    static final int BACKGROUND = Color.rgb(14, 12, 16);
    static final int PANEL = Color.rgb(38, 32, 40);
    static final int ACCENT = Color.rgb(255, 54, 148);
    static final int INK = Color.rgb(17, 12, 16);
    static final int TEXT = Color.rgb(251, 247, 246);
    static final int MUTED = Color.rgb(203, 186, 197);

    private LauncherTheme() {}

    static int dp(android.content.Context context, int value) {
        return Math.round(value * context.getResources().getDisplayMetrics().density);
    }

    static void apply(Activity activity) {
        activity.getWindow().setStatusBarColor(BACKGROUND);
        activity.getWindow().setNavigationBarColor(BACKGROUND);
    }

    static void button(Button button, boolean primary) {
        button.setAllCaps(false);
        button.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        button.setTextSize(14);
        button.setStateListAnimator(null);
        button.setMinHeight(dp(button.getContext(), 48));
        button.setPadding(dp(button.getContext(), 14), dp(button.getContext(), 10),
                dp(button.getContext(), 14), dp(button.getContext(), 10));
        button.setBackgroundTintList(null);
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(primary ? ACCENT : PANEL);
        shape.setCornerRadius(dp(button.getContext(), 3));
        if (!primary) shape.setStroke(dp(button.getContext(), 1), 0xFF705665);
        button.setBackground(new RippleDrawable(ColorStateList.valueOf(0x33FFFFFF), shape, null));
        button.setTextColor(new ColorStateList(
                new int[][] {new int[] {-android.R.attr.state_enabled}, new int[] {}},
                new int[] {MUTED, primary ? INK : TEXT}));
    }

    static void section(LinearLayout root, String text) {
        TextView heading = new TextView(root.getContext());
        heading.setText(text);
        heading.setTextColor(MUTED);
        heading.setTextSize(12);
        heading.setTypeface(Typeface.DEFAULT_BOLD);
        heading.setLetterSpacing(0.08f);
        heading.setPadding(0, dp(root.getContext(), 14), 0, dp(root.getContext(), 6));
        root.addView(heading);
    }
}
