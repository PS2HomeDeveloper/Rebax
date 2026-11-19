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

#ifndef PROJECT_TYPES_H
#define PROJECT_TYPES_H

typedef enum {
    PROJECT_TYPE_MODERN = 0,
    PROJECT_TYPE_PS2,
    PROJECT_TYPE_PS1,
    PROJECT_TYPE_COUNT
} project_type_t;

typedef enum {
    DEV_SYSTEM_NODES = 0,
    DEV_SYSTEM_NATIVE,
    DEV_SYSTEM_COUNT
} dev_system_t;

typedef enum {
    EXPORT_METHOD_COMPILER = 0,
    EXPORT_METHOD_TEMPLATE,
    EXPORT_METHOD_COUNT
} export_method_t;

#define PROJECT_PLATFORM_ANDROID 1u
#define PROJECT_PLATFORM_LINUX 2u
#define PROJECT_PLATFORM_WINDOWS 4u
#define PROJECT_PLATFORM_MACOS 8u
#define PROJECT_PLATFORM_IOS 16u
#define PROJECT_PLATFORM_ALL 31u

const char *project_type_id(project_type_t type);
const char *project_type_label(project_type_t type);
int project_type_parse(const char *id, project_type_t *out);

const char *dev_system_id(dev_system_t system);
const char *dev_system_label(dev_system_t system);
int dev_system_parse(const char *id, dev_system_t *out);
int dev_system_is_available(dev_system_t system);

const char *export_method_id(export_method_t method);
int export_method_parse(const char *id, export_method_t *out);

unsigned project_platforms_parse(const char *list);
void project_platforms_format(unsigned platforms, char *out, int cap);
unsigned project_type_default_platforms(project_type_t type);

#endif
