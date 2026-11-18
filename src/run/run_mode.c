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

#include <stdio.h>
#include "run/run_mode.h"
#include "packages/package_manager.h"

#define PLATFORM_PS2 5
#define PLATFORM_PS1 6

#if defined(__ANDROID__)
#define HOST_PLATFORM 0
#elif defined(_WIN32)
#define HOST_PLATFORM 2
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define HOST_PLATFORM 4
#else
#define HOST_PLATFORM 3
#endif
#else
#define HOST_PLATFORM 1
#endif

int run_host_platform_index(void) { return HOST_PLATFORM; }

int run_host_is_desktop(void) { return HOST_PLATFORM != 0 && HOST_PLATFORM != 4; }

run_mode_t run_mode_default(project_type_t type) { (void)type; return RUN_MODE_SIMULATION; }

static void fill(run_mode_info_t *m, run_mode_t mode, const char *label, int available, const char *reason, int platform) {
    m->mode = mode;
    m->has_settings = (mode == RUN_MODE_EMULATOR || mode == RUN_MODE_PS2_DEVICE);
    m->available = available;
    m->package_platform = platform;
    snprintf(m->label, sizeof(m->label), "%s", label);
    snprintf(m->reason, sizeof(m->reason), "%s", reason);
}

static int package_ready(int platform) { return package_platform_state(platform) == PKG_STATE_INSTALLED; }

int run_modes_for_project(project_type_t type, run_mode_info_t *out, int max) {
    int n = 0;
    if (max < 4) return 0;
    fill(&out[n++], RUN_MODE_SIMULATION, "Built-in simulation", 1, "", -1);
    if (type == PROJECT_TYPE_MODERN) {
        if (HOST_PLATFORM != 4) {
            int ok = package_ready(HOST_PLATFORM);
            fill(&out[n++], RUN_MODE_COMPILED, "Run on this device (compiled)", ok,
                 ok ? "" : "Install the package for this device first", HOST_PLATFORM);
        }
    } else if (type == PROJECT_TYPE_PS2) {
        int ok = package_ready(PLATFORM_PS2);
        fill(&out[n++], RUN_MODE_EMULATOR, "PlayStation 2 emulator", ok,
             ok ? "" : "Install the PlayStation 2 package first", PLATFORM_PS2);
        fill(&out[n++], RUN_MODE_PS2_DEVICE, "Real PlayStation 2 (network)", ok,
             ok ? "" : "Install the PlayStation 2 package first", PLATFORM_PS2);
    } else if (type == PROJECT_TYPE_PS1) {
        int ok = package_ready(PLATFORM_PS1);
        fill(&out[n++], RUN_MODE_EMULATOR, "PlayStation 1 emulator", ok,
             ok ? "" : "Install the PlayStation 1 package first", PLATFORM_PS1);
    }
    return n;
}
