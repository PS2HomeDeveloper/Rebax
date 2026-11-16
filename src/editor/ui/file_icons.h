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

/*
 * ============================================================
 * file_icons.h
 * ============================================================
 * Defines the icon for each file type by extension - used anywhere that
 * displays files (file browser, filesystem panel, etc.) instead of each place
 * duplicating its own extension table.
 * ============================================================
 */

#ifndef FILE_ICONS_H
#define FILE_ICONS_H

/* Is the extension (without dot, case-insensitive) an image format
 * supported by the engine? */
int file_icons_is_raster_image_ext(const char *ext);

/* The icon number (ICON_* constants) appropriate for a full filename like "a.png" */
int file_icons_for_name(const char *file_name);

#endif /* FILE_ICONS_H */
