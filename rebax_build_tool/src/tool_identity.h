/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) | _| | _ \ / _ \   \  /
 * |  _ <| |___| |_) / ___ \  /  \
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */
#ifndef REBAX_TOOL_IDENTITY_H
#define REBAX_TOOL_IDENTITY_H

int rebax_library_identity(const char *path, const char *architecture, char *description, int cap);
int cmd_identify_lib(int argc, char **argv);

#endif
