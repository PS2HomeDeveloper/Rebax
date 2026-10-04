/*
 * ============================================================
 * file_manager.c
 * ============================================================
 * Browse a single folder at a time (not a recursive search through all subfolders)
 * - exactly like any normal file manager: enter a folder, see only its contents,
 * and navigate by entering subfolders or going back via the path bar. This is
 * intentional: it matches the desired breadcrumb behavior and is much cheaper
 * than scanning an entire tree every time the window opens.
 *
 * Thumbnails: actually decoded via stb_image for each image file present in the
 * current folder when entering it (not all icons at once, only the displayed folder contents)
 * - any file whose decode fails (corrupt despite correct extension) keeps the default
 * image icon instead of its thumbnail, exactly as desired. Files larger than THUMB_MAX_FILE_SIZE
 * are excluded from thumbnailing entirely (to avoid freezing the UI on a huge file)
 * and are shown with their default icon directly.
 *
 * Note: STB_IMAGE_IMPLEMENTATION is not defined here - icon_atlas.c is the sole
 * definition in the project (see that file's comment). Here we only include the header
 * and use the ready-made functions from that compiled implementation.
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strcasecmp */
#include <stddef.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <direct.h>
#define REBAX_MKDIR(path, mode) _mkdir(path)
#else
#define REBAX_MKDIR(path, mode) mkdir(path, mode)
#endif

#include "nodes_editor/image_loader.h" /* Supports all image formats our engine supports - stb_image internally for PNG/JPEG/BMP/TGA/TIFF, and our own decoders for RAW/TIM2/TIM */

#include "file_manager.h"
#include "window.h"
#include "shape_provider.h"
#include "icon_atlas.h"
#include "labeled_button.h"
#include "text_field.h"
#include "font.h"
#include "current_project.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_selection.h"
#include "ui_dialog.h"
#include "path_utils.h"
#include "file_icons.h"

/* ------------------------------------------------------------
 * Layout constants
 * ------------------------------------------------------------ */
#define DIALOG_WIDTH        640
#define DIALOG_HEIGHT        460
#define DIALOG_PADDING         16
#define TITLEBAR_HEIGHT        32
#define CLOSE_BTN_SIZE          22
#define CRUMB_BAR_HEIGHT        24
#define TOOLBAR_HEIGHT          30
#define GRID_TOP_GAP             8
#define FOOTER_HEIGHT           44
#define FILENAME_ROW_HEIGHT     32 /* One extra row only in SAVE_FILE mode - the filename field + its label */

#define CELL_W                  84
#define CELL_H                  96
#define GRID_ICON_SIZE          48
#define CELL_CORNER_RADIUS       8
#define LABEL_FONT_SIZE         14 /* Matches the item name size in file_system_panel.c exactly - it was 12 by mistake, inconsistent with other file managers in the engine */

#define SCROLL_STEP_PX           40

#define AB_MAX_ENTRIES          512
#define AB_MAX_PATH              900
#define AB_MAX_EXTS               16
#define AB_MAX_CRUMBS             24

#define THUMB_MAX_FILE_SIZE (8 * 1024 * 1024) /* 8 megabytes - larger than this is excluded from thumbnailing */

/* ------------------------------------------------------------
 * Internal data types
 * ------------------------------------------------------------ */
typedef struct {
    char full_path[AB_MAX_PATH];
    char display_name[160];
    int  is_dir;

    window_texture_t *thumb_tex;   /* NULL if there is no real thumbnail (the extension icon is used instead) */
    int  thumb_w, thumb_h;         /* Actual thumbnail dimensions (aspect ratio preserved, no stretching) */

    window_texture_t *label_tex;
    int  label_w, label_h;
} ab_entry_t;

/* ------------------------------------------------------------
 * Global state (single instance across the program - same pattern as other
 * pop-up windows in the project: add_node_dialog, project_dialog)
 * ------------------------------------------------------------ */
static int g_ready = 0;
static int g_is_open = 0;

static file_manager_root_t g_root_mode;
static file_manager_mode_t g_pick_mode;
static file_manager_callback_t g_callback = NULL;
static void *g_user_data = NULL;

static char g_extensions[AB_MAX_EXTS][16];
static int  g_extension_count = 0;

static char g_root_path[AB_MAX_PATH];
static char g_root_label[64];
static char g_current_path[AB_MAX_PATH];

static ab_entry_t g_entries[AB_MAX_ENTRIES];
static int g_entry_count = 0;

static char g_crumb_label[AB_MAX_CRUMBS][64];
static char g_crumb_path[AB_MAX_CRUMBS][AB_MAX_PATH];
static window_texture_t *g_crumb_tex[AB_MAX_CRUMBS];
static int  g_crumb_w[AB_MAX_CRUMBS], g_crumb_h[AB_MAX_CRUMBS];
static int  g_crumb_x[AB_MAX_CRUMBS], g_crumb_w_click[AB_MAX_CRUMBS]; /* Click rectangles calculated each frame */
static int  g_crumb_count = 0;
static window_texture_t *g_crumb_sep_tex = NULL;
static int  g_crumb_sep_w = 0, g_crumb_sep_h = 0;

static text_field_t g_search_field;

/* Only in SAVE_FILE mode - the filename field and its fixed label "File name:"
 * (built once in ensure_ready, doesn't change between window openings) */
