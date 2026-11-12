#include <stdio.h>
#include <string.h>

#include "editor/dialogs/packages.h"
#include "packages/package_manager.h"
#include "editor/ui/icon_atlas.h"
#include "editor/ui/base.h"
#include "editor/ui/dialog.h"
#include "editor/ui/scrollbar.h"
#include "platform/common/window.h"

#define DIALOG_MAX_W 720
#define DIALOG_MAX_H 580
#define TITLEBAR_H 40
#define PAD 20
#define ROW_H 38
#define CLOSE_SIZE 22
#define ARROW_SIZE 16
#define INDENT 34
#define HEADER_H 118
#define TEMPLATES_H 66
#define PROGRESS_H 62

typedef struct {
    int platform;
    int arch;
    int y;
} row_t;

typedef struct {
    int x, y, w, h;
    int list_x, list_y, list_w, list_h;
    int templates_y, progress_y;
} layout_t;

typedef struct {
    window_texture_t *tex;
    int w, h;
    char text[420];
} cached_text_t;

static int g_open;
static int g_ready;
static int g_expanded[PKG_PLATFORM_COUNT];
static int g_scroll;
static int g_dragging;
static int g_was_busy;
static int g_content_h;

static window_texture_t *g_title_tex;
static int g_title_w, g_title_h;
static window_texture_t *g_section_tex, *g_templates_tex, *g_templates_empty_tex, *g_compiler_tex;
static int g_section_w, g_section_h, g_templates_w, g_templates_h, g_templates_empty_w, g_templates_empty_h;
static int g_compiler_w, g_compiler_h;
static window_texture_t *g_platform_tex[PKG_PLATFORM_COUNT];
static int g_platform_w[PKG_PLATFORM_COUNT], g_platform_h[PKG_PLATFORM_COUNT];
static window_texture_t *g_arch_tex[PKG_PLATFORM_COUNT][4];
static int g_arch_w[PKG_PLATFORM_COUNT][4], g_arch_h[PKG_PLATFORM_COUNT][4];
static window_texture_t *g_state_tex[4];
static int g_state_w[4], g_state_h[4];
static labeled_button_t g_install_btn, g_remove_btn;
static window_texture_t *g_install_shape, *g_install_text, *g_remove_shape, *g_remove_text;
static cached_text_t g_title_line, g_message_line, g_percent_line, g_host_cache;

static const char *STATE_TEXT[4] = { "Not installed", "Partially installed", "Installed", "Not available on iOS" };

static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1;
    font_init();
    g_title_tex = ui_make_text_texture("Export Packages", FONT_WEIGHT_BOLD, 18, &g_title_w, &g_title_h);
    g_section_tex = ui_make_text_texture("Compilers and SDKs", FONT_WEIGHT_BOLD, 15, &g_section_w, &g_section_h);
    g_templates_tex = ui_make_text_texture("Export Templates", FONT_WEIGHT_BOLD, 15, &g_templates_w, &g_templates_h);
    g_templates_empty_tex = ui_make_text_texture("No export templates are available yet.", FONT_WEIGHT_REGULAR, 12,
                                                 &g_templates_empty_w, &g_templates_empty_h);
    g_compiler_tex = ui_make_text_texture("Compiler (clang) for this device", FONT_WEIGHT_REGULAR, 14,
                                          &g_compiler_w, &g_compiler_h);
    for (int p = 0; p < PKG_PLATFORM_COUNT; p++) {
        g_platform_tex[p] = ui_make_text_texture(package_platform_name(p), FONT_WEIGHT_BOLD, 14,
                                                 &g_platform_w[p], &g_platform_h[p]);
        for (int a = 0; a < package_platform_arch_count(p); a++)
            g_arch_tex[p][a] = ui_make_text_texture(package_arch_name(p, a), FONT_WEIGHT_REGULAR, 14,
                                                    &g_arch_w[p][a], &g_arch_h[p][a]);
    }
    for (int s = 0; s < 4; s++)
        g_state_tex[s] = ui_make_text_texture(STATE_TEXT[s], FONT_WEIGHT_REGULAR, 12, &g_state_w[s], &g_state_h[s]);

    g_install_btn = labeled_button_create("Install", FONT_WEIGHT_REGULAR, 13, 14, 6, 4,
                                          UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    g_remove_btn = labeled_button_create("Uninstall", FONT_WEIGHT_REGULAR, 13, 14, 6, 4, 0x74, 0x3B, 0x3B);
    if (g_install_btn.width > 0) {
        g_install_shape = window_create_texture(g_install_btn.shape_img.pixels, g_install_btn.shape_img.width, g_install_btn.shape_img.height);
        g_install_text = window_create_texture(g_install_btn.text_img.pixels, g_install_btn.text_img.width, g_install_btn.text_img.height);
    }
    if (g_remove_btn.width > 0) {
        g_remove_shape = window_create_texture(g_remove_btn.shape_img.pixels, g_remove_btn.shape_img.width, g_remove_btn.shape_img.height);
        g_remove_text = window_create_texture(g_remove_btn.text_img.pixels, g_remove_btn.text_img.width, g_remove_btn.text_img.height);
    }
}

