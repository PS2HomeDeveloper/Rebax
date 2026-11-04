/*
 * code_editor.c - small, responsibility-separated embedded code editor.
 *
 * This library is responsible for the UI for editing C/C++ files only. It does
 * not reimplement the File Manager; it uses it to pick and save files, and
 * uses window to capture input. The theme is dark so the code area is calm and
 * clear inside the Viewport.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "code_editor.h"
#include "window.h"
#include "font.h"
#include "ui_common.h"
#include "ui_theme.h"
#include "file_manager.h"
#include "icon_atlas.h"

#define CODE_EDITOR_CAPACITY 131072
#define CODE_EDITOR_MAX_LINES 4096
#define CODE_EDITOR_LINE_HEIGHT 18
#define CODE_EDITOR_TOOLBAR_HEIGHT 30
#define CODE_EDITOR_TAB_HEIGHT 24
#define CODE_EDITOR_STATUS_HEIGHT 22
#define CODE_EDITOR_GUTTER_WIDTH 48
#define CODE_EDITOR_BUTTON_WIDTH 58
#define CODE_EDITOR_BUTTON_GAP 5
#define CODE_EDITOR_EXT_COUNT 12

static const char *g_code_extensions[CODE_EDITOR_EXT_COUNT] = {
    "c", "h", "cpp", "hpp", "cc", "hh", "cxx", "hxx", "c++", "h++", "ipp", "inl"
};
static char g_text[CODE_EDITOR_CAPACITY];
static size_t g_length = 0;
static size_t g_cursor = 0;
static size_t g_anchor = 0;
static int g_active = 0;
static int g_dirty = 0;
static int g_scroll_line = 0;
static int g_scroll_column = 0;
static int g_preferred_column = 0;
static char g_path[1024];
static char g_title[128] = "untitled.c";
static int g_hover_button = -1;
static int g_has_focus = 0;
static int g_status_ticks = 0;
static char g_status[160] = "Ready";

static window_texture_t *g_button_text[4];
static int g_button_w[4], g_button_h[4];
static window_texture_t *g_tab_text = NULL;
static int g_tab_w = 0, g_tab_h = 0;

static window_texture_t *g_line_text[CODE_EDITOR_MAX_LINES];
static window_texture_t *g_line_number_text[CODE_EDITOR_MAX_LINES];
static int g_line_width[CODE_EDITOR_MAX_LINES];
static int g_line_height[CODE_EDITOR_MAX_LINES];
static char g_cached_line[CODE_EDITOR_MAX_LINES][256];
static char g_cached_number[CODE_EDITOR_MAX_LINES][16];
static int g_cached_line_count = 0;
static int g_cache_dirty = 1;

static void set_status(const char *text) {
    snprintf(g_status, sizeof(g_status), "%s", text ? text : "Ready");
    g_status_ticks = 180;
}

static void free_line_cache(void) {
    for (int i = 0; i < g_cached_line_count; i++) {
        if (g_line_text[i]) window_destroy_texture(g_line_text[i]);
        if (g_line_number_text[i]) window_destroy_texture(g_line_number_text[i]);
        g_line_text[i] = NULL;
        g_line_number_text[i] = NULL;
    }
    g_cached_line_count = 0;
}

static void rebuild_tab_texture(void) {
    if (g_tab_text) window_destroy_texture(g_tab_text);
    char label[144];
    snprintf(label, sizeof(label), "%s%s", g_title, g_dirty ? " *" : "");
    g_tab_text = ui_make_text_texture(label, FONT_WEIGHT_REGULAR, 12, &g_tab_w, &g_tab_h);
}

static void invalidate_view(void) {
    g_cache_dirty = 1;
    rebuild_tab_texture();
}

static int line_count(void) {
    int count = 1;
    for (size_t i = 0; i < g_length; i++) if (g_text[i] == '\n') count++;
    return count;
}

static size_t line_start_for_cursor(void) {
    size_t p = g_cursor;
    while (p > 0 && g_text[p - 1] != '\n') p--;
    return p;
}

static int cursor_line(void) {
    int line = 0;
    for (size_t i = 0; i < g_cursor && i < g_length; i++) if (g_text[i] == '\n') line++;
    return line;
}

static int cursor_column(void) {
    size_t start = line_start_for_cursor();
    return (int)(g_cursor - start);
}

static size_t line_start(int target) {
    int current = 0;
    size_t p = 0;
    while (p < g_length && current < target) {
        if (g_text[p++] == '\n') current++;
    }
    return p;
}

static size_t line_end(size_t start) {
    while (start < g_length && g_text[start] != '\n') start++;
    return start;
}

static void delete_selection(void) {
    if (g_anchor == g_cursor) return;
    size_t a = g_anchor < g_cursor ? g_anchor : g_cursor;
    size_t b = g_anchor < g_cursor ? g_cursor : g_anchor;
    memmove(g_text + a, g_text + b, g_length - b + 1);
    g_length -= b - a;
    g_cursor = g_anchor = a;
    g_dirty = 1;
}

static void insert_bytes(const char *bytes, size_t count) {
    if (!bytes || count == 0) return;
    delete_selection();
    if (g_length + count >= CODE_EDITOR_CAPACITY) {
        set_status("File is too large for the editor");
        return;
    }
    memmove(g_text + g_cursor + count, g_text + g_cursor, g_length - g_cursor + 1);
    memcpy(g_text + g_cursor, bytes, count);
    g_cursor += count;
    g_anchor = g_cursor;
    g_dirty = 1;
}

static void move_vertical(int direction) {
    int current = cursor_line();
    if (g_preferred_column <= 0) g_preferred_column = cursor_column();
    int target = current + direction;
    int total = line_count();
    if (target < 0) target = 0;
    if (target >= total) target = total - 1;
    size_t start = line_start(target);
    size_t end = line_end(start);
    size_t wanted = start + (size_t)g_preferred_column;
    if (wanted > end) wanted = end;
    g_cursor = g_anchor = wanted;
}

static void normalize_view(void) {
    int line = cursor_line();
    int col = cursor_column();
    if (line < g_scroll_line) g_scroll_line = line;
    if (line >= g_scroll_line + 1) {
        /* Actual visible line count is corrected by update/draw bounds. */
    }
    if (col < g_scroll_column) g_scroll_column = col;
    if (g_scroll_line < 0) g_scroll_line = 0;
    if (g_scroll_column < 0) g_scroll_column = 0;
}

