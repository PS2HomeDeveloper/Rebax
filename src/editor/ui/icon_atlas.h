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

/*
 * ============================================================
 * icon_atlas.h
 * ============================================================
 * Loads the engine's bundled icon atlas (a single image containing all engine icons,
 * 256x256, a 16x16 grid), and provides any drawing file the ability to draw any icon
 * once its index is known - the index itself is provided by the generated icon_names.h
 * at build time (example: ICON_Folder, ICON_add).
 *
 * No need to edit this file when adding a new icon - just put a new PNG in
 * resources/images/icons/icons_src/ and build.
 * ============================================================
 */

#ifndef ICON_ATLAS_H
#define ICON_ATLAS_H

#include "icon_names.h" /* Auto-generated - defines ICON_<filename> for each icon */

/* Loads the bundled atlas (only once). Returns 1 on success and 0 on
 * failure. Must be called before any of the drawing functions below */
int icon_atlas_init(void);

/* Draws a single icon (use ICON_* constants from icon_names.h), at the requested
 * size (automatically scaled if the size differs from the original 16x16) */
void icon_atlas_draw(int icon_id, int x, int y, int size);

/* Same as icon_atlas_draw but rotates the icon around its center - used for file/tree
 * expand/collapse arrows (0 closed, 90 open) */
void icon_atlas_draw_rotated(int icon_id, int x, int y, int size, double angle_degrees);

/* Same as icon_atlas_draw but with extra tinting (like window_draw_texture_tinted) */
void icon_atlas_draw_tinted(int icon_id, int x, int y, int size,
                             unsigned char r, unsigned char g, unsigned char b);

/* Frees the loaded atlas - called once at program shutdown */
void icon_atlas_shutdown(void);

#endif /* ICON_ATLAS_H */