static text_field_t g_filename_field;
static window_texture_t *g_filename_label_tex = NULL;
static int g_filename_label_w = 0, g_filename_label_h = 0;

static int g_selected_index = -1; /* In file-pick mode only - index into g_entries */
static int g_scroll_offset = 0;

static window_texture_t *g_title_tex = NULL;
static int g_title_w = 0, g_title_h = 0;

static window_texture_t *g_filter_label_tex = NULL; /* "*.png, *.jpg" next to the search - NULL if no filter */
static int g_filter_label_w = 0, g_filter_label_h = 0;

static labeled_button_t  g_cancel_btn, g_select_btn;
static window_texture_t *g_cancel_btn_tex = NULL, *g_cancel_txt_tex = NULL;
static window_texture_t *g_select_btn_tex = NULL, *g_select_txt_tex = NULL;

static window_texture_t *g_hover_tex = NULL;   /* Grid cell highlight on hover */
static window_texture_t *g_selected_tex = NULL; /* Highlight for the currently selected grid cell */


/* ------------------------------------------------------------
 * Small helper utilities
 * ------------------------------------------------------------ */

static int extension_matches_filter(const char *name) {
    if (g_extension_count <= 0) return 1; /* No filter = all files accepted */
    const char *dot = strrchr(name, '.');
    if (dot == NULL) return 0;
    const char *ext = dot + 1;
    for (int i = 0; i < g_extension_count; i++) {
        if (strcasecmp(ext, g_extensions[i]) == 0) return 1;
    }
    return 0;
}

/* Downscale raw image data (RGBA) to fit a dst_size×dst_size box without
 * distortion (letterbox - preserves aspect ratio), using nearest neighbor
 * sampling - perfectly adequate for such a small thumbnail size */
static window_texture_t *make_thumbnail_texture(const unsigned char *src, int sw, int sh,
                                                  int dst_size, int *out_w, int *out_h) {
    if (sw <= 0 || sh <= 0) return NULL;

    int dw, dh;
    if (sw >= sh) {
        dw = dst_size;
        dh = (int)((long)sh * dst_size / sw);
        if (dh < 1) dh = 1;
    } else {
        dh = dst_size;
        dw = (int)((long)sw * dst_size / sh);
        if (dw < 1) dw = 1;
    }

    unsigned char *dst = (unsigned char *)malloc((size_t)dw * (size_t)dh * 4);
    if (dst == NULL) return NULL;

    for (int y = 0; y < dh; y++) {
        int sy = (int)((long)y * sh / dh);
        for (int x = 0; x < dw; x++) {
            int sx = (int)((long)x * sw / dw);
            const unsigned char *sp = &src[(size_t)(sy * sw + sx) * 4];
            unsigned char *dp = &dst[(size_t)(y * dw + x) * 4];
            dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2]; dp[3] = sp[3];
        }
    }

    window_texture_t *tex = window_create_texture(dst, dw, dh);
    free(dst);
    *out_w = dw;
    *out_h = dh;
    return tex;
}

/* ------------------------------------------------------------
 * Load folder contents
 * ------------------------------------------------------------ */

static void free_entries(void) {
    for (int i = 0; i < g_entry_count; i++) {
        if (g_entries[i].thumb_tex != NULL) window_destroy_texture(g_entries[i].thumb_tex);
        if (g_entries[i].label_tex != NULL) window_destroy_texture(g_entries[i].label_tex);
    }
    g_entry_count = 0;
}

static void add_entry(const char *full_path, const char *display_name, int is_dir) {
    if (g_entry_count >= AB_MAX_ENTRIES) return;

    ab_entry_t *e = &g_entries[g_entry_count++];
    memset(e, 0, sizeof(*e));
    strncpy(e->full_path, full_path, sizeof(e->full_path) - 1);
    strncpy(e->display_name, display_name, sizeof(e->display_name) - 1);
    e->is_dir = is_dir;

    font_text_image_t txt = font_render_text(display_name, FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE);
    if (txt.pixels != NULL) {
        e->label_tex = window_create_texture(txt.pixels, txt.width, txt.height);
        e->label_w = txt.width;
        e->label_h = txt.height;
        font_free_text_image(&txt);
    }

    if (!is_dir) {
        const char *dot = strrchr(display_name, '.');
        if (dot != NULL && file_icons_is_raster_image_ext(dot + 1)) {
            struct stat st;
            if (stat(full_path, &st) == 0 && st.st_size > 0 && st.st_size <= THUMB_MAX_FILE_SIZE) {
                int w, h;
                /* raw_width/height intentionally 0 here - the browser displays general files
                 * without knowledge of their intended dimensions (unlike the Sprite2D node
                 * which has specific Raw Width/Height properties) - safely returns
                 * NULL for .raw files (they are shown with the default icon instead,
                 * the only possible behavior without external size information) */
                unsigned char *pixels = image_loader_editor_load(full_path, &w, &h, 0, 0, 0);
                if (pixels != NULL) {
                    e->thumb_tex = make_thumbnail_texture(pixels, w, h, GRID_ICON_SIZE, &e->thumb_w, &e->thumb_h);
                    free(pixels);
                }
                /* pixels == NULL means the image file is corrupt despite having the correct extension -
                 * thumb_tex remains NULL, so the default image icon is drawn instead
                 * during rendering (exactly the desired behavior) */
            }
        }
    }
}