static void edit_input(void) {
    const char *typed = window_text_input_this_frame();
    if (typed && typed[0]) {
        /* SDL text input is already UTF-8. Newline is handled separately. */
        insert_bytes(typed, strlen(typed));
    }
    if (window_key_just_pressed_enter()) insert_bytes("\n", 1);
    if (window_key_just_pressed_tab()) insert_bytes("    ", 4);
    if (window_key_just_pressed_backspace()) {
        if (g_anchor != g_cursor) delete_selection();
        else if (g_cursor > 0) { g_cursor--; delete_selection(); }
    }
    if (window_key_just_pressed_delete()) {
        if (g_anchor != g_cursor) delete_selection();
        else if (g_cursor < g_length) { g_anchor = g_cursor + 1; delete_selection(); }
    }
    if (window_key_just_pressed_left()) {
        if (g_cursor > 0) g_cursor--;
        g_anchor = g_cursor;
        g_preferred_column = 0;
    }
    if (window_key_just_pressed_right()) {
        if (g_cursor < g_length) g_cursor++;
        g_anchor = g_cursor;
        g_preferred_column = 0;
    }
    if (window_key_just_pressed_home()) { g_cursor = line_start_for_cursor(); g_anchor = g_cursor; g_preferred_column = 0; }
    if (window_key_just_pressed_end()) { g_cursor = line_end(g_cursor); g_anchor = g_cursor; g_preferred_column = 0; }
    if (window_key_just_pressed_up()) move_vertical(-1);
    if (window_key_just_pressed_down()) move_vertical(1);
    if (window_key_just_pressed_copy()) {
        size_t a = g_anchor < g_cursor ? g_anchor : g_cursor;
        size_t b = g_anchor < g_cursor ? g_cursor : g_anchor;
        if (a != b) { char *clip = malloc(b - a + 1); if (clip) { memcpy(clip, g_text + a, b - a); clip[b-a] = '\0'; window_set_clipboard_text(clip); free(clip); } }
    }
    if (window_key_just_pressed_cut()) {
        size_t a = g_anchor < g_cursor ? g_anchor : g_cursor;
        size_t b = g_anchor < g_cursor ? g_cursor : g_anchor;
        if (a != b) { char *clip = malloc(b - a + 1); if (clip) { memcpy(clip, g_text + a, b - a); clip[b-a] = '\0'; window_set_clipboard_text(clip); free(clip); } delete_selection(); }
    }
    if (window_key_just_pressed_paste()) {
        char *clip = window_get_clipboard_text();
        if (clip) { insert_bytes(clip, strlen(clip)); free(clip); }
    }
    normalize_view();
    if (g_dirty) invalidate_view();
}

