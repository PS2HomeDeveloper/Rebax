/*
 * ============================================================
 * window.c
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "window.h"

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static int g_should_close = 0;
static int g_width  = 0;
static int g_height = 0;

/* حالة الماوس */
static int g_mouse_x = 0;
static int g_mouse_y = 0;
static int g_mouse_left_just_pressed = 0;
static int g_mouse_left_down = 0; /* يبقى 1 طول مدة الضغط، مو لحظة واحدة بس */
static int g_mouse_right_just_pressed = 0;
static int g_mouse_right_down = 0;
static int g_mouse_middle_just_pressed = 0;
static int g_mouse_middle_down = 0;
static int g_mouse_wheel_delta = 0;
static int g_mouse_delta_x = 0;
static int g_mouse_delta_y = 0;

/* الوقت - لحساب الفرق بين إطارين متتاليين (Delta Time) */
static Uint32 g_last_frame_ticks = 0;
static float g_delta_time = 0.0f;

/* نص الكتابة الوارد هذا الإطار فقط */
static char g_text_input_buffer[64] = "";

/* مفاتيح التحكم - كل واحد 1 فقط في إطار الضغطة الأولى */
static int g_key_backspace = 0;
static int g_key_delete    = 0;
static int g_key_left      = 0;
static int g_key_right     = 0;
static int g_key_home      = 0;
static int g_key_end       = 0;
static int g_key_copy      = 0;
static int g_key_paste     = 0;
static int g_key_cut       = 0;
static int g_key_f         = 0; /* تأطير العقدة المحددة بكاميرا التطوير 3D */

int window_init(const char *title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }

    g_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (g_window == NULL) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    g_renderer = SDL_CreateRenderer(
        g_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (g_renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return 0;
    }

    g_width  = width;
    g_height = height;
    g_should_close = 0;
    g_last_frame_ticks = SDL_GetTicks();
    return 1;
}

int window_should_close(void) {
    return g_should_close;
}

void window_poll_events(void) {
    /* دلتا الوقت - فرق التذكرة الحالية عن آخر إطار، بالثواني */
    Uint32 now_ticks = SDL_GetTicks();
    g_delta_time = (float)(now_ticks - g_last_frame_ticks) / 1000.0f;
    g_last_frame_ticks = now_ticks;

    /* نصفّر كل الحالات "لهذا الإطار فقط" قبل قراءة الأحداث الجديدة */
    g_mouse_left_just_pressed = 0;
    g_mouse_right_just_pressed = 0;
    g_mouse_middle_just_pressed = 0;
    g_mouse_wheel_delta = 0;
    g_mouse_delta_x = 0;
    g_mouse_delta_y = 0;
    g_text_input_buffer[0] = '\0';
    g_key_backspace = g_key_delete = g_key_left = g_key_right = 0;
    g_key_home = g_key_end = g_key_copy = g_key_paste = g_key_cut = 0;
    g_key_f = 0;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_should_close = 1;
        } else if (event.type == SDL_WINDOWEVENT
                   && event.window.event == SDL_WINDOWEVENT_RESIZED) {
            g_width  = event.window.data1;
            g_height = event.window.data2;
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                g_mouse_left_just_pressed = 1;
                g_mouse_left_down = 1;
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                g_mouse_right_just_pressed = 1;
                g_mouse_right_down = 1;
            } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                g_mouse_middle_just_pressed = 1;
                g_mouse_middle_down = 1;
            }
        } else if (event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                g_mouse_left_down = 0;
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                g_mouse_right_down = 0;
            } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                g_mouse_middle_down = 0;
            }
        } else if (event.type == SDL_MOUSEWHEEL) {
            g_mouse_wheel_delta += event.wheel.y;
        } else if (event.type == SDL_MOUSEMOTION) {
            g_mouse_x = event.motion.x;
            g_mouse_y = event.motion.y;
            g_mouse_delta_x += event.motion.xrel;
            g_mouse_delta_y += event.motion.yrel;
        } else if (event.type == SDL_TEXTINPUT) {
            strncat(g_text_input_buffer, event.text.text,
                    sizeof(g_text_input_buffer) - strlen(g_text_input_buffer) - 1);
        } else if (event.type == SDL_KEYDOWN) {
            int ctrl = (event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)) != 0;
            switch (event.key.keysym.sym) {
                case SDLK_BACKSPACE: g_key_backspace = 1; break;
                case SDLK_DELETE:    g_key_delete    = 1; break;
                case SDLK_LEFT:      g_key_left      = 1; break;
                case SDLK_RIGHT:     g_key_right     = 1; break;
                case SDLK_HOME:      g_key_home      = 1; break;
                case SDLK_END:       g_key_end       = 1; break;
                case SDLK_c: if (ctrl) g_key_copy  = 1; break;
                case SDLK_v: if (ctrl) g_key_paste = 1; break;
                case SDLK_x: if (ctrl) g_key_cut   = 1; break;
                case SDLK_f: g_key_f = 1; break;
                default: break;
            }
        }
    }
}

