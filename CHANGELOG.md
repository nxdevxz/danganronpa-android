# Changelog

## 0.5.2 Beta

### Launcher and controls

- Launcher settings and Performance monitor now have their own sections on the main screen.
- Graphics and control settings now show all available choices in a list. The Play button stays visible while you scroll.
- The in-game quick menu now contains only the on-screen controls toggle. The FPS limit is available in the launcher.
- Restored the previous P4G renderer and removed the experimental FSR, shadow, bloom, and high-resolution HUD settings.

### Game setup and storage

- Choose whether to install the game in `Android/data` or play directly from an NSP or XCI file.
- If the update file becomes unavailable in direct mode, select it again or prepare the base game on its own.
- Setup now shows extraction progress, shader generation progress, and available storage in Portuguese when Portuguese is selected.

### Installation checks and settings

- Returning to the launcher within five minutes of a successful installation check can trigger a quick check. Changes to the installation manifest, a new preparation run, or a launch failure trigger a full check.
- The launcher replaces the settings file only after the new file has been written successfully. If saving fails, the previous settings are preserved.
