package io.github.p4grecomp;

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.ScrollView;
import android.widget.TextView;

/** Dark launcher-style replacement for platform AlertDialog. */
final class P4gDialog extends Dialog {
    private P4gDialog(Context context, Builder spec) {
        super(context);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setCancelable(spec.cancelable);
        setCanceledOnTouchOutside(spec.cancelable);
        if (spec.cancelClick != null) setOnCancelListener(spec.cancelClick);

        LinearLayout card = new LinearLayout(context);
        card.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(context, 20);
        card.setPadding(pad, pad, pad, pad);
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(P4gTheme.BACKGROUND);
        shape.setCornerRadius(dp(context, 22));
        shape.setStroke(dp(context, 1), 0x554C5566);
        card.setBackground(shape);

        if (spec.title != null && spec.title.length() != 0) {
            TextView title = text(context, spec.title, 21, P4gTheme.YELLOW);
            title.setTypeface(android.graphics.Typeface.create("sans-serif-medium", android.graphics.Typeface.NORMAL));
            title.setPadding(0, 0, 0, dp(context, 10));
            card.addView(title);
        }
        if (spec.message != null && spec.message.length() != 0) {
            TextView message = text(context, spec.message, 15, P4gTheme.TEXT);
            message.setLineSpacing(0, 1.12f);
            message.setPadding(0, 0, 0, dp(context, 12));
            card.addView(message);
        }
        if (spec.view != null) card.addView(spec.view, new LinearLayout.LayoutParams(-1, -2));
        if (spec.items != null) addChoices(card, spec);

        if (spec.neutral != null || spec.negative != null || spec.positive != null) {
            LinearLayout actions = new LinearLayout(context);
            actions.setGravity(Gravity.END);
            actions.setPadding(0, dp(context, 14), 0, 0);
            if (spec.neutral != null)
                actions.addView(action(spec.neutral, false, DialogInterface.BUTTON_NEUTRAL, spec.neutralClick));
            if (spec.negative != null)
                actions.addView(action(spec.negative, false, DialogInterface.BUTTON_NEGATIVE, spec.negativeClick));
            if (spec.positive != null)
                actions.addView(action(spec.positive, true, DialogInterface.BUTTON_POSITIVE, spec.positiveClick));
            card.addView(actions, new LinearLayout.LayoutParams(-1, -2));
        }

        ScrollView scroll = new ScrollView(context);
        scroll.setFillViewport(true);
        scroll.addView(card);
        setContentView(scroll);
        Window window = getWindow();
        if (window != null) {
            window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            window.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            WindowManager.LayoutParams attributes = window.getAttributes();
            attributes.dimAmount = 0.72f;
            window.setAttributes(attributes);
        }
    }

