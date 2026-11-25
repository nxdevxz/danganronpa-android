package io.github.nxdevxz.danganronpa;

import android.content.Context;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/** Launcher language. This does not change the game's language. */
final class Lang {
    private static final String[] CODES = {"en", "pt", "es", "fr", "de"};
    private static final String[] NAMES = {"English", "Português (Brasil)", "Español", "Français", "Deutsch"};
    private static final Map<String, String> ES = new HashMap<>();
    private static final Map<String, String> FR = new HashMap<>();
    private static final Map<String, String> DE = new HashMap<>();
    private static String language = "en";

    static {
        add("Launcher settings", "Configuración del launcher", "Paramètres du launcher", "Launcher-Einstellungen");
        add("Language and orientation", "Idioma y orientación", "Langue et orientation", "Sprache und Ausrichtung");
        add("Launcher language", "Idioma del launcher", "Langue du launcher", "Launcher-Sprache");
        add("Launcher orientation", "Orientación del launcher", "Orientation du launcher", "Launcher-Ausrichtung");
        add("Landscape", "Horizontal", "Paysage", "Querformat");
        add("Portrait", "Vertical", "Portrait", "Hochformat");
        add("Reset settings", "Restablecer configuración", "Réinitialiser les paramètres", "Einstellungen zurücksetzen");
        add("Reset", "Restablecer", "Réinitialiser", "Zurücksetzen");
        add("Reset the launcher language and orientation?", "¿Restablecer el idioma y la orientación del launcher?",
                "Réinitialiser la langue et l’orientation du launcher ?", "Launcher-Sprache und -Ausrichtung zurücksetzen?");
        add("Reset this section's settings to defaults?", "¿Restablecer la configuración predeterminada de esta sección?",
                "Rétablir les paramètres par défaut de cette section ?", "Einstellungen dieses Bereichs zurücksetzen?");
        add("Cancel", "Cancelar", "Annuler", "Abbrechen");
        add("Close", "Cerrar", "Fermer", "Schließen");
        add("Back", "Volver", "Retour", "Zurück");
        add("Choose", "Elegir", "Choisir", "Auswählen");
        add("Select", "Seleccionar", "Sélectionner", "Auswählen");
        add("Open", "Abrir", "Ouvrir", "Öffnen");
        add("Save", "Guardar", "Enregistrer", "Speichern");
        add("Import", "Importar", "Importer", "Importieren");
        add("Export", "Exportar", "Exporter", "Exportieren");
        add("Manage", "Gestionar", "Gérer", "Verwalten");
        add("Prepare", "Preparar", "Préparer", "Vorbereiten");
        add("Play", "Jugar", "Jouer", "Spielen");
        add("On", "Activado", "Activé", "Ein");
        add("Off", "Desactivado", "Désactivé", "Aus");
        add("Game setup", "Preparación del juego", "Préparation du jeu", "Spieleinrichtung");
        add("Game file (.nsp / .xci)", "Archivo del juego (.nsp / .xci)", "Fichier du jeu (.nsp / .xci)", "Spieldatei (.nsp / .xci)");
        add("Danganronpa update .nsp (optional)", "Actualización de Danganronpa .nsp (opcional)", "Mise à jour Danganronpa .nsp (facultative)", "Danganronpa-Update .nsp (optional)");
        add("Keys (prod.keys)", "Claves (prod.keys)", "Clés (prod.keys)", "Schlüssel (prod.keys)");
        add("Quick settings", "Ajustes rápidos", "Réglages rapides", "Schnelleinstellungen");
        add("Configuration presets", "Preajustes de configuración", "Préréglages de configuration", "Konfigurationsprofile");
        add("Choose between frame rate, balance and image quality.",
                "Elige entre fluidez, equilibrio y calidad de imagen.",
                "Choisissez entre fluidité, équilibre et qualité d’image.",
                "Wähle zwischen Bildrate, Ausgewogenheit und Bildqualität.");
        add("Performance", "Rendimiento", "Performances", "Leistung");
        add("Quality", "Calidad", "Qualité", "Qualität");
        add("Apply", "Aplicar", "Appliquer", "Anwenden");
        add("Preset applied", "Preajuste aplicado", "Préréglage appliqué", "Profil angewendet");
        add("Game menu", "Menú del juego", "Menu du jeu", "Spielmenü");
        add("Continue", "Continuar", "Continuer", "Fortsetzen");
        add("Exit", "Salir", "Quitter", "Beenden");
        add("Exit the game?", "¿Salir del juego?", "Quitter le jeu ?", "Spiel beenden?");
        add("Exit and return to the launcher? Unsaved progress may be lost.",
                "¿Salir y volver al launcher? Se puede perder el progreso no guardado.",
                "Quitter et revenir au launcher ? La progression non sauvegardée peut être perdue.",
                "Beenden und zum Launcher zurückkehren? Ungespeicherter Fortschritt kann verloren gehen.");
        add("Quick changes apply the next time the game opens.",
                "Los cambios rápidos se aplican la próxima vez que se abre el juego.",
                "Les changements rapides s’appliquent au prochain lancement du jeu.",
                "Schnelle Änderungen gelten beim nächsten Spielstart.");
        add("Display, performance and controls", "Pantalla, rendimiento y controles", "Affichage, performances et commandes", "Anzeige, Leistung und Steuerung");
        add("Advanced graphics", "Gráficos avanzados", "Graphismes avancés", "Erweiterte Grafik");
        add("Driver and experimental settings", "Driver y ajustes experimentales", "Pilote et paramètres expérimentaux", "Treiber und experimentelle Einstellungen");
        add("Controls", "Controles", "Commandes", "Steuerung");
        add("Touch, sensitivity and layout", "Toque, sensibilidad y diseño", "Tactile, sensibilité et disposition", "Touch, Empfindlichkeit und Layout");
        add("In-game statistics", "Estadísticas durante el juego", "Statistiques en jeu", "Statistiken im Spiel");
        add("Settings profiles", "Perfiles de configuración", "Profils de paramètres", "Einstellungsprofile");
        add("Game, saves & mods", "Juego, partidas y mods", "Jeu, sauvegardes et mods", "Spiel, Spielstände und Mods");
        add("Updates, backups and content", "Actualizaciones, copias y contenido", "Mises à jour, sauvegardes et contenu", "Updates, Sicherungen und Inhalte");
        add("Information & support", "Información y soporte", "Informations et assistance", "Informationen und Hilfe");
        add("Diagnostics and community", "Diagnóstico y comunidad", "Diagnostic et communauté", "Diagnose und Community");
        add("Game update", "Actualización del juego", "Mise à jour du jeu", "Spiel-Update");
        add("Original base game (NSP/XCI)", "Juego base original (NSP/XCI)", "Jeu de base original (NSP/XCI)", "Originales Basisspiel (NSP/XCI)");
        add("Update file (.nsp)", "Archivo de actualización (.nsp)", "Fichier de mise à jour (.nsp)", "Update-Datei (.nsp)");
        add("Apply update", "Aplicar actualización", "Appliquer la mise à jour", "Update anwenden");
        add("Saves & mods", "Partidas y mods", "Sauvegardes et mods", "Spielstände und Mods");
        add("Logs & diagnostics", "Registros y diagnóstico", "Journaux et diagnostic", "Protokolle und Diagnose");
        add("Community", "Comunidad", "Communauté", "Community");
        add("Join", "Unirse", "Rejoindre", "Beitreten");
        add("Report an issue", "Informar de un problema", "Signaler un problème", "Problem melden");
        add("Output resolution", "Resolución de salida", "Résolution de sortie", "Ausgabeauflösung");
        add("Screen format", "Formato de pantalla", "Format d’écran", "Bildformat");
        add("Stretch", "Estirar", "Étirer", "Strecken");
        add("FPS limit", "Límite de FPS", "Limite de FPS", "FPS-Limit");
        add("Anisotropic filtering", "Filtrado anisotrópico", "Filtrage anisotrope", "Anisotrope Filterung");
        add("Off (best performance)", "Desactivado (máximo rendimiento)", "Désactivé (performances maximales)", "Aus (beste Leistung)");
        add("2x (faster)", "2x (rápido)", "2x (rapide)", "2x (schneller)");
        add("4x (default)", "4x (predeterminado)", "4x (par défaut)", "4x (Standard)");
        add("8x (higher quality)", "8x (mayor calidad)", "8x (meilleure qualité)", "8x (höhere Qualität)");
        add("16x (best quality)", "16x (máxima calidad)", "16x (qualité maximale)", "16x (beste Qualität)");
        add("Overlay size", "Tamaño del overlay", "Taille de l’overlay", "Overlay-Größe");
        add("100% (default)", "100% (predeterminado)", "100% (par défaut)", "100% (Standard)");
        add("Detailed performance logging", "Registro detallado de rendimiento", "Journal détaillé des performances", "Detaillierte Leistungsprotokolle");
        add("Pipeline prewarming (experimental)", "Precalentamiento de pipelines (experimental)", "Préchauffage des pipelines (expérimental)", "Pipeline-Vorwärmung (experimentell)");
        add("Background", "Segundo plano", "Arrière-plan", "Hintergrund");
        add("Intensive (more CPU)", "Intensivo (más CPU)", "Intensif (plus de CPU)", "Intensiv (mehr CPU)");
        add("Background prewarm load", "Carga de precalentamiento en segundo plano", "Charge du préchauffage en arrière-plan", "Hintergrundlast der Vorwärmung");
        add("Light", "Ligera", "Légère", "Leicht");
        add("Standard", "Estándar", "Standard", "Standard");
        add("CPU affinity (experimental)", "Afinidad de CPU (experimental)", "Affinité CPU (expérimentale)", "CPU-Affinität (experimentell)");
        add("Android scheduler", "Planificador de Android", "Planificateur Android", "Android-Scheduler");
        add("Optimized (recommended)", "Optimizada (recomendada)", "Optimisée (recommandée)", "Optimiert (empfohlen)");
        add("Split frame threads", "Separar hilos de fotogramas", "Séparer les threads d’image", "Frame-Threads trennen");
        add("Disable motion blur (experimental)", "Desactivar desenfoque de movimiento (experimental)", "Désactiver le flou de mouvement (expérimental)", "Bewegungsunschärfe deaktivieren (experimentell)");
        add("3D texture sharpness (experimental)", "Nitidez de texturas 3D (experimental)", "Netteté des textures 3D (expérimentale)", "3D-Texturschärfe (experimentell)");
        add("Original", "Original", "Originale", "Original");
        add("Sharper", "Más nítidas", "Plus nettes", "Schärfer");
        add("Sharper (-1, test)", "Más nítidas (-1, prueba)", "Plus nettes (-1, test)", "Schärfer (-1, Test)");
        add("On-screen controls", "Controles en pantalla", "Commandes à l’écran", "Bildschirmsteuerung");
        add("Left analog mode", "Modo del analógico izquierdo", "Mode du stick gauche", "Modus des linken Analogsticks");
        add("Fixed", "Fijo", "Fixe", "Fest");
        add("Floating", "Flotante", "Flottant", "Schwebend");
        add("Button size", "Tamaño de botones", "Taille des boutons", "Tastengröße");
        add("Small", "Pequeño", "Petite", "Klein");
        add("Normal", "Normal", "Normale", "Normal");
        add("Large", "Grande", "Grande", "Groß");
        add("Button opacity", "Opacidad de botones", "Opacité des boutons", "Tastendeckkraft");
        add("Low", "Baja", "Faible", "Niedrig");
        add("High", "Alta", "Élevée", "Hoch");
        add("Touch stick sensitivity", "Sensibilidad del analógico táctil", "Sensibilité du stick tactile", "Touch-Stick-Empfindlichkeit");
        add("Touch camera sensitivity", "Sensibilidad táctil de la cámara", "Sensibilité tactile de la caméra", "Touch-Kameraempfindlichkeit");
        add("GPU driver", "Driver de GPU", "Pilote GPU", "GPU-Treiber");
        add("System driver", "Driver del sistema", "Pilote système", "Systemtreiber");
        add("Controls position", "Posición de los controles", "Position des commandes", "Steuerungsposition");
        add("Edit layout", "Editar diseño", "Modifier la disposition", "Layout bearbeiten");
        add("Translation / CPK mod", "Traducción / Mod CPK", "Traduction / Mod CPK", "Übersetzung / CPK-Mod");
        add("Import CPK", "Importar CPK", "Importer CPK", "CPK importieren");
        add("Mods and translations", "Mods y traducciones", "Mods et traductions", "Mods und Übersetzungen");
        add("Retry", "Reintentar", "Réessayer", "Erneut versuchen");
        add("Enabled", "Activado", "Activé", "Aktiviert");
        add("Disabled", "Desactivado", "Désactivé", "Deaktiviert");
        add("Delete", "Eliminar", "Supprimer", "Löschen");
        add("Error", "Error", "Erreur", "Fehler");
        add("Checking preparation…", "Comprobando preparación…", "Vérification de la préparation…", "Vorbereitung wird geprüft…");
        add("No game selected", "Ningún juego seleccionado", "Aucun jeu sélectionné", "Kein Spiel ausgewählt");
        add("selected", "seleccionado", "sélectionné", "ausgewählt");
        add("not selected", "no seleccionado", "non sélectionné", "nicht ausgewählt");
        add("Share", "Compartir", "Partager", "Teilen");
        add("Share logs", "Compartir registros", "Partager les journaux", "Protokolle teilen");
    }

