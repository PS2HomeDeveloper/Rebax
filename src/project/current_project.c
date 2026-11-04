/*
 * ============================================================
 * current_project.c
 * ============================================================
 */

#include <string.h>
#include <stddef.h>

#include "current_project.h"

#define MAX_PATH_LEN 1024

static char g_path[MAX_PATH_LEN] = {0};
static int g_is_open = 0;

void current_project_set_path(const char *root_path) {
    if (root_path == NULL || root_path[0] == '\0') {
        g_path[0] = '\0';
        g_is_open = 0;
        return;
    }
    strncpy(g_path, root_path, MAX_PATH_LEN - 1);
    g_path[MAX_PATH_LEN - 1] = '\0';
    g_is_open = 1;
}

const char *current_project_get_path(void) {
    return g_is_open ? g_path : NULL;
}

int current_project_is_open(void) {
    return g_is_open;
}