void window_clear(unsigned char r, unsigned char g, unsigned char b) {
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    SDL_RenderClear(g_renderer);
}

void window_fill_rect(int x, int y, int w, int h,
                       unsigned char r, unsigned char g, unsigned char b) {
    SDL_Rect rect = { x, y, w, h };
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    SDL_RenderFillRect(g_renderer, &rect);
}

/* التعريف الحقيقي للنوع المبهم المعلن في window.h - ببساطة يغلّف
 * مؤشر SDL_Texture بدون كشف تفاصيل SDL خارج هذا الملف */
struct window_texture {
    SDL_Texture *sdl_texture;
};

window_texture_t *window_create_texture(const unsigned char *rgba_pixels,
                                         int width, int height) {
    SDL_Texture *sdl_tex = SDL_CreateTexture(
        g_renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC, width, height
    );
    if (sdl_tex == NULL) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_SetTextureBlendMode(sdl_tex, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(sdl_tex, NULL, rgba_pixels, width * 4);

    window_texture_t *tex = (window_texture_t *)malloc(sizeof(window_texture_t));
    tex->sdl_texture = sdl_tex;
    return tex;
}

void window_draw_texture(window_texture_t *tex, int x, int y, int w, int h) {
    if (tex == NULL) return;
    SDL_Rect dst = { x, y, w, h };
    SDL_RenderCopy(g_renderer, tex->sdl_texture, NULL, &dst);
}

void window_draw_texture_tinted(window_texture_t *tex, int x, int y, int w, int h,
                                 unsigned char r, unsigned char g, unsigned char b) {
    if (tex == NULL) return;
    SDL_SetTextureColorMod(tex->sdl_texture, r, g, b);
    SDL_Rect dst = { x, y, w, h };
    SDL_RenderCopy(g_renderer, tex->sdl_texture, NULL, &dst);
    SDL_SetTextureColorMod(tex->sdl_texture, 255, 255, 255); /* رجوع للوضع الطبيعي */
}

void window_draw_texture_region(window_texture_t *tex,
                                 int src_x, int src_y, int src_w, int src_h,
                                 int dst_x, int dst_y, int dst_w, int dst_h) {
    if (tex == NULL) return;
    SDL_Rect src = { src_x, src_y, src_w, src_h };
    SDL_Rect dst = { dst_x, dst_y, dst_w, dst_h };
    SDL_RenderCopy(g_renderer, tex->sdl_texture, &src, &dst);
}

void window_draw_texture_region_tinted(window_texture_t *tex,
                                        int src_x, int src_y, int src_w, int src_h,
                                        int dst_x, int dst_y, int dst_w, int dst_h,
                                        unsigned char r, unsigned char g, unsigned char b) {
    if (tex == NULL) return;
    SDL_SetTextureColorMod(tex->sdl_texture, r, g, b);
    SDL_Rect src = { src_x, src_y, src_w, src_h };
    SDL_Rect dst = { dst_x, dst_y, dst_w, dst_h };
    SDL_RenderCopy(g_renderer, tex->sdl_texture, &src, &dst);
    SDL_SetTextureColorMod(tex->sdl_texture, 255, 255, 255);
}

void window_draw_texture_region_rotated(window_texture_t *tex,
                                         int src_x, int src_y, int src_w, int src_h,
                                         int dst_x, int dst_y, int dst_w, int dst_h,
                                         double angle_degrees) {
    if (tex == NULL) return;
    SDL_Rect src = { src_x, src_y, src_w, src_h };
    SDL_Rect dst = { dst_x, dst_y, dst_w, dst_h };
    /* NULL = يدور حول مركز المستطيل الهدف تلقائياً، بدون قلب */
    SDL_RenderCopyEx(g_renderer, tex->sdl_texture, &src, &dst,
                      angle_degrees, NULL, SDL_FLIP_NONE);
}

void window_destroy_texture(window_texture_t *tex) {
    if (tex == NULL) return;
    if (tex->sdl_texture != NULL) {
        SDL_DestroyTexture(tex->sdl_texture);
    }
    free(tex);
}

void window_present(void) {
    SDL_RenderPresent(g_renderer);
}

int window_get_width(void)  { return g_width; }
int window_get_height(void) { return g_height; }

int window_mouse_x(void) { return g_mouse_x; }
int window_mouse_y(void) { return g_mouse_y; }
int window_mouse_left_just_pressed(void) { return g_mouse_left_just_pressed; }
int window_mouse_left_down(void) { return g_mouse_left_down; }

int window_mouse_right_just_pressed(void) { return g_mouse_right_just_pressed; }
int window_mouse_right_down(void) { return g_mouse_right_down; }
int window_mouse_middle_just_pressed(void) { return g_mouse_middle_just_pressed; }
int window_mouse_middle_down(void) { return g_mouse_middle_down; }

int window_mouse_wheel_delta(void) { return g_mouse_wheel_delta; }

int window_mouse_delta_x(void) { return g_mouse_delta_x; }
int window_mouse_delta_y(void) { return g_mouse_delta_y; }

void window_set_relative_mouse_mode(int enabled) {
    SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE);
}

