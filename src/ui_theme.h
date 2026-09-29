/* ui_theme.h - ألوان وثوابت المظهر المشتركة لكل واجهات المحرك. */
#ifndef UI_THEME_H
#define UI_THEME_H

typedef struct {
    unsigned char r, g, b, a;
} ui_color_t;

static const ui_color_t UI_COLOR_BLACK_MUTED = {0x13, 0x13, 0x13, 0xFF};
static const ui_color_t UI_COLOR_GRAY_MUTED = {0x42, 0x42, 0x42, 0xFF};
static const ui_color_t UI_COLOR_BUTTON_BLUE = {0x3B, 0x5B, 0x74, 0xFF};
static const ui_color_t UI_COLOR_VIEWPORT_BLACK = {0x00, 0x00, 0x00, 0xFF};
static const ui_color_t UI_COLOR_PANEL_BG = {0x38, 0x38, 0x38, 0xFF};
static const ui_color_t UI_COLOR_HOVER_BLUE = {0x2C, 0x40, 0x50, 0xFF};
static const ui_color_t UI_COLOR_TAB_ACTIVE = {0x29, 0x3B, 0x48, 0xFF};
static const ui_color_t UI_COLOR_TAB_HOVER = {0x1D, 0x25, 0x2B, 0xFF};

#endif /* UI_THEME_H */