static void free_crumb_textures(void) {
    for (int i = 0; i < g_crumb_count; i++) {
        if (g_crumb_tex[i] != NULL) { window_destroy_texture(g_crumb_tex[i]); g_crumb_tex[i] = NULL; }
    }
}

static void rebuild_breadcrumb(void) {
    free_crumb_textures();
    g_crumb_count = 0;

    strncpy(g_crumb_label[0], g_root_label, sizeof(g_crumb_label[0]) - 1);
    g_crumb_label[0][sizeof(g_crumb_label[0]) - 1] = '\0';
    strncpy(g_crumb_path[0], g_root_path, sizeof(g_crumb_path[0]) - 1);
    g_crumb_path[0][sizeof(g_crumb_path[0]) - 1] = '\0';
    g_crumb_count = 1;

    size_t root_len = strlen(g_root_path);
    if (strncmp(g_current_path, g_root_path, root_len) == 0
        && strlen(g_current_path) > root_len) {
        char rel_copy[AB_MAX_PATH];
        strncpy(rel_copy, g_current_path + root_len, sizeof(rel_copy) - 1);
        rel_copy[sizeof(rel_copy) - 1] = '\0';

        char accum[AB_MAX_PATH];
        strncpy(accum, g_root_path, sizeof(accum) - 1);
        accum[sizeof(accum) - 1] = '\0';

        char *save = NULL;
        char *tok = strtok_r(rel_copy, "/", &save);
        while (tok != NULL && g_crumb_count < AB_MAX_CRUMBS) {
            size_t len = strlen(accum);
            snprintf(accum + len, sizeof(accum) - len, "/%s", tok);

            strncpy(g_crumb_label[g_crumb_count], tok, sizeof(g_crumb_label[0]) - 1);
            g_crumb_label[g_crumb_count][sizeof(g_crumb_label[0]) - 1] = '\0';
            strncpy(g_crumb_path[g_crumb_count], accum, sizeof(g_crumb_path[0]) - 1);
            g_crumb_path[g_crumb_count][sizeof(g_crumb_path[0]) - 1] = '\0';
            g_crumb_count++;

            tok = strtok_r(NULL, "/", &save);
        }
    }

    for (int i = 0; i < g_crumb_count; i++) {
        g_crumb_tex[i] = ui_make_text_texture(g_crumb_label[i], FONT_WEIGHT_REGULAR, 13,
                                            &g_crumb_w[i], &g_crumb_h[i]);
    }
}

static void load_directory(const char *path) {
    free_entries();
    g_selected_index = -1;
    g_scroll_offset = 0;

    DIR *d = opendir(path);
    if (d == NULL) {
        strncpy(g_current_path, path, sizeof(g_current_path) - 1);
        g_current_path[sizeof(g_current_path) - 1] = '\0';
        rebuild_breadcrumb();
        return;
    }

    char *dir_names[AB_MAX_ENTRIES];
    char *file_names[AB_MAX_ENTRIES];
    int dir_count = 0, file_count = 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue; /* hides hidden files and . and .. */

        char full[AB_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (dir_count < AB_MAX_ENTRIES) dir_names[dir_count++] = strdup(entry->d_name);
        } else {
            if (g_pick_mode == FILE_MANAGER_MODE_PICK_FILE
                && !extension_matches_filter(entry->d_name)) {
                continue;
            }
            if (file_count < AB_MAX_ENTRIES) file_names[file_count++] = strdup(entry->d_name);
        }
    }
    closedir(d);

    qsort(dir_names, dir_count, sizeof(char *), path_utils_compare_names_ci);
    qsort(file_names, file_count, sizeof(char *), path_utils_compare_names_ci);

    /* Directories always first, then files - each group alphabetically, case-insensitive */
    for (int i = 0; i < dir_count; i++) {
        char full[AB_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", path, dir_names[i]);
        add_entry(full, dir_names[i], 1);
        free(dir_names[i]);
    }
    for (int i = 0; i < file_count; i++) {
        char full[AB_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", path, file_names[i]);
        add_entry(full, file_names[i], 0);
        free(file_names[i]);
    }

    strncpy(g_current_path, path, sizeof(g_current_path) - 1);
    g_current_path[sizeof(g_current_path) - 1] = '\0';
    rebuild_breadcrumb();
}

/* ------------------------------------------------------------
 * Lazy initialization (only once, on first actual call)
 * ------------------------------------------------------------ */
static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1;

    font_init();
    icon_atlas_init();
    text_field_init(&g_search_field);
    text_field_init(&g_filename_field);
    g_filename_label_tex = ui_make_text_texture("File name:", FONT_WEIGHT_REGULAR, 13,
                                              &g_filename_label_w, &g_filename_label_h);

    ui_make_blue_button("Cancel", 15, &g_cancel_btn, &g_cancel_btn_tex, &g_cancel_txt_tex);
    ui_make_blue_button("Select", 15, &g_select_btn, &g_select_btn_tex, &g_select_txt_tex);

    g_crumb_sep_tex = ui_make_text_texture("/", FONT_WEIGHT_REGULAR, 13, &g_crumb_sep_w, &g_crumb_sep_h);

    g_hover_tex = ui_selection_make_hover(CELL_W - 4, CELL_H - 4, CELL_CORNER_RADIUS);
    g_selected_tex = ui_selection_make_selected(CELL_W - 4, CELL_H - 4, CELL_CORNER_RADIUS);

}