static void load_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { set_status("Could not open file"); return; }
    size_t n = fread(g_text, 1, CODE_EDITOR_CAPACITY - 1, f);
    fclose(f);
    g_text[n] = '\0';
    g_length = n;
    g_cursor = g_anchor = 0;
    g_scroll_line = g_scroll_column = 0;
    g_dirty = 0;
    snprintf(g_path, sizeof(g_path), "%s", path);
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    snprintf(g_title, sizeof(g_title), "%s", slash ? slash + 1 : path);
    set_status("File opened");
    invalidate_view();
}

int code_editor_open_path(const char *path) {
    if (path == NULL || path[0] == '\0') return 0;
    if (g_path[0] != '\0' && strcmp(g_path, path) == 0) {
        g_active = 1;
        g_has_focus = 1;
        set_status("File is already open");
        return 1;
    }
    if (g_dirty) {
        set_status("Save the modified script before opening another file");
        return 0;
    }
    load_file(path);
    g_active = 1;
    g_has_focus = 1;
    return g_path[0] != '\0' && strcmp(g_path, path) == 0;
}

static void on_open_file(const char *path, void *unused) {
    (void)unused;
    if (path) code_editor_open_path(path);
    g_has_focus = 1;
}

static void save_to_path(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) { set_status("Could not save file"); return; }
    fwrite(g_text, 1, g_length, f);
    fclose(f);
    snprintf(g_path, sizeof(g_path), "%s", path);
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    snprintf(g_title, sizeof(g_title), "%s", slash ? slash + 1 : path);
    g_dirty = 0;
    set_status("Saved");
    invalidate_view();
}

static void on_save_file(const char *path, void *unused) {
    (void)unused;
    if (path) save_to_path(path);
    g_has_focus = 1;
}

static void open_file_dialog(void) {
    window_stop_text_input();
    file_manager_open(FILE_MANAGER_ROOT_DEVICE, FILE_MANAGER_MODE_PICK_FILE,
                      g_code_extensions, CODE_EDITOR_EXT_COUNT,
                      on_open_file, NULL);
}

static void save_file(void) {
    if (g_path[0]) save_to_path(g_path);
    else file_manager_open_save(FILE_MANAGER_ROOT_DEVICE, g_title, on_save_file, NULL);
}

static void new_file(void) {
    g_text[0] = '\0';
    g_length = 0;
    g_cursor = g_anchor = 0;
    g_scroll_line = g_scroll_column = 0;
    g_path[0] = '\0';
    snprintf(g_title, sizeof(g_title), "untitled.c");
    g_dirty = 0;
    set_status("New C source file");
    invalidate_view();
}

static void color_bytes(unsigned char *rgb, int start, int end,
                        unsigned char r, unsigned char g, unsigned char b) {
    for (int i = start; i < end; i++) {
        rgb[i * 3] = r;
        rgb[i * 3 + 1] = g;
        rgb[i * 3 + 2] = b;
    }
}

static int is_cpp_keyword(const char *text, int start, int end) {
    static const char *keywords[] = {
        "alignas", "auto", "bool", "break", "case", "catch", "char", "class",
        "const", "constexpr", "continue", "default", "delete", "do", "double",
        "else", "enum", "explicit", "extern", "false", "float", "for", "friend",
        "if", "inline", "int", "long", "namespace", "new", "noexcept", "nullptr",
        "operator", "private", "protected", "public", "register", "return", "short",
        "signed", "sizeof", "static", "struct", "switch", "template", "this", "throw",
        "true", "try", "typedef", "typename", "union", "unsigned", "using", "virtual",
        "void", "volatile", "while", "uint8_t", "uint16_t", "uint32_t", "uint64_t",
        "int8_t", "int16_t", "int32_t", "int64_t", "size_t", "ssize_t", "NULL"
    };
    int n = end - start;
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
        if ((int)strlen(keywords[i]) == n && memcmp(text + start, keywords[i], (size_t)n) == 0) return 1;
    return 0;
}

