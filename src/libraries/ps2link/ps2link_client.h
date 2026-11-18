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

#ifndef PS2LINK_CLIENT_H
#define PS2LINK_CLIENT_H

typedef enum {
    PS2LINK_STATE_IDLE,
    PS2LINK_STATE_CONNECTING,
    PS2LINK_STATE_RUNNING,
    PS2LINK_STATE_ENDED,
    PS2LINK_STATE_FAILED
} ps2link_state_t;

/* Starts a background session: connects to ps2link on the PS2 at host, runs
 * the ELF root_dir/elf_name there (served to the PS2 as host:), and forwards
 * the console output. Returns 1 when the session thread started. */
int ps2link_start(const char *host, const char *root_dir, const char *elf_name);

/* Stops the running program (the PS2 is reset) and ends the session. */
void ps2link_stop(void);

ps2link_state_t ps2link_get_state(void);

/* Human readable reason when the state is PS2LINK_STATE_FAILED. */
const char *ps2link_get_error(void);

/* Oldest unread console line, or NULL. Valid until the next call. */
const char *ps2link_poll_line(void);

#endif
