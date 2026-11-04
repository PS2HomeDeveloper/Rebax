/*
 * ============================================================
 * loading_screen.h
 * ============================================================
 * General waiting screen (simple rotating dots, no resources - math only via cos/sin).
 * Called from anywhere that needs a visual transition during a background operation
 * (create project, import, open project, restart engine after settings changes, etc.).
 *
 * The current look is temporary (rotating blue dots) - will be replaced later with an
 * animated engine logo once the engine visual identity is designed.
 * ============================================================
 */

#ifndef LOADING_SCREEN_H
#define LOADING_SCREEN_H

/* Shows the screen and resets the internal animation counter */
void loading_screen_show(void);

/* Same screen, but without auto-hide after a fixed time - remains visible until the caller
 * calls loading_screen_hide. Used while waiting for a real variable-duration operation
 * (e.g., Rebax first-run setup) instead of a fixed fake delay */
void loading_screen_show_indefinite(void);

/* Hides the screen */
void loading_screen_hide(void);

/* 1 if the screen is currently visible, 0 otherwise */
int loading_screen_is_visible(void);

/* Returns 1 only once, exactly in the frame the screen auto-hid (after the current temporary
 * duration) - used by main.c to know exactly when to switch to the next screen without
 * repeating the transition each frame */
int loading_screen_just_finished(void);

/* Called once per frame (even if hidden) - updates the internal animation counter */
void loading_screen_update(void);

/* Draws the entire screen (background + rotating indicator in the center) - draws nothing if currently hidden */
void loading_screen_draw(int window_w, int window_h);

#endif /* LOADING_SCREEN_H */
