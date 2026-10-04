/*
 * ============================================================
 * engine_context.h
 * ============================================================
 * Global runtime context that any PS2 node needs access to during
 * init/update/draw - currently only GSGLOBAL (the global graphics
 * context), because it's the first thing a real node (Sprite2D)
 * actually needed. It is provided by the game's runtime code itself
 * (the main loop on the PS2 - a separate upcoming export step, not
 * written yet) - nodes here just call it, they don't define it.
 *
 * Why a function and not a direct extern variable? So we don't
 * mandate its storage form (global variable, singleton, control-thread
 * static...) - any node that needs GS just calls the function,
 * wherever its real source may be later.
 * ============================================================
 */

#ifndef ENGINE_CONTEXT_H
#define ENGINE_CONTEXT_H

/* GSGLOBAL is defined in gsKit.h (the gsKit library, part bundled in
 * the PS2dev environment with the executable - see the Makefile comment) */
#include <gsKit.h>

/* Returns the same GSGLOBAL* that the engine initialized once at game
 * startup on the PS2 (gsKit_init_global in the game's runtime code, a
 * separate upcoming step) - any node needing to draw or load textures
 * calls it */
GSGLOBAL *engine_get_gs_global(void);

#endif /* ENGINE_CONTEXT_H */