/* ------------------------------------------------------------
 * Open the window (public interface)
 * ------------------------------------------------------------ */
/* Shared part between file_manager_open and file_manager_open_save -
 * set up the root (Assets:// or the device), load the first folder and clear
 * the search bar. Returns 0 if setup failed (no open project for Assets - the callback
 * is already called with NULL inside; the caller should stop immediately then with no
 * further action), and 1 if it succeeded (the window is actually open) */
static int open_common(file_manager_root_t root, file_manager_callback_t callback, void *user_data) {
    ensure_ready();

    g_root_mode = root;
    g_callback = callback;
    g_user_data = user_data;

    if (root == FILE_MANAGER_ROOT_ASSETS) {
        const char *project_path = current_project_get_path();
        if (project_path == NULL) {
            /* No project open - no Assets:// to browse at all,
             * notify the caller immediately instead of opening a window on a fake folder */
            if (callback != NULL) callback(NULL, user_data);
            return 0;
        }
        snprintf(g_root_path, sizeof(g_root_path), "%s/Assets", project_path);
        struct stat st;
        if (stat(g_root_path, &st) != 0) REBAX_MKDIR(g_root_path, 0755);
        strncpy(g_root_label, "Assets://", sizeof(g_root_label) - 1);
        g_root_label[sizeof(g_root_label) - 1] = '\0';
    } else {
        /* The real root (the upper browsing limit) is always "/" with no restrictions -
         * but the actual starting point is chosen smartly depending on the device, instead of
         * opening the user directly at the practically empty system root: if
         * the device is Android (running inside Termux:X11 as the engine currently is) the
         * storage actually available to the user is at /storage/emulated/0 rather than "/" - we
         * check if it exists and start from it if present, otherwise $HOME, otherwise fall back to the root itself. The user
         * can browse to any other location on the device afterwards normally via the path bar
         * (the path start button "/" is always available, with no real lock) */
        strncpy(g_root_path, "/", sizeof(g_root_path) - 1);
        g_root_path[sizeof(g_root_path) - 1] = '\0';
        strncpy(g_root_label, "/", sizeof(g_root_label) - 1);
        g_root_label[sizeof(g_root_label) - 1] = '\0';
    }

    g_search_field.text[0] = '\0';
    g_search_field.cursor_pos = 0;
    g_search_field.scroll_offset = 0;

    if (root == FILE_MANAGER_ROOT_DEVICE) {
        struct stat st;
        const char *home = getenv("HOME");
        if (stat("/storage/emulated/0", &st) == 0 && S_ISDIR(st.st_mode)) {
            load_directory("/storage/emulated/0");
        } else if (home != NULL && stat(home, &st) == 0 && S_ISDIR(st.st_mode)) {
            load_directory(home);
        } else {
            load_directory(g_root_path);
        }
    } else {
        load_directory(g_root_path);
    }

    g_is_open = 1;
    return 1;
}

void file_manager_open(file_manager_root_t root, file_manager_mode_t mode,
                         const char **extensions, int extension_count,
                         file_manager_callback_t callback, void *user_data) {
    g_pick_mode = mode;

    g_extension_count = 0;
    if (mode == FILE_MANAGER_MODE_PICK_FILE) {
        for (int i = 0; i < extension_count && i < AB_MAX_EXTS; i++) {
            strncpy(g_extensions[g_extension_count], extensions[i], sizeof(g_extensions[0]) - 1);
            g_extensions[g_extension_count][sizeof(g_extensions[0]) - 1] = '\0';
            g_extension_count++;
        }
    }

    if (!open_common(root, callback, user_data)) return;

    if (g_title_tex != NULL) { window_destroy_texture(g_title_tex); g_title_tex = NULL; }
    g_title_tex = ui_make_text_texture(mode == FILE_MANAGER_MODE_PICK_FOLDER ? "Select Folder" : "Select File",
                                     FONT_WEIGHT_BOLD, 17, &g_title_w, &g_title_h);

    if (g_select_btn_tex != NULL) { window_destroy_texture(g_select_btn_tex); g_select_btn_tex = NULL; }
    if (g_select_txt_tex != NULL) { window_destroy_texture(g_select_txt_tex); g_select_txt_tex = NULL; }
    ui_make_blue_button("Select", 15, &g_select_btn, &g_select_btn_tex, &g_select_txt_tex);

    if (g_filter_label_tex != NULL) { window_destroy_texture(g_filter_label_tex); g_filter_label_tex = NULL; }
    if (g_extension_count > 0) {
        char joined[160] = "";
        for (int i = 0; i < g_extension_count; i++) {
            char part[32];
            snprintf(part, sizeof(part), "%s*.%s", (i > 0) ? "  " : "", g_extensions[i]);
            strncat(joined, part, sizeof(joined) - strlen(joined) - 1);
        }
        g_filter_label_tex = ui_make_text_texture(joined, FONT_WEIGHT_REGULAR, 12,
                                                &g_filter_label_w, &g_filter_label_h);
    }
}

