#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "asset_files.h"

#define ASSET_MANIFEST_PATH "rebax_assets.txt"
#define ASSET_COPY_CHUNK (1024 * 1024)

unsigned char *asset_file_read(const char *asset_path, size_t *out_size) {
    SDL_RWops *rw = SDL_RWFromFile(asset_path, "rb");
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

int asset_file_copy(const char *asset_path, const char *dest_path) {
    SDL_RWops *rw = SDL_RWFromFile(asset_path, "rb");
    if (!rw) return 0;
    unsigned char *buffer = (unsigned char *)malloc(ASSET_COPY_CHUNK);
    if (!buffer) { SDL_RWclose(rw); return 0; }
    FILE *out = fopen(dest_path, "wb");
    if (!out) { free(buffer); SDL_RWclose(rw); return 0; }
    int ok = 1;
    size_t n;
    while ((n = SDL_RWread(rw, buffer, 1, ASSET_COPY_CHUNK)) > 0) {
        if (fwrite(buffer, 1, n, out) != n) { ok = 0; break; }
    }
    if (fclose(out) != 0) ok = 0;
    free(buffer);
    SDL_RWclose(rw);
    if (!ok) remove(dest_path);
    return ok;
}

int asset_file_list(const char *dir_prefix, asset_file_name_cb callback, void *user) {
    size_t size = 0;
    unsigned char *manifest = asset_file_read(ASSET_MANIFEST_PATH, &size);
    if (!manifest) return 0;
    size_t prefix_len = strlen(dir_prefix);
    char *line = (char *)manifest;
    while (*line) {
        char *end = strchr(line, '\n');
        if (end) *end = '\0';
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';
        if (len > prefix_len && strncmp(line, dir_prefix, prefix_len) == 0 && strchr(line + prefix_len, '/') == NULL) {
            callback(line + prefix_len, user);
        }
        if (!end) break;
        line = end + 1;
    }
    free(manifest);
    return 1;
}