static void colorize_line(const char *text, int length, unsigned char *rgb,
                          int *in_block_comment) {
    color_bytes(rgb, 0, length, 210, 216, 225);
    int first = 0;
    while (first < length && isspace((unsigned char)text[first])) first++;
    if (first < length && text[first] == '#') {
        color_bytes(rgb, first, length, 190, 145, 245);
        return;
    }
    for (int i = 0; i < length;) {
        if (*in_block_comment) {
            int start = i;
            while (i + 1 < length && !(text[i] == '*' && text[i + 1] == '/')) i++;
            if (i + 1 < length) { i += 2; *in_block_comment = 0; }
            else i = length;
            color_bytes(rgb, start, i, 120, 165, 130);
            continue;
        }
        if (i + 1 < length && text[i] == '/' && text[i + 1] == '/') {
            color_bytes(rgb, i, length, 120, 165, 130);
            break;
        }
        if (i + 1 < length && text[i] == '/' && text[i + 1] == '*') {
            int start = i;
            i += 2;
            while (i + 1 < length && !(text[i] == '*' && text[i + 1] == '/')) i++;
            if (i + 1 < length) i += 2;
            else { i = length; *in_block_comment = 1; }
            color_bytes(rgb, start, i, 120, 165, 130);
            continue;
        }
        if (text[i] == '"' || text[i] == '\'') {
            int start = i;
            char quote = text[i++];
            while (i < length) {
                if (text[i] == '\\' && i + 1 < length) { i += 2; continue; }
                if (text[i++] == quote) break;
            }
            color_bytes(rgb, start, i, 224, 171, 116);
            continue;
        }
        if (isalpha((unsigned char)text[i]) || text[i] == '_') {
            int start = i++;
            while (i < length && (isalnum((unsigned char)text[i]) || text[i] == '_')) i++;
            int next = i;
            while (next < length && isspace((unsigned char)text[next])) next++;
            if (is_cpp_keyword(text, start, i)) color_bytes(rgb, start, i, 195, 155, 250);
            else if (next < length && text[next] == '(') color_bytes(rgb, start, i, 120, 190, 240);
            else if (isupper((unsigned char)text[start])) color_bytes(rgb, start, i, 105, 195, 190);
            continue;
        }
        if (isdigit((unsigned char)text[i])) {
            int start = i++;
            while (i < length && (isalnum((unsigned char)text[i]) || text[i] == '.' || text[i] == '_')) i++;
            color_bytes(rgb, start, i, 145, 190, 245);
            continue;
        }
        i++;
    }
}

static void rebuild_line_cache(void) {
    if (!g_cache_dirty) return;
    free_line_cache();
    size_t p = 0;
    int line = 0;
    int in_block_comment = 0;
    while (line < CODE_EDITOR_MAX_LINES && p <= g_length) {
        size_t end = line_end(p);
        size_t n = end - p;
        if (n >= sizeof(g_cached_line[line])) n = sizeof(g_cached_line[line]) - 1;
        memcpy(g_cached_line[line], g_text + p, n);
        g_cached_line[line][n] = '\0';
        snprintf(g_cached_number[line], sizeof(g_cached_number[line]), "%4d", line + 1);
        unsigned char rgb[sizeof(g_cached_line[line]) * 3];
        colorize_line(g_cached_line[line], (int)n, rgb, &in_block_comment);
        font_text_image_t image = font_render_text_colored(g_cached_line[line], FONT_WEIGHT_REGULAR, 13, rgb);
        if (image.pixels != NULL) {
            g_line_text[line] = window_create_texture(image.pixels, image.width, image.height);
            g_line_width[line] = image.width;
            g_line_height[line] = image.height;
            font_free_text_image(&image);
        }
        int nw = 0, nh = 0;
        g_line_number_text[line] = ui_make_text_texture(g_cached_number[line], FONT_WEIGHT_REGULAR, 11, &nw, &nh);
        line++;
        if (end >= g_length) break;
        p = end + 1;
    }
    g_cached_line_count = line;
    g_cache_dirty = 0;
}

static int button_hit(int index, int x, int y, int w) {
    (void)w;
    int bx = x + 8 + index * (CODE_EDITOR_BUTTON_WIDTH + CODE_EDITOR_BUTTON_GAP);
    return window_mouse_x() >= bx && window_mouse_x() < bx + CODE_EDITOR_BUTTON_WIDTH &&
           window_mouse_y() >= y && window_mouse_y() < y + CODE_EDITOR_TOOLBAR_HEIGHT;
}