void file_manager_open_save(file_manager_root_t root, const char *default_filename,
                              file_manager_callback_t callback, void *user_data) {
    g_pick_mode = FILE_MANAGER_MODE_SAVE_FILE;
    g_extension_count = 0; /* No filtering - all files are shown to the context only, exactly like PICK_FOLDER */

    if (!open_common(root, callback, user_data)) return;

    if (g_title_tex != NULL) { window_destroy_texture(g_title_tex); g_title_tex = NULL; }
    g_title_tex = ui_make_text_texture("Save As", FONT_WEIGHT_BOLD, 17, &g_title_w, &g_title_h);

    if (g_select_btn_tex != NULL) { window_destroy_texture(g_select_btn_tex); g_select_btn_tex = NULL; }
    if (g_select_txt_tex != NULL) { window_destroy_texture(g_select_txt_tex); g_select_txt_tex = NULL; }
    ui_make_blue_button("Save", 15, &g_select_btn, &g_select_btn_tex, &g_select_txt_tex);

    if (g_filter_label_tex != NULL) { window_destroy_texture(g_filter_label_tex); g_filter_label_tex = NULL; }

    g_filename_field.text[0] = '\0';
    if (default_filename != NULL) {
        strncpy(g_filename_field.text, default_filename, TEXT_FIELD_MAX_LEN - 1);
        g_filename_field.text[TEXT_FIELD_MAX_LEN - 1] = '\0';
    }
    g_filename_field.cursor_pos = (int)strlen(g_filename_field.text);
    g_filename_field.scroll_offset = 0;
}

int file_manager_is_open(void) { return g_is_open; }

/* ------------------------------------------------------------
 * Update (logic + interaction)
 * ------------------------------------------------------------ */
static void finish(const char *picked_path) {
    file_manager_callback_t cb = g_callback;
    void *ud = g_user_data;
    g_is_open = 0;
    g_callback = NULL;
    g_user_data = NULL;
    window_stop_text_input();
    if (cb != NULL) cb(picked_path, ud);
}

