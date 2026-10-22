#include "recovery.h"

#ifdef MEMORIES_IOS
#include "paths.h"
#include "settings.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* The game counts as started once it has drawn this many frames over this
 * many seconds since the settings were applied. */
#ifndef STABLE_SECONDS
#define STABLE_SECONDS 20.0
#endif
#ifndef STABLE_FRAMES
#define STABLE_FRAMES 120
#endif

/* The settings that cost the device the most, or change what it draws: what
 * "reset the graphics settings" puts back. Sound, controls, language and the
 * rest are left as the player set them. */
static const SettingId graphics[] = {
    SET_SCALE, SET_SCALING, SET_ASPECT, SET_FILTER, SET_VSYNC, SET_SPEED, SET_FPS,
    SET_MENU_SCALE, SET_INTERNAL_SCALE, SET_BRIGHTNESS, SET_CONTRAST, SET_SATURATION,
    SET_GAMMA, SET_CRT, SET_FLASH, SET_XBR, SET_HD_TEXT, SET_MSAA, SET_PGXP,
};

static char marker_path[1024];
static int armed;            /* the marker file is there */
static unsigned frames_seen; /* frames drawn since the marker was last armed */
static double first_frame;   /* when the first of them was drawn; 0 before */

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static void arm(void)
{
    frames_seen = 0;
    first_frame = 0;
    if (armed || !marker_path[0]) return;
    {
        FILE *file = fopen(marker_path, "w");
        if (!file) return;
        fputs("The last start of the game had not finished when this file was written.\n", file);
        fclose(file);
        armed = 1;
    }
}

static void disarm(void)
{
    if (armed && marker_path[0]) remove(marker_path);
    armed = 0;
}

static void setting_changed(SettingId id, int value)
{
    size_t i;
    (void)value;
    for (i = 0; i < sizeof(graphics) / sizeof(graphics[0]); i++) {
        if (graphics[i] == id) {
            arm();
            return;
        }
    }
}

/* settings.txt kept as settings.txt.bak, so a reset can be undone by hand
 * (Files > On My iPhone > the app's folder). */
static void back_up_settings(void)
{
    char from[1024], to[1040], buffer[4096];
    FILE *in, *out;
    size_t got;
    if (Paths_User(from, sizeof(from), "settings.txt")) return;
    if (snprintf(to, sizeof(to), "%s.bak", from) >= (int)sizeof(to)) return;
    in = fopen(from, "rb");
    if (!in) return;
    out = fopen(to, "wb");
    if (out) {
        while ((got = fread(buffer, 1, sizeof(buffer), in)) > 0) fwrite(buffer, 1, got, out);
        fclose(out);
    }
    fclose(in);
}

static void mods_off(const char *key, int value, void *context)
{
    (void)context;
    /* "mod.<id>" says whether a mod is applied; "mod.<id>.<key>" is one of
     * its own settings (mods/manager.c), left alone. */
    if (value && !strncmp(key, "mod.", 4) && !strchr(key + 4, '.')) Settings_SetNamed(key, 0);
}

static void reset(int with_mods)
{
    size_t i;
    back_up_settings();
    for (i = 0; i < sizeof(graphics) / sizeof(graphics[0]); i++) {
        Settings_Set(graphics[i], Settings_Default(graphics[i]));
    }
    if (with_mods) Settings_VisitNamed(mods_off, NULL);
    Settings_Save();
}

/* 0 keep, 1 reset the graphics settings, 2 also turn the mods off. -1 if the
 * question could not be shown. */
static int ask(void)
{
    static const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Reset graphics (Đặt lại đồ họa)"},
        {0, 2, "Reset + turn mods off (Tắt mod)"},
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Keep settings (Giữ nguyên)"},
    };
    static const char message[] =
        "The game did not finish starting last time. It may have hung or been closed by iOS, "
        "often because a graphics setting is too demanding for this device.\n\n"
        "Reset the graphics settings to their defaults? Your old settings file is kept as "
        "settings.txt.bak.\n\n"
        "Lần trước game chưa khởi động xong, thường do thiết lập đồ họa quá nặng so với máy. "
        "Đặt lại về mặc định? File thiết lập cũ được giữ lại là settings.txt.bak.";
    const SDL_MessageBoxData box = {SDL_MESSAGEBOX_WARNING, NULL, "Startup problem", message, 3, buttons, NULL};
    int button = 0;
    if (!SDL_ShowMessageBox(&box, &button)) return -1;
    return button;
}

static void clear_on_exit(void)
{
    disarm(); /* a normal exit; a crash leaves through _exit (debug/crash.c) and a hang or iOS's kill through no exit at all */
}

void Recovery_AtStart(void)
{
    static int started;
    FILE *file;
    if (started) return;
    started = 1;
    if (Paths_User(marker_path, sizeof(marker_path), "startup-check.txt")) {
        marker_path[0] = '\0';
        return;
    }
    file = fopen(marker_path, "r");
    if (file) {
        int choice;
        fclose(file);
        remove(marker_path);
        choice = ask();
        fprintf(stderr, "memories-pc: the last start did not finish; answer %d\n", choice);
        /* Unable to ask: put the graphics back anyway, or the app stays unusable. */
        if (choice < 0) choice = 1;
        if (choice > 0) reset(choice == 2);
    }
    arm();
    Settings_Observe(setting_changed);
    atexit(clear_on_exit);
}

void Recovery_Frame(void)
{
    if (!armed) return;
    if (!first_frame) first_frame = now();
    if (++frames_seen >= STABLE_FRAMES && now() - first_frame >= STABLE_SECONDS) disarm();
}

#else
void Recovery_AtStart(void) {}
void Recovery_Frame(void) {}
#endif
