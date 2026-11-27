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
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "project/project_types.h"

#include <stdio.h>
#include <string.h>

static const char *const TYPE_IDS[PROJECT_TYPE_COUNT] = {"modern", "ps2", "ps1"};
static const char *const TYPE_LABELS[PROJECT_TYPE_COUNT] = {"Desktop and Mobile", "PlayStation 2", "PlayStation 1"};
static const char *const TYPE_HINTS[PROJECT_TYPE_COUNT] = {"Windows, Linux, macOS, Android and iOS", "", ""};
static const char *const SYSTEM_IDS[DEV_SYSTEM_COUNT] = {"nodes", "native"};
static const char *const SYSTEM_LABELS[DEV_SYSTEM_COUNT] = {"Nodes", "Near-Native"};
static const char *const METHOD_IDS[EXPORT_METHOD_COUNT] = {"compiler", "template"};

static const struct {
    const char *id;
    unsigned bit;
} PLATFORMS[] = {
    {"android", PROJECT_PLATFORM_ANDROID}, {"linux", PROJECT_PLATFORM_LINUX}, {"windows", PROJECT_PLATFORM_WINDOWS},
    {"macos", PROJECT_PLATFORM_MACOS}, {"ios", PROJECT_PLATFORM_IOS},
};

static int parse_id(const char *const *ids, int count, const char *id, int *out) {
    if (id == NULL) return 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(ids[i], id) == 0) { *out = i; return 1; }
    }
    return 0;
}

const char *project_type_id(project_type_t type) { return (unsigned)type < PROJECT_TYPE_COUNT ? TYPE_IDS[type] : TYPE_IDS[0]; }
const char *project_type_label(project_type_t type) { return (unsigned)type < PROJECT_TYPE_COUNT ? TYPE_LABELS[type] : TYPE_LABELS[0]; }
const char *project_type_hint(project_type_t type) { return (unsigned)type < PROJECT_TYPE_COUNT ? TYPE_HINTS[type] : ""; }
int project_type_parse(const char *id, project_type_t *out) { int v; if (!parse_id(TYPE_IDS, PROJECT_TYPE_COUNT, id, &v)) return 0; *out = (project_type_t)v; return 1; }

const char *dev_system_id(dev_system_t system) { return (unsigned)system < DEV_SYSTEM_COUNT ? SYSTEM_IDS[system] : SYSTEM_IDS[0]; }
const char *dev_system_label(dev_system_t system) { return (unsigned)system < DEV_SYSTEM_COUNT ? SYSTEM_LABELS[system] : SYSTEM_LABELS[0]; }
int dev_system_parse(const char *id, dev_system_t *out) { int v; if (!parse_id(SYSTEM_IDS, DEV_SYSTEM_COUNT, id, &v)) return 0; *out = (dev_system_t)v; return 1; }
int dev_system_is_available(dev_system_t system) { return system == DEV_SYSTEM_NODES || system == DEV_SYSTEM_NATIVE; }

const char *export_method_id(export_method_t method) { return (unsigned)method < EXPORT_METHOD_COUNT ? METHOD_IDS[method] : METHOD_IDS[0]; }
int export_method_parse(const char *id, export_method_t *out) { int v; if (!parse_id(METHOD_IDS, EXPORT_METHOD_COUNT, id, &v)) return 0; *out = (export_method_t)v; return 1; }

unsigned project_platforms_parse(const char *list) {
    unsigned bits = 0;
    char word[16];
    int len = 0;
    for (const char *p = list;; p++) {
        if (*p == ',' || *p == '\0') {
            word[len] = '\0';
            for (size_t i = 0; i < sizeof(PLATFORMS) / sizeof(PLATFORMS[0]); i++) {
                if (strcmp(PLATFORMS[i].id, word) == 0) bits |= PLATFORMS[i].bit;
            }
            len = 0;
            if (*p == '\0') break;
        } else if (len < (int)sizeof(word) - 1) {
            word[len++] = *p;
        }
    }
    return bits;
}

void project_platforms_format(unsigned platforms, char *out, int cap) {
    out[0] = '\0';
    int used = 0;
    for (size_t i = 0; i < sizeof(PLATFORMS) / sizeof(PLATFORMS[0]); i++) {
        if (!(platforms & PLATFORMS[i].bit)) continue;
        int n = snprintf(out + used, (size_t)(cap - used), "%s%s", used ? "," : "", PLATFORMS[i].id);
        if (n < 0 || n >= cap - used) { out[used] = '\0'; return; }
        used += n;
    }
}

unsigned project_type_default_platforms(project_type_t type) { return type == PROJECT_TYPE_MODERN ? PROJECT_PLATFORM_ALL : 0; }