void file_manager_update(int window_w, int window_h) {
    if (!g_is_open) return;

    int dx = (window_w - DIALOG_WIDTH) / 2;
    int dy = (window_h - DIALOG_HEIGHT) / 2;

    int mx = window_mouse_x(), my = window_mouse_y();

    /* --- Title bar + close button --- */
    int close_x = dx + DIALOG_WIDTH - DIALOG_PADDING - CLOSE_BTN_SIZE;
    int close_y = dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2;
    if (window_mouse_left_just_pressed()
        && mx >= close_x && mx < close_x + CLOSE_BTN_SIZE
        && my >= close_y && my < close_y + CLOSE_BTN_SIZE) {
        finish(NULL);
        return;
    }

    /* --- Breadcrumb bar - counted here, reused during drawing --- */
    int crumb_y = dy + TITLEBAR_HEIGHT;
    int crumb_x = dx + DIALOG_PADDING;
    for (int i = 0; i < g_crumb_count; i++) {
        g_crumb_x[i] = crumb_x;
        g_crumb_w_click[i] = g_crumb_w[i];
        crumb_x += g_crumb_w[i];
        if (i < g_crumb_count - 1) crumb_x += g_crumb_sep_w + 10;
    }
    if (window_mouse_left_just_pressed() && my >= crumb_y && my < crumb_y + CRUMB_BAR_HEIGHT) {
        for (int i = 0; i < g_crumb_count - 1; i++) { /* The last item is the current folder itself - no need to compress it */
            if (mx >= g_crumb_x[i] && mx < g_crumb_x[i] + g_crumb_w_click[i]) {
                load_directory(g_crumb_path[i]);
                break;
            }
        }
    }

    /* --- Search bar --- */
    int toolbar_y = crumb_y + CRUMB_BAR_HEIGHT;
    int search_x = dx + DIALOG_PADDING;
    int search_w = (DIALOG_WIDTH - DIALOG_PADDING * 2) / 2;
    text_field_update(&g_search_field, search_x + 20, toolbar_y, search_w - 20, TOOLBAR_HEIGHT - 6, search_w - 20);

    if (g_search_field.focused) window_start_text_input(); else window_stop_text_input();

    /* --- Grid area --- */
    int grid_x = dx + DIALOG_PADDING;
    int grid_y = toolbar_y + TOOLBAR_HEIGHT + GRID_TOP_GAP;
    int grid_w = DIALOG_WIDTH - DIALOG_PADDING * 2;
    int filename_row_reserve = (g_pick_mode == FILE_MANAGER_MODE_SAVE_FILE) ? FILENAME_ROW_HEIGHT : 0;
    int grid_h = DIALOG_HEIGHT - (grid_y - dy) - FOOTER_HEIGHT - DIALOG_PADDING - filename_row_reserve;

    int columns = grid_w / CELL_W;
    if (columns < 1) columns = 1;

    int visible_count = 0;
    for (int i = 0; i < g_entry_count; i++) {
        if (ui_text_contains_ci(g_entries[i].display_name, g_search_field.text)) visible_count++;
    }
    int total_rows = (visible_count + columns - 1) / columns;
    int content_h = total_rows * CELL_H;
    int max_scroll = content_h - grid_h;
    if (max_scroll < 0) max_scroll = 0;

    if (mx >= grid_x && mx < grid_x + grid_w && my >= grid_y && my < grid_y + grid_h) {
        g_scroll_offset -= window_mouse_wheel_delta() * SCROLL_STEP_PX;
    }
    if (g_scroll_offset < 0) g_scroll_offset = 0;
    if (g_scroll_offset > max_scroll) g_scroll_offset = max_scroll;

    int visible_index = 0;
    int clicked_grid = 0;
    for (int i = 0; i < g_entry_count && !clicked_grid; i++) {
        ab_entry_t *e = &g_entries[i];
        if (!ui_text_contains_ci(e->display_name, g_search_field.text)) continue;

        int col = visible_index % columns;
        int row = visible_index / columns;
        int cell_x = grid_x + col * CELL_W;
        int cell_y = grid_y + row * CELL_H - g_scroll_offset;
        visible_index++;

        if (cell_y + CELL_H < grid_y || cell_y > grid_y + grid_h) continue; /* Outside the view area - no need to check for clicks */

        int inside = (mx >= cell_x && mx < cell_x + CELL_W && my >= cell_y && my < cell_y + CELL_H
                      && my >= grid_y && my < grid_y + grid_h);
        if (inside && window_mouse_left_just_pressed()) {
            if (e->is_dir) {
                /* e points into g_entries; load_directory() clears and rebuilds
                 * that array, so copy the path before calling it. */
                char next_path[AB_MAX_PATH];
                strncpy(next_path, e->full_path, sizeof(next_path) - 1);
                next_path[sizeof(next_path) - 1] = '\0';
                g_search_field.text[0] = '\0';
                g_search_field.cursor_pos = 0;
                g_search_field.scroll_offset = 0;
                load_directory(next_path);
                clicked_grid = 1;
            } else if (g_pick_mode == FILE_MANAGER_MODE_PICK_FILE) {
                g_selected_index = i;
                clicked_grid = 1;
            }
            /* In PICK_FOLDER mode: clicking a file does nothing - files
             * are shown for context only, not selectable in this mode */
        }
    }

    /* --- Current final selection (to enable/disable the Select/Save button) --- */
    int can_select = 0;
    char final_path[AB_MAX_PATH];
    final_path[0] = '\0';
    if (g_pick_mode == FILE_MANAGER_MODE_SAVE_FILE) {
        /* File name row - directly below the grid, above the button row */
        int buttons_y_early = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;
        int filename_row_y = buttons_y_early - FILENAME_ROW_HEIGHT;
        int field_label_w = 70; /* Approximate width sufficient for the "File name:" label in font 13 */
        int field_x = grid_x + field_label_w;
        int field_y = filename_row_y + (FILENAME_ROW_HEIGHT - 22) / 2;
        int field_w = grid_w - field_label_w;

        int was_focused = g_filename_field.focused;
        text_field_update(&g_filename_field, field_x, field_y, field_w, 22, field_w);
        if (g_filename_field.focused != was_focused) {
            if (g_filename_field.focused) window_start_text_input(); else window_stop_text_input();
        }

        if (g_filename_field.text[0] != '\0') {
            can_select = 1;
            int written = snprintf(final_path, sizeof(final_path), "%s/%s", g_current_path, g_filename_field.text);
            if (written < 0 || written >= (int)sizeof(final_path)) {
                final_path[sizeof(final_path) - 1] = '\0'; /* Safe truncation - snprintf already guarantees this; documents awareness of the case to silence GCC's defensive warning */
            }
        }
    } else if (g_pick_mode == FILE_MANAGER_MODE_PICK_FOLDER) {
        can_select = 1;
        strncpy(final_path, g_current_path, sizeof(final_path) - 1);
    } else if (g_selected_index >= 0 && g_selected_index < g_entry_count
               && !g_entries[g_selected_index].is_dir) {
        can_select = 1;
        strncpy(final_path, g_entries[g_selected_index].full_path, sizeof(final_path) - 1);
    }

    /* --- Cancel / Select buttons --- */
    int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;
    int select_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_select_btn.width;
    int cancel_x = select_x - 8 - g_cancel_btn.width;

    if (window_mouse_left_just_pressed()) {
        if (mx >= cancel_x && mx < cancel_x + g_cancel_btn.width
            && my >= buttons_y && my < buttons_y + g_cancel_btn.height) {
            finish(NULL);
            return;
        }
        if (can_select && mx >= select_x && mx < select_x + g_select_btn.width
            && my >= buttons_y && my < buttons_y + g_select_btn.height) {
            finish(final_path);
            return;
        }
    }
}

/* ------------------------------------------------------------
 * Drawing
 * ------------------------------------------------------------ */
