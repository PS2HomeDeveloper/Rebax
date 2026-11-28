/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
 * |  _ <| |___| |_) / ___ \  /  \ 
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "core/i18n.h"
#include "editor/panels/code_editor.h"
#include "platform/common/window.h"
#include "editor/ui/font.h"
#include "editor/ui/base.h"
#include "editor/ui/theme.h"
#include "editor/panels/file_manager.h"
#include "editor/ui/icon_atlas.h"

#define CODE_EDITOR_CAPACITY 131072
#define CODE_EDITOR_MAX_LINES 4096
#define CODE_EDITOR_LINE_HEIGHT 22
#define CODE_EDITOR_TOOLBAR_HEIGHT 34
#define CODE_EDITOR_TAB_HEIGHT 28
#define CODE_EDITOR_STATUS_HEIGHT 26
#define CODE_EDITOR_GUTTER_WIDTH 48
#define CODE_EDITOR_BUTTON_WIDTH 58
#define CODE_EDITOR_BUTTON_GAP 5
#define CODE_EDITOR_EXT_COUNT 12
#define CODE_EDITOR_BUTTONS 6
#define CODE_EDITOR_UNDO_MAX 100
#define CODE_EDITOR_LONG_PRESS_MS 450

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
static int g_scroll_x = 0;
static int g_follow_cursor = 0;
static int g_view_w = 400, g_view_h = 300;
static int g_press_active = 0, g_press_x = 0, g_press_y = 0, g_dragged = 0, g_drag_accum = 0;
#define CODE_FONT 13
static int g_preferred_column = 0;
static char g_path[1024];
static char g_title[128] = "untitled.c";
static int g_hover_button = -1;
static int g_has_focus = 0;
static int g_status_ticks = 0;
static char g_status[160] = "Ready";
static char g_completion[8][64];
static int g_completion_count = 0;
static char g_completion_prefix[64];
static int g_matching_position = -1;
static int g_max_line_w = 0;
static int g_popup_x, g_popup_y, g_popup_w, g_popup_h;
static size_t g_comp_cursor = (size_t)-1, g_comp_length = (size_t)-1;
static unsigned g_press_tick = 0;
static int g_long_press = 0;

typedef struct { char *text; size_t length, cursor; } undo_state_t;
static undo_state_t g_undo[CODE_EDITOR_UNDO_MAX], g_redo[CODE_EDITOR_UNDO_MAX];
static int g_undo_count = 0, g_redo_count = 0;
static int g_last_kind = 0;
static unsigned g_last_edit_tick = 0;

static window_texture_t *g_button_text[CODE_EDITOR_BUTTONS];
static int g_button_w[CODE_EDITOR_BUTTONS], g_button_h[CODE_EDITOR_BUTTONS];
static window_texture_t *g_tab_text = NULL;
static int g_tab_w = 0, g_tab_h = 0;

static window_texture_t *g_line_text[CODE_EDITOR_MAX_LINES];
static window_texture_t *g_line_number_text[CODE_EDITOR_MAX_LINES];
static int g_line_width[CODE_EDITOR_MAX_LINES];
static int g_line_height[CODE_EDITOR_MAX_LINES];
static int g_line_number_w[CODE_EDITOR_MAX_LINES], g_line_number_h[CODE_EDITOR_MAX_LINES];
static char g_cached_line[CODE_EDITOR_MAX_LINES][256];
static char g_cached_number[CODE_EDITOR_MAX_LINES][16];
static int g_cached_line_count = 0;
static int g_cache_dirty = 1;

