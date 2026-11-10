#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

#include "resource_files.h"

unsigned char *resource_file_read(const char *relative_path, size_t *out_size) {
    char path[1536];
    if (out_size) *out_size = 0;
    if (!relative_path) return NULL;
#if defined(__ANDROID__)
    snprintf(path, sizeof(path), "resources/%s", relative_path);
#else
    char *base = SDL_GetBasePath();
    snprintf(path, sizeof(path), "%sresources/%s", base ? base : "", relative_path);
    SDL_free(base);
#endif
    SDL_RWops *rw = SDL_RWFromFile(path, "rb");
    if (!rw) return NULL;
    Sint64 size = SDL_RWsize(rw);
    if (size < 0) { SDL_RWclose(rw); return NULL; }
    unsigned char *data = (unsigned char *)malloc((size_t)size + 1);
    if (!data) { SDL_RWclose(rw); return NULL; }
    size_t got = 0;
    while (got < (size_t)size) {
        size_t n = SDL_RWread(rw, data + got, 1, (size_t)size - got);
        if (n == 0) break;
        got += n;
    }
    SDL_RWclose(rw);
    if (got != (size_t)size) { free(data); return NULL; }
    data[got] = '\0';
    if (out_size) *out_size = got;
    return data;
}