static void cache_set(cached_text_t *c, const char *text, int size) {
    if (strcmp(c->text, text) == 0 && (c->tex || text[0] == '\0')) return;
    if (c->tex) window_destroy_texture(c->tex);
    c->tex = NULL;
    snprintf(c->text, sizeof(c->text), "%s", text);
    if (text[0]) c->tex = ui_make_text_texture(text, FONT_WEIGHT_REGULAR, size, &c->w, &c->h);
}

static void cache_free(cached_text_t *c) {
    if (c->tex) window_destroy_texture(c->tex);
    memset(c, 0, sizeof(*c));
}

static void compute_layout(int window_w, int window_h, layout_t *l) {
    l->w = window_w - 40 < DIALOG_MAX_W ? window_w - 40 : DIALOG_MAX_W;
    l->h = window_h - 40 < DIALOG_MAX_H ? window_h - 40 : DIALOG_MAX_H;
    if (l->w < 320) l->w = 320;
    if (l->h < 360) l->h = 360;
    ui_center_rect(window_w, window_h, l->w, l->h, &l->x, &l->y);
    l->list_x = l->x + PAD;
    l->list_w = l->w - PAD * 2;
    l->list_y = l->y + TITLEBAR_H + HEADER_H;
    l->progress_y = l->y + l->h - PAD - PROGRESS_H;
    l->templates_y = l->progress_y - TEMPLATES_H - 8;
    l->list_h = l->templates_y - 8 - l->list_y;
    if (l->list_h < ROW_H) l->list_h = ROW_H;
}

static int build_rows(row_t *rows, int cap) {
    int n = 0, y = 0;
    for (int p = 0; p < PKG_PLATFORM_COUNT && n < cap; p++) {
        rows[n].platform = p; rows[n].arch = -1; rows[n].y = y; n++; y += ROW_H;
        if (g_expanded[p] && package_platform_has_archs(p)) {
            for (int a = 0; a < package_platform_arch_count(p) && n < cap; a++) {
                rows[n].platform = p; rows[n].arch = a; rows[n].y = y; n++; y += ROW_H;
            }
        }
    }
    g_content_h = y;
    return n;
}

static int button_x(const layout_t *l, const labeled_button_t *b, int scrollbar) {
    return l->list_x + l->list_w - (scrollbar ? UI_SCROLLBAR_WIDTH + 6 : 0) - b->width - 10;
}

static int is_installed_row(const row_t *r) {
    if (r->arch < 0) {
        if (package_platform_has_archs(r->platform) && g_expanded[r->platform]) return -1;
        return package_platform_state(r->platform) == PKG_STATE_INSTALLED;
    }
    return package_arch_state(r->platform, r->arch) == PKG_STATE_INSTALLED;
}

static pkg_state_t row_state(const row_t *r) {
    if (r->arch < 0) return package_platform_state(r->platform);
    return package_arch_state(r->platform, r->arch);
}

void packages_dialog_open(void) {
    ensure_ready();
    package_manager_init();
    package_refresh_states();
    g_was_busy = package_is_busy();
    g_scroll = 0;
    g_open = 1;
}

int packages_dialog_is_open(void) { return g_open; }