void code_editor_set_active(int active) {
    g_active = active;
    if (active && g_tab_text == NULL) rebuild_tab_texture();
    if (!active) { g_has_focus = 0; window_stop_text_input(); }
}

void code_editor_update(int x, int y, int w, int h) {
    if (!g_active) return;
    int mx = window_mouse_x(), my = window_mouse_y();
    g_hover_button = -1;
    for (int i = 0; i < 4; i++) if (button_hit(i, x, y, w)) g_hover_button = i;
    if (window_mouse_left_just_pressed()) {
        if (g_hover_button == 0) new_file();
        else if (g_hover_button == 1) open_file_dialog();
        else if (g_hover_button == 2) save_file();
        else if (g_hover_button == 3) file_manager_open_save(FILE_MANAGER_ROOT_DEVICE, g_title, on_save_file, NULL);
        int editor_top = y + CODE_EDITOR_TOOLBAR_HEIGHT + CODE_EDITOR_TAB_HEIGHT;
        if (mx >= x && mx < x + w && my >= editor_top && my < y + h - CODE_EDITOR_STATUS_HEIGHT) {
            g_has_focus = 1;
            int visible = (h - CODE_EDITOR_TOOLBAR_HEIGHT - CODE_EDITOR_TAB_HEIGHT - CODE_EDITOR_STATUS_HEIGHT) / CODE_EDITOR_LINE_HEIGHT;
            int clicked_line = g_scroll_line + (my - editor_top) / CODE_EDITOR_LINE_HEIGHT;
            if (clicked_line < line_count()) {
                size_t start = line_start(clicked_line);
                size_t end = line_end(start);
                int col = g_scroll_column + (mx - x - CODE_EDITOR_GUTTER_WIDTH - 8) / 8;
                if (col < 0) col = 0;
                size_t pos = start + (size_t)col;
                if (pos > end) pos = end;
                g_cursor = g_anchor = pos;
                (void)visible;
            }
        }
    }
    int editor_top = y + CODE_EDITOR_TOOLBAR_HEIGHT + CODE_EDITOR_TAB_HEIGHT;
    int editor_bottom = y + h - CODE_EDITOR_STATUS_HEIGHT;
    if (mx >= x && mx < x + w && my >= editor_top && my < editor_bottom) {
        int wheel = window_mouse_wheel_delta();
        if (wheel) g_scroll_line -= wheel;
        int max_line = line_count() - 1;
        int visible = (editor_bottom - editor_top) / CODE_EDITOR_LINE_HEIGHT;
        if (g_scroll_line < 0) g_scroll_line = 0;
        if (g_scroll_line > max_line - visible + 1) g_scroll_line = max_line - visible + 1;
        if (g_scroll_line < 0) g_scroll_line = 0;
    }
    if (g_has_focus) {
        window_start_text_input();
        edit_input();
        if (window_key_just_pressed_save()) save_file();
    } else window_stop_text_input();
    if (g_status_ticks > 0) g_status_ticks--;
}

