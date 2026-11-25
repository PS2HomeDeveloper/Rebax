#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "run/simulation/sim_profile.h"
#include "filesystem/resources.h"

static void set_defaults(sim_profile_t *profile, const char *id,
                         const char *label, int width, int height,
                         int frame_rate, int nearest_filter) {
    memset(profile, 0, sizeof(*profile));
    snprintf(profile->id, sizeof(profile->id), "%s", id);
    snprintf(profile->label, sizeof(profile->label), "%s", label);
    profile->width = width;
    profile->height = height;
    profile->alternate_width = width;
    profile->alternate_height = height;
    profile->frame_rate = frame_rate;
    profile->nearest_filter = nearest_filter;
    profile->integer_scale = nearest_filter;
    profile->memory_mb = 32;
    profile->texture_memory_mb = 8;
    profile->max_texture_size = 1024;
}

void sim_profile_defaults(sim_profile_t *profile, project_type_t type) {
    if (profile == NULL) return;
    if (type == PROJECT_TYPE_PS2) {
        set_defaults(profile, "ps2", "PlayStation 2", 640, 448, 60, 1);
    } else if (type == PROJECT_TYPE_PS1) {
        set_defaults(profile, "ps1", "PlayStation 1", 320, 240, 60, 1);
    } else {
        set_defaults(profile, "modern", "Modern", 640, 360, 60, 0);
    }
}

static int parse_int(const char *text, int fallback) {
    char *end;
    long value;
    if (text == NULL || text[0] == '\0') return fallback;
    end = NULL;
    value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 0 || value > 32768) {
        return fallback;
    }
    return (int)value;
}

static void apply_value(sim_profile_t *profile, const char *key,
                        const char *value) {
    if (strcmp(key, "id") == 0) {
        snprintf(profile->id, sizeof(profile->id), "%s", value);
    } else if (strcmp(key, "label") == 0) {
        snprintf(profile->label, sizeof(profile->label), "%s", value);
    } else if (strcmp(key, "width") == 0) {
        profile->width = parse_int(value, profile->width);
    } else if (strcmp(key, "height") == 0) {
        profile->height = parse_int(value, profile->height);
    } else if (strcmp(key, "alternate_width") == 0) {
        profile->alternate_width = parse_int(value, profile->alternate_width);
    } else if (strcmp(key, "alternate_height") == 0) {
        profile->alternate_height = parse_int(value, profile->alternate_height);
    } else if (strcmp(key, "frame_rate") == 0) {
        profile->frame_rate = parse_int(value, profile->frame_rate);
    } else if (strcmp(key, "nearest_filter") == 0) {
        profile->nearest_filter = parse_int(value, profile->nearest_filter) != 0;
    } else if (strcmp(key, "integer_scale") == 0) {
        profile->integer_scale = parse_int(value, profile->integer_scale) != 0;
    } else if (strcmp(key, "memory_mb") == 0) {
        profile->memory_mb = parse_int(value, profile->memory_mb);
    } else if (strcmp(key, "texture_memory_mb") == 0) {
        profile->texture_memory_mb = parse_int(value, profile->texture_memory_mb);
    } else if (strcmp(key, "max_texture_size") == 0) {
        profile->max_texture_size = parse_int(value, profile->max_texture_size);
    } else if (strcmp(key, "safe_area") == 0) {
        profile->safe_area = parse_int(value, profile->safe_area);
    }
}

static void trim_left(char **text) {
    while (**text == ' ' || **text == '\t') (*text)++;
}

int sim_profile_load(sim_profile_t *profile, project_type_t type) {
    const char *id;
    unsigned char *data;
    size_t size = 0;
    char path[96];
    char *save = NULL;
    char *line;
    if (profile == NULL) return 0;
    sim_profile_defaults(profile, type);
    id = profile->id;
    snprintf(path, sizeof(path), "simulation/profiles/%s.profile", id);
    data = resource_file_read(path, &size);
    if (data == NULL || size == 0) {
        free(data);
        return 0;
    }
    line = strtok_r((char *)data, "\r\n", &save);
    while (line != NULL) {
        char *equals = strchr(line, '=');
        if (equals != NULL) {
            char *key = line;
            char *value = equals + 1;
            *equals = '\0';
            trim_left(&key);
            trim_left(&value);
            if (key[0] != '\0' && value[0] != '\0') {
                apply_value(profile, key, value);
            }
        }
        line = strtok_r(NULL, "\r\n", &save);
    }
    free(data);
    if (profile->width <= 0 || profile->height <= 0 ||
        profile->frame_rate <= 0 || profile->frame_rate > 240 ||
        profile->alternate_width < 0 || profile->alternate_height < 0) {
        sim_profile_defaults(profile, type);
        return 0;
    }
    return 1;
}
