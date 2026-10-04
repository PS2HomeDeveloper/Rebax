/*
 * ============================================================
 * font.h
 * ============================================================
 * Font library. Loads the two bundled font files (Regular and Bold) only
 * once, and provides a general function for any other library to render
 * any text (sentence, character, number) at any desired size as RGBA
 * pixel data ready for drawing.
 *
 * Used by any UI library (ui_project_center, etc.) that needs to place
 * text on a button, icon, or elsewhere.
 * ============================================================
 */

#ifndef FONT_H
#define FONT_H

/* Requested font weight */
typedef enum {
    FONT_WEIGHT_REGULAR,
    FONT_WEIGHT_BOLD
} font_weight_t;

/* Text image ready for drawing - RGBA pixels (text is white with transparent edges,
 * recolored later at draw time to the desired color) */
typedef struct {
    unsigned char *pixels; /* Its length is width * height * 4 bytes */
    int width;
    int height;
} font_text_image_t;

/* Loads the two bundled font files (only once). Returns 1 on success
 * and 0 on failure. Must be called before any call to font_render_text */
int font_init(void);

/* Renders text (sentence, character, number - any string) at the requested weight and size
 * (pixel_size is approximately the character height in pixels, same concept as the usual "font size").
 * The result is a new memory allocation - free it later with
 * font_free_text_image after use or after converting it to a texture */
font_text_image_t font_render_text(const char *text, font_weight_t weight,
                                    int pixel_size);

/* Render text using one RGB color per UTF-8 byte; a NULL color map uses white. */
font_text_image_t font_render_text_colored(const char *text, font_weight_t weight,
                                            int pixel_size, const unsigned char *rgb_by_byte);

/* Frees a text image produced by font_render_text */
void font_free_text_image(font_text_image_t *img);

/* Frees internally loaded font data - called once at program shutdown */
void font_shutdown(void);

#endif /* FONT_H */
