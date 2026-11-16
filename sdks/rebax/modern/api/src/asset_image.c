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
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "asset_image.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "stb_image.h"

static unsigned char *read_file(const char *path, size_t *out_size) {
    SDL_RWops *file = SDL_RWFromFile(path, "rb");
    if (file == NULL) return NULL;
    Sint64 size = SDL_RWsize(file);
    if (size <= 0 || size > 0x7fffffff) {
        SDL_RWclose(file);
        return NULL;
    }
    unsigned char *data = (unsigned char *)malloc((size_t)size);
    if (data == NULL) {
        SDL_RWclose(file);
        return NULL;
    }
    size_t read = SDL_RWread(file, data, 1, (size_t)size);
    SDL_RWclose(file);
    if (read != (size_t)size) {
        free(data);
        return NULL;
    }
    *out_size = read;
    return data;
}

unsigned char *asset_image_load(const char *project_path, int *out_width, int *out_height) {
    if (project_path == NULL || project_path[0] == '\0' || out_width == NULL || out_height == NULL) return NULL;
    const char *relative = project_path;
    if (strncmp(relative, "assets/", 7) == 0) relative += 7;
    size_t size = 0;
    unsigned char *data = NULL;
    char *base = SDL_GetBasePath();
    if (base != NULL) {
        size_t length = strlen(base) + strlen(relative) + 16;
        char *full = (char *)malloc(length);
        if (full != NULL) {
            SDL_snprintf(full, length, "%sassets/%s", base, relative);
            data = read_file(full, &size);
            free(full);
        }
        SDL_free(base);
    }
    if (data == NULL) data = read_file(relative, &size);
    if (data == NULL) return NULL;
    int channels = 0;
    unsigned char *pixels = stbi_load_from_memory(data, (int)size, out_width, out_height, &channels, 4);
    free(data);
    return pixels;
}

void asset_image_free(unsigned char *pixels) {
    if (pixels != NULL) stbi_image_free(pixels);
}
