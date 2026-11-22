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

#ifndef I18N_H
#define I18N_H

void i18n_init(void);
void i18n_shutdown(void);
const char *tr(const char *text);
int i18n_language_count(void);
const char *i18n_language_code(int index);
const char *i18n_language_name(int index);
int i18n_set_language(const char *code);
const char *i18n_current_language(void);

#endif