void packages_dialog_update(int window_w, int window_h) {
    if (!g_open) return;
    layout_t l;
    compute_layout(window_w, window_h, &l);
    int mx = window_mouse_x(), my = window_mouse_y();
    int busy = package_is_busy();
    if (g_was_busy && !busy) package_refresh_states();
    g_was_busy = busy;

    row_t rows[64];
    int count = build_rows(rows, 64);
    ui_scrollbar_update(&g_scroll, &g_dragging, l.list_x, l.list_w, l.list_y, l.list_h, g_content_h, ROW_H);
    int max_scroll = g_content_h - l.list_h;
    if (max_scroll < 0) max_scroll = 0;
    if (g_scroll > max_scroll) g_scroll = max_scroll;
    if (g_scroll < 0) g_scroll = 0;

    if (!window_mouse_left_just_pressed()) return;

    if (ui_dialog_close_button_hit(mx, my, l.x + l.w - PAD - CLOSE_SIZE, l.y + (TITLEBAR_H - CLOSE_SIZE) / 2, CLOSE_SIZE)) {
        g_open = 0;
        return;
    }
    if (!package_host_supported()) return;

    if (package_host_compiler_state() == PKG_STATE_NOT_INSTALLED && !busy) {
        int bx = l.x + l.w - PAD - g_install_btn.width;
        int by = l.y + TITLEBAR_H + 70 + (ROW_H - g_install_btn.height) / 2;
        if (ui_point_in_rect(mx, my, bx, by, g_install_btn.width, g_install_btn.height)) {
            package_install_host_compiler();
            return;
        }
    }

    if (!ui_point_in_rect(mx, my, l.list_x, l.list_y, l.list_w, l.list_h)) return;
    int scrollbar = g_content_h > l.list_h;
    for (int i = 0; i < count; i++) {
        row_t *r = &rows[i];
        int ry = l.list_y + r->y - g_scroll;
        if (my < ry || my >= ry + ROW_H) continue;
        int installed = is_installed_row(r);
        pkg_state_t st = row_state(r);
        if (installed != -1 && st != PKG_STATE_UNAVAILABLE && !busy) {
            const labeled_button_t *b = installed ? &g_remove_btn : &g_install_btn;
            int bx = button_x(&l, b, scrollbar);
            int by = ry + (ROW_H - b->height) / 2;
            if (ui_point_in_rect(mx, my, bx, by, b->width, b->height)) {
                if (r->arch < 0 && package_platform_has_archs(r->platform)) {
                    if (installed) package_uninstall_platform(r->platform);
                    else package_install_platform(r->platform);
                } else {
                    int a = r->arch < 0 ? 0 : r->arch;
                    if (installed) package_uninstall_arch(r->platform, a);
                    else package_install_arch(r->platform, a);
                }
                return;
            }
        }
        if (r->arch < 0 && package_platform_has_archs(r->platform)) {
            g_expanded[r->platform] = !g_expanded[r->platform];
        }
        return;
    }
}

static void draw_button(const labeled_button_t *b, window_texture_t *shape, window_texture_t *text, int x, int y) {
    if (shape) window_draw_texture(shape, x, y, b->width, b->height);
    if (text) window_draw_texture(text, x + b->text_offset_x, y + b->text_offset_y, b->text_img.width, b->text_img.height);
}

static void state_color(pkg_state_t s, unsigned char *r, unsigned char *g, unsigned char *b) {
    if (s == PKG_STATE_INSTALLED) { *r = 92; *g = 184; *b = 92; }
    else if (s == PKG_STATE_PARTIAL) { *r = 220; *g = 195; *b = 125; }
    else if (s == PKG_STATE_UNAVAILABLE) { *r = 217; *g = 92; *b = 92; }
    else { *r = 170; *g = 175; *b = 182; }
}

static void draw_state(pkg_state_t s, int right_x, int cy) {
    unsigned char r, g, b;
    state_color(s, &r, &g, &b);
    if (g_state_tex[s]) window_draw_texture_tinted(g_state_tex[s], right_x - g_state_w[s], cy - g_state_h[s] / 2,
                                                   g_state_w[s], g_state_h[s], r, g, b);
}