    private Lang() {}

    private static void add(String english, String spanish, String french, String german) {
        ES.put(english, spanish);
        FR.put(english, french);
        DE.put(english, german);
    }

    static void load(Context context) {
        language = normalize(context.getSharedPreferences("launcher", Context.MODE_PRIVATE)
                .getString("language", "en"));
    }

    static String[] names() { return NAMES.clone(); }
    static String currentName() { return NAMES[currentIndex()]; }

    static int currentIndex() {
        for (int i = 0; i < CODES.length; i++) if (CODES[i].equals(language)) return i;
        return 0;
    }

    static void select(Context context, int index) {
        if (index < 0 || index >= CODES.length) index = 0;
        language = CODES[index];
        context.getSharedPreferences("launcher", Context.MODE_PRIVATE).edit()
                .putString("language", language).apply();
    }

    static String t(String portugueseText, String english) {
        if (language.equals("pt")) return portugueseText;
        if (language.equals("es")) return translated(ES, english);
        if (language.equals("fr")) return translated(FR, english);
        if (language.equals("de")) return translated(DE, english);
        return english;
    }

    static String f(String portugueseText, String english, Object... args) {
        return String.format(locale(), t(portugueseText, english), args);
    }

    private static String translated(Map<String, String> translations, String english) {
        String value = translations.get(english);
        return value == null ? english : value;
    }

    private static Locale locale() {
        if (language.equals("pt")) return new Locale("pt", "BR");
        if (language.equals("es")) return new Locale("es");
        if (language.equals("fr")) return Locale.FRENCH;
        if (language.equals("de")) return Locale.GERMAN;
        return Locale.ENGLISH;
    }

    private static String normalize(String code) {
        if (code != null) for (String supported : CODES) if (supported.equals(code)) return code;
        return "en";
    }
}
