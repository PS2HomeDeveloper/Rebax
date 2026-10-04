/*
 * ============================================================
 * loading_screen.c
 * ============================================================
 * Idea: 8 dots evenly distributed around the circumference of a circle (angle for each point
 * computed via cos/sin - standard circle math). Every certain number of frames, the
 * "active" dot moves to the next to give a sense of rotation - all this without any images
 * or resources, just small rectangles.
 * ============================================================
 */

#include <math.h>

#include "loading_screen.h"
#include "window.h"
#include "ui_theme.h"

#define DOT_COUNT        8
#define DOT_SIZE         6   /* Normal dot size in pixels */
#define DOT_SIZE_ACTIVE  12  /* Active dot size (larger = indicates motion) */
#define ORBIT_RADIUS     34  /* Radius of the circle on which the dots are distributed */
#define FRAMES_PER_STEP  6   /* How many frames per step the active dot moves */
#define AUTO_HIDE_FRAMES 90  /* Duration the screen shows before auto-hiding (~1.5 seconds) -
                               * temporary until tied to a real operation's completion later */

static int g_visible = 0;
static int g_frame_counter = 0;
static int g_just_finished = 0;
static int g_auto_hide = 1; /* 0 while waiting for a real operation (e.g., Rebax first-run setup) - main.c controls hiding in that case */

void loading_screen_show(void) {
    g_visible = 1;
    g_frame_counter = 0;
    g_just_finished = 0;
    g_auto_hide = 1;
}

/* Same as loading_screen_show but without auto-hide after a fixed time -
 * remains visible until the caller calls loading_screen_hide (used by
 * main.c while waiting for rebax_paths_setup_* - a real variable-duration
 * wait, not a fixed fake delay) */
void loading_screen_show_indefinite(void) {
    g_visible = 1;
    g_frame_counter = 0;
    g_just_finished = 0;
    g_auto_hide = 0;
}

void loading_screen_hide(void) {
    g_visible = 0;
}

int loading_screen_is_visible(void) {
    return g_visible;
}

/* One-shot flag - returns 1 only in the frame the screen auto-hid, then returns 0 thereafter
 * even if polled. main.c reads this to know exactly when to switch to the next screen
 * without repeating the transition */
int loading_screen_just_finished(void) {
    if (g_just_finished) {
        g_just_finished = 0;
        return 1;
    }
    return 0;
}

void loading_screen_update(void) {
    if (!g_visible) {
        return;
    }
    g_frame_counter++;
    if (g_auto_hide && g_frame_counter >= AUTO_HIDE_FRAMES) {
        g_visible = 0;
        g_just_finished = 1;
    }
}

void loading_screen_draw(int window_w, int window_h) {
    if (!g_visible) {
        return;
    }

    /* The screen manages its own background - full screen takeover while waiting */
    window_clear(UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);

    int cx = window_w / 2;
    int cy = window_h / 2;
    int active = (g_frame_counter / FRAMES_PER_STEP) % DOT_COUNT;

    for (int i = 0; i < DOT_COUNT; i++) {
        double angle = (2.0 * 3.14159265358979 * i) / DOT_COUNT;
        int dx = cx + (int)(cos(angle) * ORBIT_RADIUS);
        int dy = cy + (int)(sin(angle) * ORBIT_RADIUS);

        int is_active = (i == active);
        int size = is_active ? DOT_SIZE_ACTIVE : DOT_SIZE;

        window_fill_rect(dx - size / 2, dy - size / 2, size, size,
                          UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    }
}
