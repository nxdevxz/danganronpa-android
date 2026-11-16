package io.github.p4grecomp;

import android.app.NativeActivity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.system.Os;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.File;
import java.util.Map;

/** The runtime lives in :game; its native exit must not kill the launcher. */
public class GameActivity extends NativeActivity {
    private P4gDialog exitDialog;
    private Object backCallback;
    private PerformanceOverlay performanceOverlay;
    static native long nativeFrames();

    @Override protected void onCreate(Bundle state) {
        // The installed payload is checked by the launcher; no source URI, NSP descriptor or keys are opened here.
        try {
            File data = Launcher.dataDir();
            // Android draws the metrics panel; avoid duplicating the native FPS digits.
            Os.setenv("NX_SHOW_FPS", "0", true);
            File installed = Launcher.installationDir(this);
            if (!InstalledGameFiles.present(installed))
                throw new java.io.IOException("Game installation is incomplete. Install the game again from its NSP/XCI.");
            Os.unsetenv("NX_GAME_FD");
            Os.unsetenv("NX_GAME_NSP");
            Os.unsetenv("NX_GAME_NAME");
            Os.setenv("NX_P4G_INSTALLED", "1", true);
            Os.setenv("NX_P4G_GAME_ROOT", InstalledGameFiles.game(installed).getCanonicalPath(), true);
            try (java.io.FileOutputStream out = new java.io.FileOutputStream(new File(getFilesDir(), "data_root.txt"))) {
                out.write((data.getCanonicalPath() + "\n").getBytes("UTF-8"));
            }
        } catch (Exception e) {
            startActivity(menuIntent().putExtra("game_error", String.valueOf(e.getMessage())));
            android.os.Process.killProcess(android.os.Process.myPid());
            return;
        }
        super.onCreate(state);
        Lang.load(this);
        performanceOverlay = new PerformanceOverlay(this,
                Launcher.readConfig(new File(Launcher.dataDir(), "config.txt")));
        if (Build.VERSION.SDK_INT >= 33) backCallback = Api33.register(this);
    }

    private Intent menuIntent() {
        return new Intent(this, Launcher.class).addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
    }

    /** Also called by the native input queue when it receives phone Back. */
    public void confirmExit() {
        runOnUiThread(() -> {
            if (isFinishing() || isDestroyed() || (exitDialog != null && exitDialog.isShowing())) return;
            Map<String, String> config = Launcher.readConfig(new File(Launcher.dataDir(), "config.txt"));
            LinearLayout quick = new LinearLayout(this);
            quick.setOrientation(LinearLayout.VERTICAL);
            P4gTheme.section(quick, Lang.t("Configurações rápidas", "Quick settings"));
            addQuickSetting(quick, config, Lang.t("Resolução de saída", "Output resolution"), "RESOLUTION",
                    new String[] {"480p", "720p", "1080p"}, "720p");
            addQuickSetting(quick, config, Lang.t("Limite de FPS", "FPS limit"), "NX_FPS_CAP",
                    new String[] {"30", "45", "60"}, "30");
            addQuickSetting(quick, config, Lang.t("Filtragem anisotrópica", "Anisotropic filtering"), "NX_MAX_ANISO",
                    new String[] {"1", "2", "4", "8", "16"}, "4");
            addQuickSetting(quick, config, Lang.t("Controles na tela", "On-screen controls"), "NX_TOUCH_SHOW",
                    new String[] {"0", "1"}, "1");
            exitDialog = new P4gDialog.Builder(this)
                .setTitle(Lang.t("Menu do jogo", "Game menu"))
                .setMessage(Lang.t("As mudanças rápidas serão aplicadas na próxima vez que o jogo for aberto. Sair agora pode perder o progresso não salvo.",
                        "Quick changes apply the next time the game opens. Exiting now may lose unsaved progress."))
                .setView(quick)
                .setNegativeButton(Lang.t("Continuar", "Continue"), null)
                .setPositiveButton(Lang.t("Sair", "Exit"), (dialog, which) -> {
                    startActivity(menuIntent());
                    // NativeActivity destruction stops the runtime. Since it is
                    // in :game, the launcher's process and menu stay alive.
                    finish();
                }).create();
            exitDialog.show();
        });
    }

    private void addQuickSetting(LinearLayout root, Map<String, String> config, String label, String key,
            String[] values, String fallback) {
        LinearLayout row = new LinearLayout(this);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, P4gTheme.dp(this, 2), 0, P4gTheme.dp(this, 2));
        TextView name = new TextView(this);
        name.setText(label);
        name.setTextColor(P4gTheme.TEXT);
        name.setTextSize(15);
        row.addView(name, new LinearLayout.LayoutParams(0, -2, 1));
        Button value = new Button(this);
        P4gTheme.button(value, false);
        String current = config.get(key);
        int selected = 0;
        for (int i = 0; i < values.length; i++) if (values[i].equals(current == null ? fallback : current)) selected = i;
        final int[] index = {selected};
        value.setText(quickLabel(key, values[index[0]]));
        value.setOnClickListener(v -> {
            index[0] = (index[0] + 1) % values.length;
            String selectedValue = values[index[0]];
            if ("RESOLUTION".equals(key)) Launcher.putResolution(config, selectedValue);
            else config.put(key, selectedValue);
            value.setText(quickLabel(key, selectedValue));
            Launcher.writeConfig(new File(Launcher.dataDir(), "config.txt"), config);
        });
        row.addView(value, new LinearLayout.LayoutParams(P4gTheme.dp(this, 126), -2));
        root.addView(row);
    }

    private String quickLabel(String key, String value) {
        if ("NX_TOUCH_SHOW".equals(key)) return "1".equals(value) ? Lang.t("Ativados", "On") : Lang.t("Desativados", "Off");
        if ("NX_MAX_ANISO".equals(key) && "1".equals(value)) return Lang.t("Desativada", "Off");
        return "NX_MAX_ANISO".equals(key) ? value + "x" : value + ("NX_FPS_CAP".equals(key) ? " FPS" : "");
    }
    @Override public void onBackPressed() { confirmExit(); }
    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        int source = event.getSource();
        boolean controller = (source & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
                             (source & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK && !controller) {
            if (event.getAction() == KeyEvent.ACTION_UP) confirmExit();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }
    @Override protected void onResume() {
        super.onResume();
        if (performanceOverlay != null) performanceOverlay.start();
    }
    @Override protected void onPause() {
        if (performanceOverlay != null) performanceOverlay.stop();
        super.onPause();
    }
    @Override protected void onDestroy() {
        if (performanceOverlay != null) performanceOverlay.close();
        if (Build.VERSION.SDK_INT >= 33 && backCallback != null) Api33.unregister(this, backCallback);
        if (exitDialog != null) exitDialog.dismiss();
        super.onDestroy();
    }
    private static final class Api33 {
        static Object register(GameActivity activity) {
            android.window.OnBackInvokedCallback callback = activity::confirmExit;
            activity.getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT, callback);
            return callback;
        }
        static void unregister(GameActivity activity, Object callback) {
            activity.getOnBackInvokedDispatcher().unregisterOnBackInvokedCallback(
                (android.window.OnBackInvokedCallback) callback);
        }
    }
}