void packages_dialog_draw(int window_w, int window_h) {
    if (!g_open) return;
    layout_t l;
    compute_layout(window_w, window_h, &l);
    ui_dialog_draw_backdrop(window_w, window_h);
    ui_dialog_draw_frame(l.x, l.y, l.w, l.h);
    window_fill_rect(l.x, l.y, l.w, TITLEBAR_H, 0x29, 0x2B, 0x30);
    ui_dialog_draw_title_left(g_title_tex, g_title_w, g_title_h, l.x, l.y, TITLEBAR_H, PAD);
    ui_dialog_draw_close_button(l.x + l.w - PAD - CLOSE_SIZE, l.y + (TITLEBAR_H - CLOSE_SIZE) / 2, CLOSE_SIZE);

    int busy = package_is_busy();
    int hx = l.x + PAD;
    int hy = l.y + TITLEBAR_H + 14;
    if (g_section_tex) window_draw_texture(g_section_tex, hx, hy, g_section_w, g_section_h);
    static char host_line[96];
    snprintf(host_line, sizeof(host_line), "This device: %s", package_host_text());
    cache_set(&g_host_cache, host_line, 12);
    if (g_host_cache.tex) window_draw_texture_tinted(g_host_cache.tex, hx, hy + g_section_h + 6, g_host_cache.w, g_host_cache.h, 170, 175, 182);

    int comp_y = l.y + TITLEBAR_H + 70;
    window_fill_rect(l.list_x, comp_y, l.list_w, ROW_H - 4, 0x30, 0x32, 0x37);
    if (g_compiler_tex) window_draw_texture(g_compiler_tex, l.list_x + 12, comp_y + (ROW_H - 4 - g_compiler_h) / 2, g_compiler_w, g_compiler_h);
    pkg_state_t cs = package_host_compiler_state();
    int right = l.list_x + l.list_w - 12;
    if (cs == PKG_STATE_NOT_INSTALLED && !busy) {
        draw_button(&g_install_btn, g_install_shape, g_install_text, l.list_x + l.list_w - g_install_btn.width, comp_y + (ROW_H - g_install_btn.height) / 2);
        right = l.list_x + l.list_w - g_install_btn.width - 12;
    }
    draw_state(cs, right, comp_y + (ROW_H - 4) / 2);

    window_fill_rect(l.list_x, l.list_y - 4, l.list_w, l.list_h + 8, 0x2E, 0x30, 0x35);
    window_set_clip_rect(l.list_x, l.list_y, l.list_w, l.list_h);
    row_t rows[64];
    int count = build_rows(rows, 64);
    int scrollbar = g_content_h > l.list_h;
    int mx = window_mouse_x(), my = window_mouse_y();
    for (int i = 0; i < count; i++) {
        row_t *r = &rows[i];
        int ry = l.list_y + r->y - g_scroll;
        if (ry + ROW_H < l.list_y || ry > l.list_y + l.list_h) continue;
        int is_arch = r->arch >= 0;
        int hover = ui_point_in_rect(mx, my, l.list_x, ry, l.list_w, ROW_H) && ui_point_in_rect(mx, my, l.list_x, l.list_y, l.list_w, l.list_h);
        if (hover) window_fill_rect(l.list_x, ry, l.list_w, ROW_H - 2, UI_COLOR_HOVER_BLUE.r, UI_COLOR_HOVER_BLUE.g, UI_COLOR_HOVER_BLUE.b);
        else window_fill_rect(l.list_x, ry, l.list_w, ROW_H - 2, 0x35, 0x37, 0x3C);
        int cy = ry + (ROW_H - 2) / 2;
        int tx = l.list_x + 12 + (is_arch ? INDENT : 0);
        if (!is_arch && package_platform_has_archs(r->platform)) {
            icon_atlas_draw_rotated(ICON_tree_expand_collapse, tx, cy - ARROW_SIZE / 2, ARROW_SIZE, g_expanded[r->platform] ? 90.0 : 0.0);
            tx += ARROW_SIZE + 8;
        } else if (!is_arch) {
            tx += ARROW_SIZE + 8;
        }
        if (is_arch) {
            if (g_arch_tex[r->platform][r->arch]) window_draw_texture(g_arch_tex[r->platform][r->arch], tx, cy - g_arch_h[r->platform][r->arch] / 2, g_arch_w[r->platform][r->arch], g_arch_h[r->platform][r->arch]);
        } else if (g_platform_tex[r->platform]) {
            window_draw_texture(g_platform_tex[r->platform], tx, cy - g_platform_h[r->platform] / 2, g_platform_w[r->platform], g_platform_h[r->platform]);
        }
        int installed = is_installed_row(r);
        pkg_state_t st = row_state(r);
        int right_edge = l.list_x + l.list_w - (scrollbar ? UI_SCROLLBAR_WIDTH + 6 : 0) - 10;
        if (installed != -1 && st != PKG_STATE_UNAVAILABLE && !busy) {
            const labeled_button_t *b = installed ? &g_remove_btn : &g_install_btn;
            draw_button(b, installed ? g_remove_shape : g_install_shape, installed ? g_remove_text : g_install_text,
                        right_edge - b->width, ry + (ROW_H - 2 - b->height) / 2);
            right_edge -= b->width + 14;
        } else {
            right_edge -= 4;
        }
        if (!(r->arch < 0 && g_expanded[r->platform] && package_platform_has_archs(r->platform))) draw_state(st, right_edge, cy);
    }
    window_clear_clip_rect();
    ui_scrollbar_draw(g_scroll, l.list_x, l.list_w, l.list_y, l.list_h, g_content_h);

    if (g_templates_tex) window_draw_texture(g_templates_tex, l.list_x, l.templates_y, g_templates_w, g_templates_h);
    if (g_templates_empty_tex) window_draw_texture_tinted(g_templates_empty_tex, l.list_x, l.templates_y + g_templates_h + 8,
                                                          g_templates_empty_w, g_templates_empty_h, 170, 175, 182);

    pkg_status_t st;
    package_get_status(&st);
    char line[300];
    if (st.busy) {
        snprintf(line, sizeof(line), "%s", st.title);
        cache_set(&g_title_line, line, 13);
        snprintf(line, sizeof(line), "%s %d%%", st.extracting ? "Extracting" : "Downloading", st.percent);
        cache_set(&g_percent_line, line, 12);
        if (g_title_line.tex) window_draw_texture(g_title_line.tex, l.list_x, l.progress_y, g_title_line.w, g_title_line.h);
        if (g_percent_line.tex) window_draw_texture_tinted(g_percent_line.tex, l.list_x + l.list_w - g_percent_line.w, l.progress_y,
                                                           g_percent_line.w, g_percent_line.h, 170, 175, 182);
        int by = l.progress_y + 24;
        window_fill_rect(l.list_x, by, l.list_w, 8, 0x22, 0x23, 0x27);
        window_fill_rect(l.list_x, by, l.list_w * st.percent / 100, 8, 0x4F, 0x86, 0xB5);
    } else {
        cache_set(&g_title_line, "", 13);
        cache_set(&g_percent_line, "", 12);
    }
    cache_set(&g_message_line, st.message, 12);
    if (g_message_line.tex) {
        unsigned char r = 170, g = 175, b = 182;
        if (st.message_kind == PKG_MSG_ERROR) { r = 217; g = 92; b = 92; }
        else if (st.message_kind == PKG_MSG_WARNING) { r = 220; g = 195; b = 125; }
        else if (st.message_kind == PKG_MSG_INFO) { r = 92; g = 184; b = 92; }
        window_draw_texture_tinted(g_message_line.tex, l.list_x, l.progress_y + (st.busy ? 40 : 6), g_message_line.w, g_message_line.h, r, g, b);
    }
}

