/*
 * ============================================================
 * window.h
 * ============================================================
 * مكتبة النافذة: تغلّف تفاصيل SDL2 وتوفر دوال بسيطة لفتح نافذة،
 * الرسم فيها، والتقاط أحداث الإغلاق. أي مكتبة أخرى تحتاج ترسم
 * على الشاشة تستخدم هذه الدوال، بدون ما تتعامل مع SDL2 مباشرة.
 * ============================================================
 */

#ifndef WINDOW_H
#define WINDOW_H

int window_init(const char *title, int width, int height);
int window_should_close(void);
void window_poll_events(void);
void window_clear(unsigned char r, unsigned char g, unsigned char b);
void window_fill_rect(int x, int y, int w, int h,
                       unsigned char r, unsigned char g, unsigned char b);

typedef struct window_texture window_texture_t;
window_texture_t *window_create_texture(const unsigned char *rgba_pixels,
                                         int width, int height);
void window_draw_texture(window_texture_t *tex, int x, int y, int w, int h);
void window_draw_texture_tinted(window_texture_t *tex, int x, int y, int w, int h,
                                 unsigned char r, unsigned char g, unsigned char b);
void window_draw_texture_region(window_texture_t *tex,
                                 int src_x, int src_y, int src_w, int src_h,
                                 int dst_x, int dst_y, int dst_w, int dst_h);
void window_draw_texture_region_tinted(window_texture_t *tex,
                                        int src_x, int src_y, int src_w, int src_h,
                                        int dst_x, int dst_y, int dst_w, int dst_h,
                                        unsigned char r, unsigned char g, unsigned char b);
void window_draw_texture_region_rotated(window_texture_t *tex,
                                         int src_x, int src_y, int src_w, int src_h,
                                         int dst_x, int dst_y, int dst_w, int dst_h,
                                         double angle_degrees);
void window_destroy_texture(window_texture_t *tex);
void window_present(void);
int window_get_width(void);
int window_get_height(void);

int window_mouse_x(void);
int window_mouse_y(void);
int window_mouse_left_just_pressed(void);
int window_mouse_left_down(void);
int window_mouse_right_just_pressed(void);
int window_mouse_right_down(void);
int window_mouse_middle_just_pressed(void);
int window_mouse_middle_down(void);
int window_mouse_wheel_delta(void);
int window_mouse_delta_x(void);
int window_mouse_delta_y(void);
void window_set_relative_mouse_mode(int enabled);
float window_get_delta_time(void);

void window_start_text_input(void);
void window_stop_text_input(void);
const char *window_text_input_this_frame(void);
int window_key_just_pressed_backspace(void);
int window_key_just_pressed_delete(void);
int window_key_just_pressed_left(void);
int window_key_just_pressed_right(void);
int window_key_just_pressed_home(void);
int window_key_just_pressed_end(void);
int window_key_just_pressed_copy(void);
int window_key_just_pressed_paste(void);
int window_key_just_pressed_cut(void);
int window_key_just_pressed_enter(void);
int window_key_just_pressed_up(void);
int window_key_just_pressed_down(void);
int window_key_just_pressed_tab(void);
int window_key_just_pressed_save(void);

int window_key_down_w(void);
int window_key_down_a(void);
int window_key_down_s(void);
int window_key_down_d(void);
int window_key_down_q(void);
int window_key_down_e(void);
int window_key_down_shift(void);
int window_key_just_pressed_f(void);

char *window_get_clipboard_text(void);
void window_set_clipboard_text(const char *text);
void window_set_clip_rect(int x, int y, int w, int h);
void window_clear_clip_rect(void);
void window_shutdown(void);

#endif /* WINDOW_H */
