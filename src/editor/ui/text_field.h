/*
 * ============================================================
 * text_field.h
 * ============================================================
 * General-purpose text field (project name, path, search, etc.) - supports
 * keyboard typing, copy/paste/cut, cursor navigation (left/right/home/end),
 * and automatic horizontal scrolling when text exceeds the field width
 * (instead of overflowing).
 *
 * Used by any UI library that needs text input (project_dialog currently,
 * search bar in the future) without repeating the same logic everywhere.
 * ============================================================
 */

#ifndef TEXT_FIELD_H
#define TEXT_FIELD_H

#define TEXT_FIELD_MAX_LEN 256

typedef struct {
    char text[TEXT_FIELD_MAX_LEN];
    int  cursor_pos;      /* Cursor position within the text (in characters) */
    int  scroll_offset;   /* Number of text pixels hidden from the left (for scrolling) */
    int  focused;         /* 1 if this field is currently receiving input */

    /* ------------------------------------------------------------
     * Internal cache (not read or modified outside text_field.c) -
     * prevents re-rendering the background and text from scratch every frame,
     * and draws them only when something actually changes (new text or field size)
     * ------------------------------------------------------------ */
    void *_cache_bg_tex;
    void *_cache_text_tex;
    char  _cache_text[TEXT_FIELD_MAX_LEN];
    int   _cache_w;
    int   _cache_h;
    int   _cache_text_w;   /* Dimensions of the last actually rendered text - avoid remeasuring every frame */
    int   _cache_text_h;
    int   _cache_valid;
} text_field_t;

/* Initializes an empty field */
void text_field_init(text_field_t *field);

/* Frees any cached memory (background/text) created for this field -
 * Called once when the window containing it is closed */
void text_field_free(text_field_t *field);

/* Called every frame (whether the field is active or not) - checks if the user
 * clicked inside the field bounds (rect) to activate it, and if active reads any
 * typing/navigation/copy-paste input from window.c and updates its internal state.
 * field_width is used to compute correct horizontal scrolling */
void text_field_update(text_field_t *field, int x, int y, int w, int h,
                        int field_width);

/* Draws the field background (gray color, simple rounded corners using the same
 * base as the button image but in a different color) + the text inside it with
 * computed scrolling, using a specified font size and family. Non-const on purpose -
 * updates internal cache memory when necessary */
void text_field_draw(text_field_t *field, int x, int y, int w, int h,
                      int font_pixel_size);

#endif /* TEXT_FIELD_H */