static void set_status(const char *text) {
    snprintf(g_status, sizeof(g_status), "%s", tr(text ? text : "Ready"));
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

static int snapshot_state(undo_state_t *state) {
    state->text = (char *)malloc(g_length + 1);
    if (!state->text) return 0;
    memcpy(state->text, g_text, g_length + 1);
    state->length = g_length;
    state->cursor = g_cursor;
    return 1;
}

static void free_states(undo_state_t *list, int *count) {
    for (int i = 0; i < *count; i++) { free(list[i].text); list[i].text = NULL; }
    *count = 0;
}

static void push_state(undo_state_t *list, int *count) {
    undo_state_t state;
    if (!snapshot_state(&state)) return;
    if (*count == CODE_EDITOR_UNDO_MAX) {
        free(list[0].text);
        memmove(list, list + 1, sizeof(list[0]) * (CODE_EDITOR_UNDO_MAX - 1));
        (*count)--;
    }
    list[(*count)++] = state;
}

static void begin_edit(int kind) {
    unsigned now = window_ticks();
    if (kind == 4 || kind != g_last_kind || now - g_last_edit_tick > 800) {
        push_state(g_undo, &g_undo_count);
        free_states(g_redo, &g_redo_count);
    }
    g_last_kind = kind;
    g_last_edit_tick = now;
}

static void restore_state(const undo_state_t *state) {
    memcpy(g_text, state->text, state->length + 1);
    g_length = state->length;
    g_cursor = g_anchor = state->cursor <= g_length ? state->cursor : g_length;
    g_dirty = 1;
    g_last_kind = 0;
    g_follow_cursor = 1;
    invalidate_view();
}

static void do_undo(void) {
    if (g_undo_count == 0) return;
    push_state(g_redo, &g_redo_count);
    undo_state_t state = g_undo[--g_undo_count];
    restore_state(&state);
    free(state.text);
}

static void do_redo(void) {
    if (g_redo_count == 0) return;
    push_state(g_undo, &g_undo_count);
    undo_state_t state = g_redo[--g_redo_count];
    restore_state(&state);
    free(state.text);
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
    g_length += count;
    g_cursor += count;
    g_anchor = g_cursor;
    g_dirty = 1;
}

static int is_word_char(char c) { return isalnum((unsigned char)c) || c == '_'; }

static void add_candidate(const char *word) {
    if (g_completion_count >= 8) return;
    size_t prefix = strlen(g_completion_prefix);
    if (strlen(word) <= prefix || strlen(word) >= sizeof(g_completion[0])) return;
    if (strncmp(word, g_completion_prefix, prefix) != 0) return;
    for (int i = 0; i < g_completion_count; i++) if (strcmp(g_completion[i], word) == 0) return;
    snprintf(g_completion[g_completion_count++], sizeof(g_completion[0]), "%s", word);
}

static void update_completion(void) {
    static const char *const words[] = {
        "rebax_log", "rebax_delta_time", "rebax_key_down", "rebax_key_pressed", "rebax_find_node",
        "rebax_node_get_transform2d", "rebax_node_set_transform2d", "rebax_node_set_visible",
        "rebax_node_is_visible", "rebax_node_get_float", "rebax_node_set_float", "rebax_node_get_int", "rebax_node_set_int", "rebax_image_load", "rebax_image_free", "rebax_image_draw",
        "rebax_quit", "rebax_game_start", "rebax_game_update", "rebax_game_stop",
        "rebax_vec2_t", "rebax_transform2d_t", "rebax_image_t",
        "auto", "bool", "break", "case", "char", "class", "const", "continue", "double", "else",
        "enum", "float", "for", "if", "int", "namespace", "return", "sizeof", "static", "struct",
        "switch", "typedef", "unsigned", "void", "while"
    };
    if (g_cursor == g_comp_cursor && g_length == g_comp_length) return;
    g_comp_cursor = g_cursor;
    g_comp_length = g_length;
    size_t start = g_cursor;
    while (start > 0 && is_word_char(g_text[start - 1])) start--;
    g_completion_count = 0;
    if (g_cursor - start == 0 || g_cursor - start >= sizeof(g_completion_prefix)) { g_completion_prefix[0] = '\0'; return; }
    memcpy(g_completion_prefix, g_text + start, g_cursor - start);
    g_completion_prefix[g_cursor - start] = '\0';
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) add_candidate(words[i]);
    for (size_t i = 0; i < g_length && g_completion_count < 8;) {
        if (!is_word_char(g_text[i]) || isdigit((unsigned char)g_text[i])) { i++; continue; }
        size_t j = i;
        while (j < g_length && is_word_char(g_text[j])) j++;
        if (i != start && j - i < sizeof(g_completion[0]) && j - i >= 3) {
            char word[64];
            memcpy(word, g_text + i, j - i);
            word[j - i] = '\0';
            add_candidate(word);
        }
        i = j;
    }
}
static void accept_completion(int index) {
    size_t prefix = strlen(g_completion_prefix);
    if (index < 0 || index >= g_completion_count || g_cursor < prefix) return;
    char word[64];
    snprintf(word, sizeof(word), "%s", g_completion[index]);
    begin_edit(4);
    g_cursor -= prefix;
    g_anchor = g_cursor;
    {
        size_t end = g_cursor + prefix;
        memmove(g_text + g_cursor, g_text + end, g_length - end + 1);
        g_length -= prefix;
    }
    insert_bytes(word, strlen(word));
    g_completion_count = 0;
    g_comp_cursor = (size_t)-1;
    g_follow_cursor = 1;
    invalidate_view();
}
static void update_matching_bracket(void) {
    size_t at = g_cursor > 0 ? g_cursor - 1 : g_cursor, p;
    char open, close;
    int direction, depth = 0;
    g_matching_position = -1;
    if (at >= g_length) return;
    open = g_text[at];
    if (open == '(') { close = ')'; direction = 1; }
    else if (open == '[') { close = ']'; direction = 1; }
    else if (open == '{') { close = '}'; direction = 1; }
    else if (open == ')') { close = '('; direction = -1; }
    else if (open == ']') { close = '['; direction = -1; }
    else if (open == '}') { close = '{'; direction = -1; }
    else return;
    p = at;
    while (direction > 0 ? p + 1 < g_length : p > 0) {
        p = direction > 0 ? p + 1 : p - 1;
        if (g_text[p] == open) depth++;
        else if (g_text[p] == close) { if (depth == 0) { g_matching_position = (int)p; return; } depth--; }
    }
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

static int prefix_width(size_t start, size_t count) {
    char buffer[256];
    if (count >= sizeof(buffer)) count = sizeof(buffer) - 1;
    memcpy(buffer, g_text + start, count);
    buffer[count] = '\0';
    return font_measure_text(buffer, FONT_WEIGHT_MONO, CODE_FONT);
}

static void normalize_view(void) {
    if (!g_follow_cursor) return;
    g_follow_cursor = 0;
    int visible_lines = (g_view_h - CODE_EDITOR_TOOLBAR_HEIGHT - CODE_EDITOR_TAB_HEIGHT - CODE_EDITOR_STATUS_HEIGHT) / CODE_EDITOR_LINE_HEIGHT;
    if (visible_lines < 1) visible_lines = 1;
    int line = cursor_line();
    if (line < g_scroll_line) g_scroll_line = line;
    if (line >= g_scroll_line + visible_lines) g_scroll_line = line - visible_lines + 1;
    int text_w = g_view_w - CODE_EDITOR_GUTTER_WIDTH - 24;
    int caret = prefix_width(line_start_for_cursor(), g_cursor - line_start_for_cursor());
    if (caret < g_scroll_x) g_scroll_x = caret - 20;
    if (caret > g_scroll_x + text_w) g_scroll_x = caret - text_w + 20;
    if (g_scroll_x < 0) g_scroll_x = 0;
    if (g_scroll_line < 0) g_scroll_line = 0;
}

static void replace_range(size_t a, size_t b, const char *bytes, size_t n) {
    if (g_length - (b - a) + n >= CODE_EDITOR_CAPACITY) return;
    memmove(g_text + a + n, g_text + b, g_length - b + 1);
    memcpy(g_text + a, bytes, n);
    g_length = g_length - (b - a) + n;
}

static size_t indent_length(size_t start) {
    size_t n = 0;
    while (start + n < g_length && (g_text[start + n] == ' ' || g_text[start + n] == '\t')) n++;
    return n;
}

static void dedent_closing_brace(void) {
    size_t brace = g_cursor - 1, ls = brace, p, depth = 0;
    while (ls > 0 && g_text[ls - 1] != '\n') ls--;
    for (p = ls; p < brace; p++) if (g_text[p] != ' ' && g_text[p] != '\t') return;
    p = brace;
    while (p > 0) {
        p--;
        if (g_text[p] == '}') depth++;
        else if (g_text[p] == '{') {
            if (depth == 0) break;
            depth--;
        }
        if (p == 0) return;
    }
    if (g_text[p] != '{') return;
    while (p > 0 && g_text[p - 1] != '\n') p--;
    size_t indent = indent_length(p);
    char copy[128];
    if (indent >= sizeof(copy)) return;
    memcpy(copy, g_text + p, indent);
    replace_range(ls, brace, copy, indent);
    g_cursor = ls + indent + 1;
    g_anchor = g_cursor;
}

static char closing_pair(char c) {
    switch (c) {
        case '(': return ')';
        case '[': return ']';
        case '{': return '}';
        case '"': return '"';
        case '\'': return '\'';
        default: return 0;
    }
}

static void type_text(const char *typed) {
    char c = typed[0];
    int single = typed[1] == '\0';
    char next = g_cursor < g_length ? g_text[g_cursor] : '\0';
    char prev = g_cursor > 0 ? g_text[g_cursor - 1] : '\0';
    char close = single ? closing_pair(c) : 0;
    if (single && g_anchor == g_cursor && next == c && (c == ')' || c == ']' || c == '}' || c == '"' || c == '\'')) {
        g_cursor++;
        g_anchor = g_cursor;
        return;
    }
    if (close && (c != '"' && c != '\'' ? 1 : (!is_word_char(prev) && !is_word_char(next) && prev != c))) {
        char pair[2] = { c, close };
        insert_bytes(pair, 2);
        g_cursor--;
        g_anchor = g_cursor;
        return;
    }
    insert_bytes(typed, strlen(typed));
    if (single && c == '}') dedent_closing_brace();
}

static void smart_enter(void) {
    size_t start = line_start_for_cursor();
    size_t indent = indent_length(start);
    char buffer[200];
    size_t n = 0;
    int opens = g_cursor > 0 && g_text[g_cursor - 1] == '{';
    int between = opens && g_cursor < g_length && g_text[g_cursor] == '}';
    if (indent > g_cursor - start) indent = g_cursor - start;
    if (indent > 80) indent = 80;
    buffer[n++] = '\n';
    memcpy(buffer + n, g_text + start, indent);
    n += indent;
    if (opens) { memcpy(buffer + n, "    ", 4); n += 4; }
    if (between) {
        size_t after = n;
        buffer[n++] = '\n';
        memcpy(buffer + n, g_text + start, indent);
        n += indent;
        insert_bytes(buffer, n);
        g_cursor -= n - after;
        g_anchor = g_cursor;
        return;
    }
    insert_bytes(buffer, n);
}

static void smart_backspace(void) {
    if (g_anchor != g_cursor) { delete_selection(); return; }
    if (g_cursor == 0) return;
    char prev = g_text[g_cursor - 1];
    char next = g_cursor < g_length ? g_text[g_cursor] : '\0';
    if (next && closing_pair(prev) == next) {
        memmove(g_text + g_cursor - 1, g_text + g_cursor + 1, g_length - g_cursor);
        g_length -= 2;
        g_cursor--;
        g_anchor = g_cursor;
        g_dirty = 1;
        return;
    }
    size_t start = line_start_for_cursor(), width = g_cursor - start;
    if (width >= 4 && width % 4 == 0) {
        size_t i;
        for (i = start; i < g_cursor; i++) if (g_text[i] != ' ') break;
        if (i == g_cursor) {
            g_anchor = g_cursor - 4;
            delete_selection();
            return;
        }
    }
    g_cursor--;
    g_anchor = g_cursor + 1;
    delete_selection();
}

static void edit_input(void) {
    const char *typed = window_text_input_this_frame();
    if (window_key_just_pressed_undo()) { do_undo(); return; }
    if (window_key_just_pressed_redo()) { do_redo(); return; }
    if (window_key_just_pressed_select_all()) { g_anchor = 0; g_cursor = g_length; return; }
    if (window_key_just_pressed_enter() || (window_key_just_pressed_tab() && g_completion_count == 0) || window_key_just_pressed_paste() || window_key_just_pressed_cut()) begin_edit(4);
    else if (window_key_just_pressed_backspace()) begin_edit(2);
    else if (window_key_just_pressed_delete()) begin_edit(3);
    else if (typed && typed[0]) begin_edit(1);
    if (typed && typed[0]) type_text(typed);
    if (window_key_just_pressed_enter()) smart_enter();
    if (window_key_just_pressed_tab()) {
        if (g_completion_count > 0) { g_last_kind = 0; accept_completion(0); }
        else insert_bytes("    ", 4);
    }
    if (window_key_just_pressed_backspace()) smart_backspace();
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
    if (typed && typed[0]) g_follow_cursor = 1;
    if (window_key_just_pressed_enter() || window_key_just_pressed_tab() || window_key_just_pressed_backspace() || window_key_just_pressed_delete() ||
        window_key_just_pressed_left() || window_key_just_pressed_right() || window_key_just_pressed_up() || window_key_just_pressed_down() ||
        window_key_just_pressed_home() || window_key_just_pressed_end() || window_key_just_pressed_paste()) g_follow_cursor = 1;
    normalize_view();
    update_completion();
    update_matching_bracket();
    if (g_dirty) invalidate_view();
}

static void load_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { set_status("Could not open file"); return; }
    size_t n = fread(g_text, 1, CODE_EDITOR_CAPACITY - 1, f);
    fclose(f);
    g_text[n] = '\0';
    free_states(g_undo, &g_undo_count);
    free_states(g_redo, &g_redo_count);
    g_comp_cursor = (size_t)-1;
    g_length = n;
    g_cursor = g_anchor = 0;
    g_scroll_line = g_scroll_x = 0;
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
        set_status("File is already open");
        return 1;
    }
    if (g_dirty) {
        set_status("Save the modified script before opening another file");
        return 0;
    }
    load_file(path);
    g_active = 1;
    return g_path[0] != '\0' && strcmp(g_path, path) == 0;
}