void file_manager_draw(int window_w, int window_h) {
    if (!g_is_open) return;

    int dx = (window_w - DIALOG_WIDTH) / 2;
    int dy = (window_h - DIALOG_HEIGHT) / 2;

    ui_dialog_draw_backdrop(window_w, window_h);

    ui_dialog_draw_frame(dx, dy, DIALOG_WIDTH, DIALOG_HEIGHT);

    /* --- Title bar --- */
    ui_dialog_draw_title_left(g_title_tex, g_title_w, g_title_h, dx, dy, TITLEBAR_HEIGHT, DIALOG_PADDING);
    ui_dialog_draw_close_button(dx + DIALOG_WIDTH - DIALOG_PADDING - CLOSE_BTN_SIZE,
                                dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2, CLOSE_BTN_SIZE);

    window_fill_rect(dx + DIALOG_PADDING, dy + TITLEBAR_HEIGHT, DIALOG_WIDTH - DIALOG_PADDING * 2, 1, 60, 60, 60);

    /* --- Path bar --- */
    int crumb_y = dy + TITLEBAR_HEIGHT;
    for (int i = 0; i < g_crumb_count; i++) {
        int is_last = (i == g_crumb_count - 1);
        unsigned char cr = is_last ? 235 : 150, cg = is_last ? 235 : 150, cb = is_last ? 235 : 150;
        if (g_crumb_tex[i] != NULL) {
            window_draw_texture_tinted(g_crumb_tex[i], g_crumb_x[i], crumb_y + (CRUMB_BAR_HEIGHT - g_crumb_h[i]) / 2,
                                        g_crumb_w[i], g_crumb_h[i], cr, cg, cb);
        }
        if (!is_last && g_crumb_sep_tex != NULL) {
            window_draw_texture(g_crumb_sep_tex, g_crumb_x[i] + g_crumb_w[i] + 4,
                                 crumb_y + (CRUMB_BAR_HEIGHT - g_crumb_sep_h) / 2, g_crumb_sep_w, g_crumb_sep_h);
        }
    }

    /* --- Search bar + results/filter counter --- */
    int toolbar_y = crumb_y + CRUMB_BAR_HEIGHT;
    int search_x = dx + DIALOG_PADDING;
    int search_w = (DIALOG_WIDTH - DIALOG_PADDING * 2) / 2;
    icon_atlas_draw(ICON_search, search_x, toolbar_y + (TOOLBAR_HEIGHT - 6 - 16) / 2, 16);
    text_field_draw(&g_search_field, search_x + 20, toolbar_y, search_w - 20, TOOLBAR_HEIGHT - 6, 13);

    if (g_filter_label_tex != NULL) {
        int fx = dx + DIALOG_WIDTH - DIALOG_PADDING - g_filter_label_w;
        window_draw_texture_tinted(g_filter_label_tex, fx, toolbar_y + (TOOLBAR_HEIGHT - g_filter_label_h) / 2,
                                    g_filter_label_w, g_filter_label_h, 150, 150, 150);
    }

    /* --- Grid area (clipped at its edges so no content overflows) --- */
    int grid_x = dx + DIALOG_PADDING;
    int grid_y = toolbar_y + TOOLBAR_HEIGHT + GRID_TOP_GAP;
    int grid_w = DIALOG_WIDTH - DIALOG_PADDING * 2;
    int filename_row_reserve = (g_pick_mode == FILE_MANAGER_MODE_SAVE_FILE) ? FILENAME_ROW_HEIGHT : 0;
    int grid_h = DIALOG_HEIGHT - (grid_y - dy) - FOOTER_HEIGHT - DIALOG_PADDING - filename_row_reserve;

    int columns = grid_w / CELL_W;
    if (columns < 1) columns = 1;

    window_set_clip_rect(grid_x, grid_y, grid_w, grid_h);

    int mx = window_mouse_x(), my = window_mouse_y();
    int visible_index = 0;
    for (int i = 0; i < g_entry_count; i++) {
        ab_entry_t *e = &g_entries[i];
        if (!ui_text_contains_ci(e->display_name, g_search_field.text)) continue;

        int col = visible_index % columns;
        int row = visible_index / columns;
        int cell_x = grid_x + col * CELL_W;
        int cell_y = grid_y + row * CELL_H - g_scroll_offset;
        visible_index++;

        if (cell_y + CELL_H < grid_y || cell_y > grid_y + grid_h) continue;

        int is_dim = (!e->is_dir && g_pick_mode == FILE_MANAGER_MODE_PICK_FOLDER);
        int is_hover = (mx >= cell_x && mx < cell_x + CELL_W && my >= cell_y && my < cell_y + CELL_H
                         && my >= grid_y && my < grid_y + grid_h);
        int is_selected = (!e->is_dir && i == g_selected_index);

        if (is_selected && g_selected_tex != NULL) {
            window_draw_texture(g_selected_tex, cell_x + 2, cell_y + 2, CELL_W - 4, CELL_H - 4);
        } else if (is_hover && !is_dim && g_hover_tex != NULL) {
            window_draw_texture(g_hover_tex, cell_x + 2, cell_y + 2, CELL_W - 4, CELL_H - 4);
        }

        int icon_area_x = cell_x + (CELL_W - GRID_ICON_SIZE) / 2;
        int icon_area_y = cell_y + 8;
        unsigned char tint = is_dim ? 100 : 255;

        if (e->is_dir) {
            icon_atlas_draw_tinted(ICON_Folder, icon_area_x, icon_area_y, GRID_ICON_SIZE, 230, 200, 60);
        } else if (e->thumb_tex != NULL) {
            /* Real thumbnail image - drawn centered in the icon square preserving its aspect ratio */
            int tw = e->thumb_w, th = e->thumb_h;
            int tx = icon_area_x + (GRID_ICON_SIZE - tw) / 2;
            int ty = icon_area_y + (GRID_ICON_SIZE - th) / 2;
            if (is_dim) {
                window_draw_texture_tinted(e->thumb_tex, tx, ty, tw, th, tint, tint, tint);
            } else {
                window_draw_texture(e->thumb_tex, tx, ty, tw, th);
            }
        } else {
            int icon_id = file_icons_for_name(e->display_name);
            if (is_dim) {
                icon_atlas_draw_tinted(icon_id, icon_area_x, icon_area_y, GRID_ICON_SIZE, tint, tint, tint);
            } else {
                icon_atlas_draw(icon_id, icon_area_x, icon_area_y, GRID_ICON_SIZE);
            }
        }

        if (e->label_tex != NULL) {
            int label_x = cell_x + (CELL_W - e->label_w) / 2;
            int label_y = icon_area_y + GRID_ICON_SIZE + 6;
            window_set_clip_rect(cell_x + 2, label_y, CELL_W - 4, LABEL_FONT_SIZE + 4);
            if (is_dim) {
                window_draw_texture_tinted(e->label_tex, label_x, label_y, e->label_w, e->label_h, tint, tint, tint);
            } else {
                window_draw_texture(e->label_tex, label_x, label_y, e->label_w, e->label_h);
            }
            window_set_clip_rect(grid_x, grid_y, grid_w, grid_h); /* Restore full grid clipping after clipping the subname */
        }
    }

    window_clear_clip_rect();

    /* --- Footer: Cancel / Select / Save buttons --- */
    int can_select = 0;
    if (g_pick_mode == FILE_MANAGER_MODE_SAVE_FILE) {
        can_select = (g_filename_field.text[0] != '\0');
    } else if (g_pick_mode == FILE_MANAGER_MODE_PICK_FOLDER) {
        can_select = 1;
    } else if (g_selected_index >= 0 && g_selected_index < g_entry_count
               && !g_entries[g_selected_index].is_dir) {
        can_select = 1;
    }

    int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;

    if (g_pick_mode == FILE_MANAGER_MODE_SAVE_FILE) {
        int filename_row_y = buttons_y - FILENAME_ROW_HEIGHT;
        int field_label_w = 70;
        int label_x = grid_x;
        int label_y = filename_row_y + (FILENAME_ROW_HEIGHT - g_filename_label_h) / 2;
        if (g_filename_label_tex != NULL) {
            window_draw_texture(g_filename_label_tex, label_x, label_y, g_filename_label_w, g_filename_label_h);
        }
        int field_x = grid_x + field_label_w;
        int field_y = filename_row_y + (FILENAME_ROW_HEIGHT - 22) / 2;
        int field_w = grid_w - field_label_w;
        text_field_draw(&g_filename_field, field_x, field_y, field_w, 22, 13);
    }

    int select_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_select_btn.width;
    int cancel_x = select_x - 8 - g_cancel_btn.width;

    window_draw_texture(g_cancel_btn_tex, cancel_x, buttons_y, g_cancel_btn.width, g_cancel_btn.height);
    window_draw_texture(g_cancel_txt_tex, cancel_x + g_cancel_btn.text_offset_x, buttons_y + g_cancel_btn.text_offset_y,
                         g_cancel_btn.text_img.width, g_cancel_btn.text_img.height);

    if (can_select) {
        window_draw_texture(g_select_btn_tex, select_x, buttons_y, g_select_btn.width, g_select_btn.height);
        window_draw_texture(g_select_txt_tex, select_x + g_select_btn.text_offset_x, buttons_y + g_select_btn.text_offset_y,
                             g_select_btn.text_img.width, g_select_btn.text_img.height);
    } else {
        /* Currently disabled - the same button but tinted light gray instead of generating
         * a second custom texture for the disabled state (window_draw_texture_tinted
         * is perfectly sufficient for this purpose) */
        window_draw_texture_tinted(g_select_btn_tex, select_x, buttons_y, g_select_btn.width, g_select_btn.height,
                                    90, 90, 90);
        window_draw_texture_tinted(g_select_txt_tex, select_x + g_select_btn.text_offset_x, buttons_y + g_select_btn.text_offset_y,
                                    g_select_btn.text_img.width, g_select_btn.text_img.height, 150, 150, 150);
    }
}

