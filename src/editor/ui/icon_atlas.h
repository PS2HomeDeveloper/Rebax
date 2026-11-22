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

#ifndef ICON_ATLAS_H
#define ICON_ATLAS_H

#include "icon_names.h"

int icon_atlas_init(void);

void icon_atlas_draw(int icon_id, int x, int y, int size);

void icon_atlas_draw_rotated(int icon_id, int x, int y, int size, double angle_degrees);

void icon_atlas_draw_tinted(int icon_id, int x, int y, int size,
                             unsigned char r, unsigned char g, unsigned char b);

void icon_atlas_shutdown(void);

#endif
