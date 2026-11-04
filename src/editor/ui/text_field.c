/*
 * ============================================================
 * text_field.c
 * ============================================================
 */

#include <stdlib.h>
#include <string.h>

#include "text_field.h"
#include "window.h"
#include "shape_provider.h"
#include "font.h"
#include "ui_theme.h" /* For UI_COLOR_GRAY_MUTED */

/* Corner radius for the text field - intentionally larger than the button's
 * (a text field is wide and flat, so a larger radius gives a clearer curve) */
#define TEXT_FIELD_CORNER_RADIUS 8

void text_field_init(text_field_t *field) {
    field->text[0] = '\0';
    field->cursor_pos = 0;
    field->scroll_offset = 0;
    field->focused = 0;

    field->_cache_bg_tex = NULL;
    field->_cache_text_tex = NULL;
    field->_cache_text[0] = '\0';
    field->_cache_w = 0;
    field->_cache_h = 0;
    field->_cache_text_w = 0;
    field->_cache_text_h = 0;
    field->_cache_valid = 0;
}

void text_field_free(text_field_t *field) {
    if (field->_cache_bg_tex != NULL) {
        window_destroy_texture((window_texture_t *)field->_cache_bg_tex);
        field->_cache_bg_tex = NULL;
    }
    if (field->_cache_text_tex != NULL) {
        window_destroy_texture((window_texture_t *)field->_cache_text_tex);
        field->_cache_text_tex = NULL;
    }
    field->_cache_valid = 0;
}

/* Computes the pixel width of text from the start up to a given character
 * position - used to calculate cursor position and required scrolling */
static int measure_text_width_upto(const char *text, int upto_chars, int pixel_size) {
    if (upto_chars <= 0) {
        return 0;
    }
    char buffer[TEXT_FIELD_MAX_LEN];
    int n = upto_chars;
    if (n >= TEXT_FIELD_MAX_LEN) n = TEXT_FIELD_MAX_LEN - 1;
    memcpy(buffer, text, (size_t)n);
    buffer[n] = '\0';

    font_text_image_t img = font_render_text(buffer, FONT_WEIGHT_REGULAR, pixel_size);
    int w = img.width;
    font_free_text_image(&img);
    return w;
}

void text_field_update(text_field_t *field, int x, int y, int w, int h,
                        int field_width) {
    /* Toggle the field's active state based on mouse click position - we only
     * change the focused flag here, without directly calling text input
     * activation/deactivation (window_start/stop_text_input) - the final
     * decision is made centrally by the caller (project_dialog.c) after
     * all fields update their state, to avoid conflicting start/stop calls
     * in the same frame when focus moves between fields. */
    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x();
        int my = window_mouse_y();
        int inside = (mx >= x && mx < x + w && my >= y && my < y + h);
        if (inside) {
            field->focused = 1;
        } else if (field->focused) {
            field->focused = 0;
        }
    }

    if (!field->focused) {
        return;
    }

    int len = (int)strlen(field->text);

    /* Insert new characters */
    const char *typed = window_text_input_this_frame();
    if (typed[0] != '\0') {
        int typed_len = (int)strlen(typed);
        if (len + typed_len < TEXT_FIELD_MAX_LEN - 1) {
            /* Insert typed text at the current cursor position */
            memmove(field->text + field->cursor_pos + typed_len,
                    field->text + field->cursor_pos,
                    (size_t)(len - field->cursor_pos + 1));
            memcpy(field->text + field->cursor_pos, typed, (size_t)typed_len);
            field->cursor_pos += typed_len;
            len += typed_len;
        }
    }

    /* Delete character before the cursor */
    if (window_key_just_pressed_backspace() && field->cursor_pos > 0) {
        memmove(field->text + field->cursor_pos - 1,
                field->text + field->cursor_pos,
                (size_t)(len - field->cursor_pos + 1));
        field->cursor_pos--;
        len--;
    }

    /* Delete character after the cursor */
    if (window_key_just_pressed_delete() && field->cursor_pos < len) {
        memmove(field->text + field->cursor_pos,
                field->text + field->cursor_pos + 1,
                (size_t)(len - field->cursor_pos));
        len--;
    }

    /* Cursor navigation */
    if (window_key_just_pressed_left() && field->cursor_pos > 0) {
        field->cursor_pos--;
    }
    if (window_key_just_pressed_right() && field->cursor_pos < len) {
        field->cursor_pos++;
    }
    if (window_key_just_pressed_home()) {
        field->cursor_pos = 0;
    }
    if (window_key_just_pressed_end()) {
        field->cursor_pos = len;
    }

    /* Copy/Cut/Paste - operate on the whole text (no partial mouse selection yet) */
    if (window_key_just_pressed_copy() || window_key_just_pressed_cut()) {
        window_set_clipboard_text(field->text);
        if (window_key_just_pressed_cut()) {
            field->text[0] = '\0';
            field->cursor_pos = 0;
            len = 0;
        }
    }
    if (window_key_just_pressed_paste()) {
        char *clip = window_get_clipboard_text();
        if (clip != NULL) {
            int clip_len = (int)strlen(clip);
            if (clip_len < TEXT_FIELD_MAX_LEN - 1) {
                strcpy(field->text, clip);
                field->cursor_pos = clip_len;
                len = clip_len;
            }
            free(clip);
        }
    }

    /* Automatic horizontal scrolling: ensure the cursor position is always
     * visible within the field bounds */
    int padding = 8;
    int cursor_px = measure_text_width_upto(field->text, field->cursor_pos, 14);
    int visible_w = field_width - padding * 2;

    if (cursor_px - field->scroll_offset > visible_w) {
        field->scroll_offset = cursor_px - visible_w;
    }
    if (cursor_px - field->scroll_offset < 0) {
        field->scroll_offset = cursor_px;
    }
    if (field->scroll_offset < 0) {
        field->scroll_offset = 0;
    }
}