void file_manager_shutdown(void) {
    free_entries();
    free_crumb_textures();

    if (g_title_tex != NULL) window_destroy_texture(g_title_tex);
    if (g_filter_label_tex != NULL) window_destroy_texture(g_filter_label_tex);
    if (g_crumb_sep_tex != NULL) window_destroy_texture(g_crumb_sep_tex);
    if (g_hover_tex != NULL) window_destroy_texture(g_hover_tex);
    if (g_selected_tex != NULL) window_destroy_texture(g_selected_tex);
    if (g_cancel_btn_tex != NULL) window_destroy_texture(g_cancel_btn_tex);
    if (g_cancel_txt_tex != NULL) window_destroy_texture(g_cancel_txt_tex);
    if (g_select_btn_tex != NULL) window_destroy_texture(g_select_btn_tex);
    if (g_select_txt_tex != NULL) window_destroy_texture(g_select_txt_tex);
    if (g_filename_label_tex != NULL) window_destroy_texture(g_filename_label_tex);

    text_field_free(&g_search_field);
    text_field_free(&g_filename_field);
    labeled_button_free(&g_cancel_btn);
    labeled_button_free(&g_select_btn);

    g_title_tex = NULL;
    g_filter_label_tex = NULL;
    g_crumb_sep_tex = NULL;
    g_hover_tex = NULL;
    g_selected_tex = NULL;

    g_is_open = 0;
    g_ready = 0;
}