void code_editor_draw(int x, int y, int w, int h) {
    if (!g_active) return;
    rebuild_line_cache();
    ui_color_t panel = {0x19, 0x1D, 0x24, 0xFF};
    ui_color_t toolbar = {0x24, 0x2B, 0x35, 0xFF};
    ui_color_t gutter = {0x15, 0x18, 0x1E, 0xFF};
    ui_color_t code_bg = {0x1C, 0x21, 0x2A, 0xFF};
    ui_color_t active_line = {0x25, 0x2E, 0x3A, 0xFF};
    window_fill_rect(x, y, w, h, panel.r, panel.g, panel.b);
    window_fill_rect(x, y, w, CODE_EDITOR_TOOLBAR_HEIGHT, toolbar.r, toolbar.g, toolbar.b);
    const char *buttons[4] = { "New", "Open", "Save", "Save As" };
    for (int i = 0; i < 4; i++) {
        int bx = x + 8 + i * (CODE_EDITOR_BUTTON_WIDTH + CODE_EDITOR_BUTTON_GAP);
        ui_color_t c = (i == g_hover_button) ? UI_COLOR_BUTTON_BLUE : UI_COLOR_GRAY_MUTED;
        window_fill_rect(bx, y + 4, CODE_EDITOR_BUTTON_WIDTH, CODE_EDITOR_TOOLBAR_HEIGHT - 8, c.r, c.g, c.b);
        if (!g_button_text[i]) g_button_text[i] = ui_make_text_texture(buttons[i], FONT_WEIGHT_REGULAR, 11, &g_button_w[i], &g_button_h[i]);
        if (g_button_text[i]) window_draw_texture(g_button_text[i], bx + (CODE_EDITOR_BUTTON_WIDTH - g_button_w[i]) / 2,
                                                  y + (CODE_EDITOR_TOOLBAR_HEIGHT - g_button_h[i]) / 2, g_button_w[i], g_button_h[i]);
    }
    int tab_y = y + CODE_EDITOR_TOOLBAR_HEIGHT;
    window_fill_rect(x, tab_y, w, CODE_EDITOR_TAB_HEIGHT, gutter.r, gutter.g, gutter.b);
    window_fill_rect(x + 8, tab_y + 2, 190, CODE_EDITOR_TAB_HEIGHT - 4, code_bg.r, code_bg.g, code_bg.b);
    if (g_tab_text) window_draw_texture(g_tab_text, x + 18, tab_y + (CODE_EDITOR_TAB_HEIGHT - g_tab_h) / 2, g_tab_w, g_tab_h);
    int top = tab_y + CODE_EDITOR_TAB_HEIGHT;
    int bottom = y + h - CODE_EDITOR_STATUS_HEIGHT;
    window_fill_rect(x, top, CODE_EDITOR_GUTTER_WIDTH, bottom - top, gutter.r, gutter.g, gutter.b);
    window_fill_rect(x + CODE_EDITOR_GUTTER_WIDTH, top, w - CODE_EDITOR_GUTTER_WIDTH, bottom - top, code_bg.r, code_bg.g, code_bg.b);
    int visible = (bottom - top) / CODE_EDITOR_LINE_HEIGHT;
    int current = cursor_line();
    for (int row = 0; row < visible; row++) {
        int line = g_scroll_line + row;
        if (line >= g_cached_line_count) break;
        int ly = top + row * CODE_EDITOR_LINE_HEIGHT;
        if (line == current) window_fill_rect(x + CODE_EDITOR_GUTTER_WIDTH, ly, w - CODE_EDITOR_GUTTER_WIDTH, CODE_EDITOR_LINE_HEIGHT,
                                                active_line.r, active_line.g, active_line.b);
        if (g_line_number_text[line]) window_draw_texture_tinted(g_line_number_text[line], x + 6, ly + 2, 30, 14, 120, 135, 150);
        if (g_line_text[line]) window_draw_texture(g_line_text[line], x + CODE_EDITOR_GUTTER_WIDTH + 8 - g_scroll_column * 8, ly + 1,
                                                    g_line_width[line], 16);
    }
    int cx = x + CODE_EDITOR_GUTTER_WIDTH + 8 + (cursor_column() - g_scroll_column) * 8;
    int cy = top + (current - g_scroll_line) * CODE_EDITOR_LINE_HEIGHT;
    if (g_has_focus && cy >= top && cy < bottom) window_fill_rect(cx, cy + 2, 2, 14, 90, 210, 240);
    window_fill_rect(x, bottom, w, CODE_EDITOR_STATUS_HEIGHT, toolbar.r, toolbar.g, toolbar.b);
    char status[256];
    snprintf(status, sizeof(status), "%s  |  Ln %d, Col %d  |  %s", g_status, current + 1, cursor_column() + 1,
             g_dirty ? "Modified" : "Saved");
    int sw = 0, sh = 0;
    window_texture_t *st = ui_make_text_texture(status, FONT_WEIGHT_REGULAR, 11, &sw, &sh);
    if (st) { window_draw_texture(st, x + 8, bottom + (CODE_EDITOR_STATUS_HEIGHT - sh) / 2, sw, sh); window_destroy_texture(st); }
}

void code_editor_shutdown(void) {
    free_line_cache();
    for (int i = 0; i < 4; i++) if (g_button_text[i]) { window_destroy_texture(g_button_text[i]); g_button_text[i] = NULL; }
    if (g_tab_text) { window_destroy_texture(g_tab_text); g_tab_text = NULL; }
    g_active = 0;
    window_stop_text_input();
}