void text_field_draw(text_field_t *field, int x, int y, int w, int h,
                      int font_pixel_size) {
    /* Background: re-rendered only when size changes (or first time) - the
     * field color and corner radius are constant, no need to recompute every frame */
    if (!field->_cache_valid || field->_cache_w != w || field->_cache_h != h) {
        if (field->_cache_bg_tex != NULL) {
            window_destroy_texture((window_texture_t *)field->_cache_bg_tex);
            field->_cache_bg_tex = NULL;
        }
        shape_image_t bg = shape_provider_render_rect(w, h, TEXT_FIELD_CORNER_RADIUS,
            UI_COLOR_GRAY_MUTED.r, UI_COLOR_GRAY_MUTED.g, UI_COLOR_GRAY_MUTED.b);
        if (bg.pixels != NULL) {
            field->_cache_bg_tex = window_create_texture(bg.pixels, bg.width, bg.height);
            shape_provider_free_image(&bg);
        }
        field->_cache_w = w;
        field->_cache_h = h;
        field->_cache_valid = 1;
    }
    if (field->_cache_bg_tex != NULL) {
        window_draw_texture((window_texture_t *)field->_cache_bg_tex, x, y, w, h);
    }

    /* Text: re-rendered only if its content actually changed since the last draw -
     * this is the most performance-critical part, since it changes on every keypress, not every frame */
    if (strcmp(field->text, field->_cache_text) != 0) {
        if (field->_cache_text_tex != NULL) {
            window_destroy_texture((window_texture_t *)field->_cache_text_tex);
            field->_cache_text_tex = NULL;
        }
        if (field->text[0] != '\0') {
            font_text_image_t txt = font_render_text(field->text, FONT_WEIGHT_REGULAR, font_pixel_size);
            if (txt.pixels != NULL) {
                field->_cache_text_tex = window_create_texture(txt.pixels, txt.width, txt.height);
                field->_cache_text_w = txt.width;
                field->_cache_text_h = txt.height;
                font_free_text_image(&txt);
            }
        } else {
            field->_cache_text_w = 0;
            field->_cache_text_h = 0;
        }
        strncpy(field->_cache_text, field->text, TEXT_FIELD_MAX_LEN - 1);
        field->_cache_text[TEXT_FIELD_MAX_LEN - 1] = '\0';
    }

    /* Text - clipped at the field bounds so overflow doesn't show outside.
     * Use dimensions stored from the last actual generation - no re-measure
     * or extra drawing here */
    if (field->_cache_text_tex != NULL) {
        int padding = 8;
        window_set_clip_rect(x, y, w, h);
        window_draw_texture((window_texture_t *)field->_cache_text_tex,
                             x + padding - field->scroll_offset,
                             y + (h - field->_cache_text_h) / 2,
                             field->_cache_text_w, field->_cache_text_h);
        window_clear_clip_rect();
    }
}