float window_get_delta_time(void) { return g_delta_time; }

void window_start_text_input(void) { SDL_StartTextInput(); }
void window_stop_text_input(void)  { SDL_StopTextInput(); }

const char *window_text_input_this_frame(void) { return g_text_input_buffer; }

int window_key_just_pressed_backspace(void) { return g_key_backspace; }
int window_key_just_pressed_delete(void)    { return g_key_delete; }
int window_key_just_pressed_left(void)      { return g_key_left; }
int window_key_just_pressed_right(void)     { return g_key_right; }
int window_key_just_pressed_home(void)      { return g_key_home; }
int window_key_just_pressed_end(void)       { return g_key_end; }
int window_key_just_pressed_copy(void)      { return g_key_copy; }
int window_key_just_pressed_paste(void)     { return g_key_paste; }
int window_key_just_pressed_cut(void)       { return g_key_cut; }

/* حالة "مضغوط الآن" لمفاتيح تحكم الكاميرا - تُقرأ مباشرة من جدول
 * حالة لوحة المفاتيح الحي اللي SDL يحدّثه بنفسه (بعكس بقية المفاتيح
 * أعلاه اللي تُلتقط من الأحداث لحظة الضغط بس) - أنسب لحركة مستمرة
 * طول مدة الضغط بدل ضغطة واحدة */
int window_key_down_w(void) { return SDL_GetKeyboardState(NULL)[SDL_SCANCODE_W]; }
int window_key_down_a(void) { return SDL_GetKeyboardState(NULL)[SDL_SCANCODE_A]; }
int window_key_down_s(void) { return SDL_GetKeyboardState(NULL)[SDL_SCANCODE_S]; }
int window_key_down_d(void) { return SDL_GetKeyboardState(NULL)[SDL_SCANCODE_D]; }
int window_key_down_q(void) { return SDL_GetKeyboardState(NULL)[SDL_SCANCODE_Q]; }
int window_key_down_e(void) { return SDL_GetKeyboardState(NULL)[SDL_SCANCODE_E]; }

int window_key_down_shift(void) {
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    return keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
}

int window_key_just_pressed_f(void) { return g_key_f; }

char *window_get_clipboard_text(void) {
    if (!SDL_HasClipboardText()) {
        return NULL;
    }
    char *sdl_text = SDL_GetClipboardText();
    if (sdl_text == NULL) {
        return NULL;
    }
    char *copy = strdup(sdl_text);
    SDL_free(sdl_text);
    return copy;
}

void window_set_clipboard_text(const char *text) {
    SDL_SetClipboardText(text);
}

void window_set_clip_rect(int x, int y, int w, int h) {
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderSetClipRect(g_renderer, &rect);
}

void window_clear_clip_rect(void) {
    SDL_RenderSetClipRect(g_renderer, NULL);
}

void window_shutdown(void) {
    if (g_renderer != NULL) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
    }
    if (g_window != NULL) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }
    SDL_Quit();
}
