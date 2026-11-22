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

#ifndef ENGINE_SETTINGS_H
#define ENGINE_SETTINGS_H

typedef struct {
    int ui_scale_percent;
    char language[16];
} engine_settings_t;

void engine_settings_load(void);
void engine_settings_save(void);
const engine_settings_t *engine_settings_get(void);
void engine_settings_set_ui_scale(int percent);
void engine_settings_set_language(const char *code);

#endif