    @Override public void show() {
        super.show();
        Window window = getWindow();
        if (window != null) window.setLayout(
                Math.min(getContext().getResources().getDisplayMetrics().widthPixels - dp(getContext(), 28), dp(getContext(), 520)),
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private void addChoices(LinearLayout card, Builder spec) {
        for (int i = 0; i < spec.items.length; i++) {
            final int index = i;
            if (spec.multiChecked != null) {
                CheckBox choice = new CheckBox(getContext());
                choice.setText(spec.items[i]);
                choice.setTextColor(P4gTheme.TEXT);
                choice.setTextSize(15);
                choice.setMinHeight(dp(getContext(), 44));
                choice.setChecked(spec.multiChecked[i]);
                choice.setButtonTintList(android.content.res.ColorStateList.valueOf(P4gTheme.YELLOW));
                choice.setOnCheckedChangeListener((button, checked) -> {
                    spec.multiChecked[index] = checked;
                    if (spec.multiClick != null) spec.multiClick.onClick(this, index, checked);
                });
                card.addView(choice, new LinearLayout.LayoutParams(-1, -2));
            } else if (spec.singleChecked >= 0) {
                RadioButton choice = new RadioButton(getContext());
                choice.setText(spec.items[i]);
                choice.setTextColor(P4gTheme.TEXT);
                choice.setTextSize(15);
                choice.setMinHeight(dp(getContext(), 44));
                choice.setChecked(i == spec.singleChecked);
                choice.setButtonTintList(android.content.res.ColorStateList.valueOf(P4gTheme.YELLOW));
                choice.setOnClickListener(v -> {
                    if (spec.itemClick != null) spec.itemClick.onClick(this, index);
                });
                card.addView(choice, new LinearLayout.LayoutParams(-1, -2));
            } else if (spec.itemClick == null) {
                TextView item = text(getContext(), spec.items[i], 14, P4gTheme.TEXT);
                item.setPadding(dp(getContext(), 4), dp(getContext(), 9), dp(getContext(), 4), dp(getContext(), 9));
                card.addView(item, new LinearLayout.LayoutParams(-1, -2));
            } else {
                Button item = new Button(getContext());
                item.setText(spec.items[i]);
                P4gTheme.button(item, false);
                LinearLayout.LayoutParams layout = new LinearLayout.LayoutParams(-1, -2);
                layout.bottomMargin = dp(getContext(), 6);
                card.addView(item, layout);
                item.setOnClickListener(v -> {
                    dismiss();
                    spec.itemClick.onClick(this, index);
                });
            }
        }
    }

    private Button action(CharSequence label, boolean primary, int which, DialogInterface.OnClickListener listener) {
        Button button = new Button(getContext());
        button.setText(label);
        P4gTheme.button(button, primary);
        LinearLayout.LayoutParams layout = new LinearLayout.LayoutParams(-2, -2);
        layout.leftMargin = dp(getContext(), 8);
        button.setLayoutParams(layout);
        button.setOnClickListener(v -> {
            dismiss();
            if (listener != null) listener.onClick(this, which);
        });
        return button;
    }

    private static TextView text(Context context, CharSequence value, int size, int color) {
        TextView view = new TextView(context);
        view.setText(value);
        view.setTextSize(size);
        view.setTextColor(color);
        return view;
    }

    private static int dp(Context context, int value) { return P4gTheme.dp(context, value); }

    static final class Builder {
        final Context context;
        CharSequence title, message, positive, negative, neutral;
        DialogInterface.OnClickListener positiveClick, negativeClick, neutralClick, itemClick;
        DialogInterface.OnMultiChoiceClickListener multiClick;
        DialogInterface.OnCancelListener cancelClick;
        CharSequence[] items;
        boolean[] multiChecked;
        int singleChecked = -1;
        View view;
        boolean cancelable = true;

        Builder(Context context) { this.context = context; }
        Builder setTitle(CharSequence value) { title = value; return this; }
        Builder setMessage(CharSequence value) { message = value; return this; }
        Builder setView(View value) { view = value; return this; }
        Builder setCancelable(boolean value) { cancelable = value; return this; }
        Builder setOnCancelListener(DialogInterface.OnCancelListener listener) {
            cancelClick = listener; return this;
        }
        Builder setPositiveButton(CharSequence label, DialogInterface.OnClickListener listener) {
            positive = label; positiveClick = listener; return this;
        }
        Builder setNegativeButton(CharSequence label, DialogInterface.OnClickListener listener) {
            negative = label; negativeClick = listener; return this;
        }
        Builder setNeutralButton(CharSequence label, DialogInterface.OnClickListener listener) {
            neutral = label; neutralClick = listener; return this;
        }
        Builder setItems(CharSequence[] values, DialogInterface.OnClickListener listener) {
            items = values; itemClick = listener; return this;
        }
        Builder setSingleChoiceItems(CharSequence[] values, int checked, DialogInterface.OnClickListener listener) {
            items = values; singleChecked = checked; itemClick = listener; return this;
        }
        Builder setMultiChoiceItems(CharSequence[] values, boolean[] checked, DialogInterface.OnMultiChoiceClickListener listener) {
            items = values; multiChecked = checked; multiClick = listener; return this;
        }
        P4gDialog create() { return new P4gDialog(context, this); }
        P4gDialog show() { P4gDialog dialog = create(); dialog.show(); return dialog; }
    }
}