void packages_dialog_shutdown(void) {
    window_texture_t **singles[] = { &g_title_tex, &g_section_tex, &g_templates_tex, &g_templates_empty_tex, &g_compiler_tex,
                                     &g_install_shape, &g_install_text, &g_remove_shape, &g_remove_text };
    for (size_t i = 0; i < sizeof(singles) / sizeof(singles[0]); i++) {
        if (*singles[i]) window_destroy_texture(*singles[i]);
        *singles[i] = NULL;
    }
    for (int p = 0; p < PKG_PLATFORM_COUNT; p++) {
        if (g_platform_tex[p]) window_destroy_texture(g_platform_tex[p]);
        g_platform_tex[p] = NULL;
        for (int a = 0; a < 4; a++) {
            if (g_arch_tex[p][a]) window_destroy_texture(g_arch_tex[p][a]);
            g_arch_tex[p][a] = NULL;
        }
    }
    for (int s = 0; s < 4; s++) {
        if (g_state_tex[s]) window_destroy_texture(g_state_tex[s]);
        g_state_tex[s] = NULL;
    }
    labeled_button_free(&g_install_btn);
    labeled_button_free(&g_remove_btn);
    cache_free(&g_title_line);
    cache_free(&g_message_line);
    cache_free(&g_percent_line);
    cache_free(&g_host_cache);
    package_manager_shutdown();
    g_open = 0;
    g_ready = 0;
}
