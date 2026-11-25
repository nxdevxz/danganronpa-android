package io.github.nxdevxz.danganronpa;

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
    private LauncherDialog exitDialog;
    private Object backCallback;
    private PerformanceOverlay performanceOverlay;
    static native long nativeFrames();

    private void openSource(String uri, String name, String fdKey, String nameKey) throws Exception {
        try (android.os.ParcelFileDescriptor file = getContentResolver().openFileDescriptor(android.net.Uri.parse(uri), "r")) {
            if (file == null) throw new java.io.IOException("Could not open " + name);
            Os.setenv(nameKey, name, true);
            Os.setenv(fdKey, Integer.toString(file.detachFd()), true);
        }
    }

    @Override protected void onCreate(Bundle state) {
        // The installed payload is checked by the launcher; no source URI, NSP descriptor or keys are opened here.
        try {
            File data = Launcher.dataDir();
            // Android draws the metrics panel; avoid duplicating the native FPS digits.
            Os.setenv("NX_SHOW_FPS", "0", true);
            String uri = getIntent().getStringExtra(ImportActivity.EXTRA_URI);
            Os.unsetenv("NX_GAME_UPDATE_FD");
            if (uri != null) {
                Os.unsetenv("NX_DANGANRONPA_INSTALLED");
                Os.unsetenv("NX_DANGANRONPA_GAME_ROOT");
                openSource(uri, getIntent().getStringExtra(ImportActivity.EXTRA_NAME), "NX_GAME_FD", "NX_GAME_NAME");
                String update = getIntent().getStringExtra(ImportActivity.EXTRA_UPDATE_URI);
                if (update != null) openSource(update, getIntent().getStringExtra(ImportActivity.EXTRA_UPDATE_NAME),
                        "NX_GAME_UPDATE_FD", "NX_GAME_UPDATE_NAME");
                Os.setenv("NX_KEYS", Launcher.keysFile(this, "prod.keys").getPath(), true);
                Os.setenv("NX_TITLE_KEYS", Launcher.keysFile(this, "title.keys").getPath(), true);
            } else {
                File installed = Launcher.installationDir(this);
                if (!InstalledGameFiles.present(installed))
                    throw new java.io.IOException("Game installation is incomplete. Install the game again from its NSP/XCI.");
                Os.unsetenv("NX_GAME_FD");
                Os.unsetenv("NX_GAME_NSP");
                Os.unsetenv("NX_GAME_NAME");
                Os.setenv("NX_DANGANRONPA_INSTALLED", "1", true);
                Os.setenv("NX_DANGANRONPA_GAME_ROOT", InstalledGameFiles.game(installed).getCanonicalPath(), true);
            }
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
            LauncherTheme.section(quick, Lang.t("Configurações rápidas", "Quick settings"));
            addQuickSetting(quick, config, Lang.t("Controles na tela", "On-screen controls"), "NX_TOUCH_SHOW",
                    new String[] {"0", "1"}, "1");
            exitDialog = new LauncherDialog.Builder(this)
                .setTitle(Lang.t("Menu do jogo", "Game menu"))
                .setMessage(Lang.t("As mudanças rápidas serão aplicadas na próxima vez que o jogo for aberto.",
                        "Quick changes apply the next time the game opens."))
                .setView(quick)
                .setNegativeButton(Lang.t("Continuar", "Continue"), null)
                .setPositiveButton(Lang.t("Sair", "Exit"), (dialog, which) -> confirmExitGame()).create();
            exitDialog.show();
        });
    }

    private void confirmExitGame() {
        exitDialog = new LauncherDialog.Builder(this)
                .setTitle(Lang.t("Sair do jogo?", "Exit the game?"))
                .setMessage(Lang.t("Quer mesmo sair e voltar ao launcher? O progresso não salvo pode ser perdido.",
                        "Exit and return to the launcher? Unsaved progress may be lost."))
                .setNegativeButton(Lang.t("Cancelar", "Cancel"), null)
                .setPositiveButton(Lang.t("Sair", "Exit"), (dialog, which) -> {
                    startActivity(menuIntent().putExtra("returned_to_menu", true));
                    // NativeActivity destruction stops the runtime. Since it is
                    // in :game, the launcher's process and menu stay alive.
                    finish();
                }).show();
    }

    private void addQuickSetting(LinearLayout root, Map<String, String> config, String label, String key,
            String[] values, String fallback) {
        LinearLayout row = new LinearLayout(this);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, LauncherTheme.dp(this, 2), 0, LauncherTheme.dp(this, 2));
        TextView name = new TextView(this);
        name.setText(label);
        name.setTextColor(LauncherTheme.TEXT);
        name.setTextSize(15);
        row.addView(name, new LinearLayout.LayoutParams(0, -2, 1));
        Button value = new Button(this);
        LauncherTheme.button(value, false);
        String current = config.get(key);
        int selected = 0;
        for (int i = 0; i < values.length; i++) if (values[i].equals(current == null ? fallback : current)) selected = i;
        final int[] index = {selected};
        value.setText(touchLabel(values[index[0]]));
        value.setOnClickListener(v -> {
            index[0] = (index[0] + 1) % values.length;
            String selectedValue = values[index[0]];
            config.put(key, selectedValue);
            value.setText(touchLabel(selectedValue));
            Launcher.writeConfig(new File(Launcher.dataDir(), "config.txt"), config);
        });
        row.addView(value, new LinearLayout.LayoutParams(LauncherTheme.dp(this, 126), -2));
        root.addView(row);
    }

    private String touchLabel(String value) {
        return "1".equals(value) ? Lang.t("Ativados", "On") : Lang.t("Desativados", "Off");
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