static void on_open_file(const char *path, void *unused) {
    (void)unused;
    if (path) code_editor_open_path(path);
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
}

static void open_file_dialog(void) {
    window_stop_text_input();
    file_manager_open(FILE_MANAGER_ROOT_SOURCE, FILE_MANAGER_MODE_PICK_FILE,
                      g_code_extensions, CODE_EDITOR_EXT_COUNT,
                      on_open_file, NULL);
}

static void save_file(void) {
    if (g_path[0]) save_to_path(g_path);
    else file_manager_open_save(FILE_MANAGER_ROOT_SOURCE, g_title, on_save_file, NULL);
}

static void new_file(void) {
    g_text[0] = '\0';
    free_states(g_undo, &g_undo_count);
    free_states(g_redo, &g_redo_count);
    g_comp_cursor = (size_t)-1;
    g_length = 0;
    g_cursor = g_anchor = 0;
    g_scroll_line = g_scroll_x = 0;
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

static int word_in_list(const char *const *list, size_t count, const char *text, int start, int end) {
    int n = end - start;
    for (size_t i = 0; i < count; i++)
        if ((int)strlen(list[i]) == n && memcmp(text + start, list[i], (size_t)n) == 0) return 1;
    return 0;
}

static int is_control_keyword(const char *text, int start, int end) {
    static const char *const list[] = {
        "break", "case", "catch", "continue", "default", "do", "else", "for", "goto",
        "if", "return", "switch", "throw", "try", "while", "co_await", "co_return", "co_yield"
    };
    return word_in_list(list, sizeof(list) / sizeof(list[0]), text, start, end);
}

static int is_cpp_keyword(const char *text, int start, int end) {
    static const char *const list[] = {
        "alignas", "alignof", "asm", "auto", "bool", "char", "class", "const", "constexpr", "consteval",
        "constinit", "const_cast", "decltype", "delete", "double", "dynamic_cast", "enum", "explicit",
        "export", "extern", "false", "final", "float", "friend", "inline", "int", "long", "mutable",
        "namespace", "new", "noexcept", "nullptr", "operator", "override", "private", "protected",
        "public", "register", "reinterpret_cast", "restrict", "short", "signed", "sizeof", "static",
        "static_assert", "static_cast", "struct", "template", "this", "thread_local", "true", "typedef",
        "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t",
        "concept", "requires", "_Bool", "_Atomic", "_Alignas", "_Generic", "_Noreturn", "_Static_assert"
    };
    return word_in_list(list, sizeof(list) / sizeof(list[0]), text, start, end);
}

static int is_known_type(const char *text, int start, int end) {
    static const char *const list[] = {
        "size_t", "ssize_t", "ptrdiff_t", "intptr_t", "uintptr_t", "FILE", "va_list", "wchar_t", "time_t"
    };
    int n = end - start;
    if (word_in_list(list, sizeof(list) / sizeof(list[0]), text, start, end)) return 1;
    return n > 2 && text[end - 2] == '_' && text[end - 1] == 't';
}

static int is_all_caps(const char *text, int start, int end) {
    int letters = 0;
    for (int i = start; i < end; i++) {
        if (islower((unsigned char)text[i])) return 0;
        if (isupper((unsigned char)text[i])) letters++;
    }
    return letters >= 2 || (letters == 1 && end - start >= 2);
}

static void color_string_parts(const char *text, int start, int end, unsigned char *rgb) {
    for (int i = start + 1; i < end; i++) {
        if (text[i] == '\\' && i + 1 < end) {
            int stop = i + 2;
            if (text[i + 1] == 'x') while (stop < end && isxdigit((unsigned char)text[stop])) stop++;
            else if (text[i + 1] >= '0' && text[i + 1] <= '7') while (stop < end && stop < i + 4 && text[stop] >= '0' && text[stop] <= '7') stop++;
            color_bytes(rgb, i, stop, 215, 186, 125);
            i = stop - 1;
        } else if (text[i] == '%') {
            int stop = i + 1;
            if (stop < end && text[stop] == '%') { color_bytes(rgb, i, stop + 1, 86, 200, 214); i = stop; continue; }
            while (stop < end && strchr("-+ #0123456789.*lhzjtLqI", text[stop])) stop++;
            if (stop < end && strchr("diouxXeEfFgGaAcspn", text[stop])) {
                color_bytes(rgb, i, stop + 1, 86, 200, 214);
                i = stop;
            }
        }
    }
}

static void colorize_line(const char *text, int length, unsigned char *rgb,
                          int *in_block_comment) {
    color_bytes(rgb, 0, length, 212, 218, 228);
    int first = 0;
    int after_member = 0;
    while (first < length && isspace((unsigned char)text[first])) first++;
    if (!*in_block_comment && first < length && text[first] == '#') {
        int i = first + 1;
        while (i < length && isspace((unsigned char)text[i])) i++;
        while (i < length && isalpha((unsigned char)text[i])) i++;
        color_bytes(rgb, first, i, 197, 134, 192);
        while (i < length && isspace((unsigned char)text[i])) i++;
        if (i < length && (text[i] == '<' || text[i] == '"') && first + 8 <= length && strncmp(text + first, "#include", 8) == 0) {
            char close = text[i] == '<' ? '>' : '"';
            int stop = i + 1;
            while (stop < length && text[stop] != close) stop++;
            if (stop < length) stop++;
            color_bytes(rgb, i, stop, 206, 145, 120);
            i = stop;
        }
        first = i;
    }
    for (int i = first; i < length;) {
        if (*in_block_comment) {
            int start = i;
            while (i + 1 < length && !(text[i] == '*' && text[i + 1] == '/')) i++;
            if (i + 1 < length) { i += 2; *in_block_comment = 0; }
            else i = length;
            color_bytes(rgb, start, i, 106, 153, 85);
            continue;
        }
        if (i + 1 < length && text[i] == '/' && text[i + 1] == '/') {
            color_bytes(rgb, i, length, 106, 153, 85);
            break;
        }
        if (i + 1 < length && text[i] == '/' && text[i + 1] == '*') {
            int start = i;
            i += 2;
            while (i + 1 < length && !(text[i] == '*' && text[i + 1] == '/')) i++;
            if (i + 1 < length) i += 2;
            else { i = length; *in_block_comment = 1; }
            color_bytes(rgb, start, i, 106, 153, 85);
            continue;
        }
        if (text[i] == '"' || text[i] == '\'') {
            int start = i;
            char quote = text[i++];
            while (i < length) {
                if (text[i] == '\\' && i + 1 < length) { i += 2; continue; }
                if (text[i++] == quote) break;
            }
            color_bytes(rgb, start, i, 206, 145, 120);
            color_string_parts(text, start, i, rgb);
            after_member = 0;
            continue;
        }
        if (isalpha((unsigned char)text[i]) || text[i] == '_') {
            int start = i++;
            while (i < length && (isalnum((unsigned char)text[i]) || text[i] == '_')) i++;
            int next = i;
            while (next < length && isspace((unsigned char)text[next])) next++;
            int is_call = next < length && text[next] == '(';
            if (after_member) {
                if (is_call) color_bytes(rgb, start, i, 220, 220, 170);
                else color_bytes(rgb, start, i, 156, 220, 254);
            }
            else if (is_control_keyword(text, start, i)) color_bytes(rgb, start, i, 197, 134, 192);
            else if (is_cpp_keyword(text, start, i)) color_bytes(rgb, start, i, 86, 156, 214);
            else if (is_known_type(text, start, i)) color_bytes(rgb, start, i, 78, 201, 176);
            else if (is_call) color_bytes(rgb, start, i, 220, 220, 170);
            else if (i - start == 4 && memcmp(text + start, "NULL", 4) == 0) color_bytes(rgb, start, i, 86, 156, 214);
            else if (is_all_caps(text, start, i)) color_bytes(rgb, start, i, 79, 193, 255);
            else if (isupper((unsigned char)text[start])) color_bytes(rgb, start, i, 78, 201, 176);
            after_member = 0;
            continue;
        }
        if (isdigit((unsigned char)text[i]) || (text[i] == '.' && i + 1 < length && isdigit((unsigned char)text[i + 1]))) {
            int start = i++;
            while (i < length && (isalnum((unsigned char)text[i]) || text[i] == '.' || text[i] == '_' || text[i] == '\'' ||
                   ((text[i] == '+' || text[i] == '-') && (text[i - 1] == 'e' || text[i - 1] == 'E') && !(text[start] == '0' && start + 1 < length && (text[start + 1] == 'x' || text[start + 1] == 'X'))))) i++;
            color_bytes(rgb, start, i, 181, 206, 168);
            after_member = 0;
            continue;
        }
        if (text[i] == '.' || (text[i] == '-' && i + 1 < length && text[i + 1] == '>') || (text[i] == ':' && i + 1 < length && text[i + 1] == ':')) {
            after_member = 1;
            i += (text[i] == '.') ? 1 : 2;
            continue;
        }
        if (!isspace((unsigned char)text[i])) {
            color_bytes(rgb, i, i + 1, 170, 180, 196);
            after_member = 0;
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
        font_text_image_t image = font_render_text_colored(g_cached_line[line], FONT_WEIGHT_MONO, CODE_FONT, rgb);
        if (image.pixels != NULL) {
            g_line_text[line] = window_create_texture(image.pixels, image.px_w, image.px_h);
            g_line_width[line] = image.width;
            g_line_height[line] = image.height;
            font_free_text_image(&image);
        }
        g_line_number_text[line] = ui_make_text_texture(g_cached_number[line], FONT_WEIGHT_MONO, 11, &g_line_number_w[line], &g_line_number_h[line]);
        line++;
        if (end >= g_length) break;
        p = end + 1;
    }
    g_cached_line_count = line;
    g_max_line_w = 0;
    for (int i = 0; i < line; i++) if (g_line_width[i] > g_max_line_w) g_max_line_w = g_line_width[i];
    g_cache_dirty = 0;
}

static size_t position_at(int x, int top_area, int px, int py) {
    int line = g_scroll_line + (py - top_area) / CODE_EDITOR_LINE_HEIGHT;
    int total = line_count();
    if (line >= total) line = total - 1;
    if (line < 0) line = 0;
    size_t start = line_start(line);
    size_t end = line_end(start);
    int target = px - x - CODE_EDITOR_GUTTER_WIDTH - 8 + g_scroll_x;
    size_t pos = start;
    while (pos < end && prefix_width(start, pos + 1 - start) - 3 < target) pos++;
    return pos;
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
    for (int i = 0; i < CODE_EDITOR_BUTTONS; i++) if (button_hit(i, x, y, w)) g_hover_button = i;
    g_view_w = w;
    g_view_h = h;
    int top_area = y + CODE_EDITOR_TOOLBAR_HEIGHT + CODE_EDITOR_TAB_HEIGHT;
    int bottom_area = y + h - CODE_EDITOR_STATUS_HEIGHT;
    int in_area = mx >= x && mx < x + w && my >= top_area && my < bottom_area;
    int touch = window_pointer_is_touch();
    int popup_hit = -1;
    if (g_has_focus && g_completion_count > 0 && g_popup_w > 0 && mx >= g_popup_x && mx < g_popup_x + g_popup_w &&
        my >= g_popup_y && my < g_popup_y + g_popup_h) {
        popup_hit = (my - g_popup_y - 3) / 20;
        if (popup_hit >= g_completion_count) popup_hit = g_completion_count - 1;
        if (popup_hit < 0) popup_hit = 0;
    }
    if (window_mouse_left_just_pressed()) {
        if (popup_hit >= 0) {
            accept_completion(popup_hit);
            g_press_active = 0;
        } else {
            if (g_hover_button == 0) new_file();
            else if (g_hover_button == 1) open_file_dialog();
            else if (g_hover_button == 2) save_file();
            else if (g_hover_button == 3) file_manager_open_save(FILE_MANAGER_ROOT_SOURCE, g_title, on_save_file, NULL);
            else if (g_hover_button == 4) do_undo();
            else if (g_hover_button == 5) do_redo();
            if (!in_area && g_hover_button < 4) g_has_focus = 0;
            g_press_active = in_area;
            g_press_x = mx;
            g_press_y = my;
            g_dragged = 0;
            g_drag_accum = 0;
            g_long_press = 0;
            g_press_tick = window_ticks();
            if (in_area && !touch) {
                g_has_focus = 1;
                g_cursor = g_anchor = position_at(x, top_area, mx, my);
                g_preferred_column = 0;
                g_long_press = 1;
            }
        }
    }
    if (g_press_active && window_mouse_left_down()) {
        if (g_long_press) {
            g_cursor = position_at(x, top_area, mx, my);
        } else {
            int moved_y = my - g_press_y, moved_x = mx - g_press_x;
            if (!g_dragged && (moved_y > 14 || moved_y < -14 || moved_x > 14 || moved_x < -14)) g_dragged = 1;
            if (!g_dragged && window_ticks() - g_press_tick > CODE_EDITOR_LONG_PRESS_MS) {
                size_t pos = position_at(x, top_area, g_press_x, g_press_y);
                size_t a = pos, b = pos;
                while (a > 0 && is_word_char(g_text[a - 1])) a--;
                while (b < g_length && is_word_char(g_text[b])) b++;
                g_anchor = a;
                g_cursor = b;
                g_has_focus = 1;
                g_long_press = 1;
            }
            if (g_dragged) {
                g_drag_accum -= window_mouse_delta_y();
                while (g_drag_accum >= CODE_EDITOR_LINE_HEIGHT) { g_scroll_line++; g_drag_accum -= CODE_EDITOR_LINE_HEIGHT; }
                while (g_drag_accum <= -CODE_EDITOR_LINE_HEIGHT) { g_scroll_line--; g_drag_accum += CODE_EDITOR_LINE_HEIGHT; }
                g_scroll_x -= window_mouse_delta_x();
            }
        }
    }
    if (g_press_active && !window_mouse_left_down()) {
        g_press_active = 0;
        if (!g_dragged && !g_long_press) {
            g_has_focus = 1;
            g_cursor = g_anchor = position_at(x, top_area, g_press_x, g_press_y);
            g_preferred_column = 0;
        }
    }
    int editor_top = y + CODE_EDITOR_TOOLBAR_HEIGHT + CODE_EDITOR_TAB_HEIGHT;
    int editor_bottom = y + h - CODE_EDITOR_STATUS_HEIGHT;
    if (mx >= x && mx < x + w && my >= editor_top && my < editor_bottom) {
        int wheel = window_mouse_wheel_delta();
        if (wheel) g_scroll_line -= wheel * 3;
    }
    {
        int max_line = line_count() - 1;
        int visible = (editor_bottom - editor_top) / CODE_EDITOR_LINE_HEIGHT;
        if (g_scroll_line > max_line - visible + 1) g_scroll_line = max_line - visible + 1;
        if (g_scroll_line < 0) g_scroll_line = 0;
    }
    {
        int max_x = g_max_line_w - (w - CODE_EDITOR_GUTTER_WIDTH - 16) + 40;
        if (max_x < 0) max_x = 0;
        if (g_scroll_x > max_x) g_scroll_x = max_x;
        if (g_scroll_x < 0) g_scroll_x = 0;
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
    ui_color_t panel = UI_COLOR_SURFACE_RAISED;
    ui_color_t toolbar = UI_COLOR_SURFACE_PANEL;
    ui_color_t gutter = UI_COLOR_SURFACE_BASE;
    ui_color_t code_bg = UI_COLOR_SURFACE_RAISED;
    ui_color_t active_line = UI_COLOR_SURFACE_CONTROL;
    window_fill_rect(x, y, w, h, panel.r, panel.g, panel.b);
    window_fill_rect(x, y, w, CODE_EDITOR_TOOLBAR_HEIGHT, toolbar.r, toolbar.g, toolbar.b);
    const char *buttons[CODE_EDITOR_BUTTONS] = { "New", "Open", "Save", "Save As", "Undo", "Redo" };
    for (int i = 0; i < CODE_EDITOR_BUTTONS; i++) {
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
    window_set_clip_rect(x + CODE_EDITOR_GUTTER_WIDTH, top, w - CODE_EDITOR_GUTTER_WIDTH, bottom - top);
    for (int row = 0; row < visible; row++) {
        int line = g_scroll_line + row;
        if (line >= g_cached_line_count) break;
        int ly = top + row * CODE_EDITOR_LINE_HEIGHT;
        if (line == current) window_fill_rect(x + CODE_EDITOR_GUTTER_WIDTH, ly, w - CODE_EDITOR_GUTTER_WIDTH, CODE_EDITOR_LINE_HEIGHT,
                                                active_line.r, active_line.g, active_line.b);
        {
            size_t ls = line_start(line), le = line_end(ls);
            size_t sa = g_anchor < g_cursor ? g_anchor : g_cursor, sb = g_anchor < g_cursor ? g_cursor : g_anchor;
            if (sa != sb && sb > ls && sa <= le) {
                size_t from = sa > ls ? sa : ls, to = sb < le ? sb : le;
                int x1 = x + CODE_EDITOR_GUTTER_WIDTH + 8 + prefix_width(ls, from - ls) - g_scroll_x;
                int x2 = x + CODE_EDITOR_GUTTER_WIDTH + 8 + prefix_width(ls, to - ls) - g_scroll_x;
                if (sb > le) x2 += 6;
                if (x2 > x1) window_fill_rect(x1, ly, x2 - x1, CODE_EDITOR_LINE_HEIGHT, 0x26, 0x4F, 0x78);
            }
            if (g_matching_position >= 0 && g_matching_position >= (int)ls && g_matching_position <= (int)le) {
                int mx0 = x + CODE_EDITOR_GUTTER_WIDTH + 8 + prefix_width(ls, (size_t)g_matching_position - ls) - g_scroll_x;
                int mw = prefix_width(ls, (size_t)g_matching_position - ls + 1) - prefix_width(ls, (size_t)g_matching_position - ls);
                window_fill_rect(mx0, ly + 2, mw, 1, UI_RGB(UI_COLOR_CODE_CYAN));
                window_fill_rect(mx0, ly + CODE_EDITOR_LINE_HEIGHT - 3, mw, 1, UI_RGB(UI_COLOR_CODE_CYAN));
                window_fill_rect(mx0, ly + 2, 1, CODE_EDITOR_LINE_HEIGHT - 4, UI_RGB(UI_COLOR_CODE_CYAN));
                window_fill_rect(mx0 + mw - 1, ly + 2, 1, CODE_EDITOR_LINE_HEIGHT - 4, UI_RGB(UI_COLOR_CODE_CYAN));
            }
        }
        {
            static int indent_w = 0;
            const char *text = g_cached_line[line];
            int spaces = 0;
            if (indent_w == 0) indent_w = font_measure_text("    ", FONT_WEIGHT_MONO, CODE_FONT);
            while (text[spaces] == ' ') spaces++;
            if (text[spaces] != '\0' && indent_w > 0)
                for (int level = 0; level * 4 < spaces; level++)
                    window_fill_rect(x + CODE_EDITOR_GUTTER_WIDTH + 8 + level * indent_w - g_scroll_x, ly, 1, CODE_EDITOR_LINE_HEIGHT, 0x2E, 0x37, 0x44);
        }
        if (g_line_text[line]) window_draw_texture(g_line_text[line], x + CODE_EDITOR_GUTTER_WIDTH + 8 - g_scroll_x, ly + (CODE_EDITOR_LINE_HEIGHT - g_line_height[line]) / 2,
                                                    g_line_width[line], g_line_height[line]);
    }
    window_clear_clip_rect();
    window_fill_rect(x, top, CODE_EDITOR_GUTTER_WIDTH, bottom - top, gutter.r, gutter.g, gutter.b);
    for (int row = 0; row < visible; row++) {
        int line = g_scroll_line + row;
        if (line >= g_cached_line_count) break;
        if (g_line_number_text[line]) window_draw_texture_tinted(g_line_number_text[line], x + 6, top + row * CODE_EDITOR_LINE_HEIGHT + 4, g_line_number_w[line], g_line_number_h[line], UI_RGB(UI_COLOR_TEXT_DISABLED));
    }
    int cx = x + CODE_EDITOR_GUTTER_WIDTH + 8 + prefix_width(line_start_for_cursor(), g_cursor - line_start_for_cursor()) - g_scroll_x;
    int cy = top + (current - g_scroll_line) * CODE_EDITOR_LINE_HEIGHT;
    {
        static size_t last_cursor = (size_t)-1, last_length = (size_t)-1;
        static unsigned blink_base = 0;
        unsigned now = window_ticks();
        if (g_cursor != last_cursor || g_length != last_length || g_anchor != g_cursor) {
            last_cursor = g_cursor;
            last_length = g_length;
            blink_base = now;
        }
        int blink_on = ((now - blink_base) / 530) % 2 == 0;
        if (g_has_focus && blink_on && cy >= top && cy < bottom && cx >= x + CODE_EDITOR_GUTTER_WIDTH)
            window_fill_rect(cx, cy + 2, 2, CODE_EDITOR_LINE_HEIGHT - 4, UI_RGB(UI_COLOR_CODE_CYAN));
    }
    g_popup_w = 0;
    if (g_has_focus && g_completion_count > 0 && cy >= top && cy < bottom) {
        int popup_y = cy + CODE_EDITOR_LINE_HEIGHT;
        int popup_x = cx + 240 > x + w ? x + w - 240 : cx;
        if (popup_x < x + CODE_EDITOR_GUTTER_WIDTH) popup_x = x + CODE_EDITOR_GUTTER_WIDTH;
        if (popup_y + g_completion_count * 20 + 6 > bottom) popup_y = cy - g_completion_count * 20 - 6;
        g_popup_x = popup_x; g_popup_y = popup_y; g_popup_w = 230; g_popup_h = g_completion_count * 20 + 6;
        cx = popup_x;
        window_fill_rect(popup_x, popup_y, 230, g_completion_count * 20 + 6, UI_RGB(UI_COLOR_SURFACE_CONTROL));
        for (int i = 0; i < g_completion_count; i++) {
            int tw = 0, th = 0;
            window_texture_t *completion = ui_make_text_texture(g_completion[i], FONT_WEIGHT_MONO, 11, &tw, &th);
            if (completion) { window_draw_texture(completion, cx + 6, popup_y + 3 + i * 20, tw, th); window_destroy_texture(completion); }
        }
    }
    window_fill_rect(x, bottom, w, CODE_EDITOR_STATUS_HEIGHT, toolbar.r, toolbar.g, toolbar.b);
    char status[256];
    snprintf(status, sizeof(status), tr("%s  |  Ln %d, Col %d  |  %s"), g_status, current + 1, cursor_column() + 1,
             g_dirty ? tr("Modified") : tr("Saved"));
    int sw = 0, sh = 0;
    window_texture_t *st = ui_make_text_texture(status, FONT_WEIGHT_REGULAR, 11, &sw, &sh);
    if (st) { window_draw_texture(st, x + 8, bottom + (CODE_EDITOR_STATUS_HEIGHT - sh) / 2, sw, sh); window_destroy_texture(st); }
}

void code_editor_shutdown(void) {
    free_line_cache();
    free_states(g_undo, &g_undo_count);
    free_states(g_redo, &g_redo_count);
    for (int i = 0; i < CODE_EDITOR_BUTTONS; i++) if (g_button_text[i]) { window_destroy_texture(g_button_text[i]); g_button_text[i] = NULL; }
    if (g_tab_text) { window_destroy_texture(g_tab_text); g_tab_text = NULL; }
    g_active = 0;
    window_stop_text_input();
}
