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

#ifndef ASSET_IMAGE_H
#define ASSET_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

unsigned char *asset_image_load(const char *project_path, int *out_width, int *out_height);
void asset_image_free(unsigned char *pixels);

#ifdef __cplusplus
}
#endif

#endif
