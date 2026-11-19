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
 * element.c (nodes_editor)
 * ============================================================
 * Visual representation of Element in the editor - Element is an abstract node without any spatial property (no Position), so there is nothing to draw here (draw_2d and draw_3d are both unsupported, they remain NULL automatically). Present in this file only for structural stability - every node type has a corresponding file here, even if visually empty currently.
 * ============================================================
 */

/* @NODE_EDITOR type=NODE_TYPE_ELEMENT */
