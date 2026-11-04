/*
 * ============================================================
 * project_create.c
 * ============================================================
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <direct.h>
#define REBAX_MKDIR(path, mode) _mkdir(path)
#else
#define REBAX_MKDIR(path, mode) mkdir(path, mode)
#endif

#include <errno.h>

#include "project_create.h"

/* Creates a single directory if it doesn't already exist. Returns 1 on success (or
 * if the directory already existed), and 0 on true failure (permissions, etc.) */
static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 1 : 0; /* Exists but is a regular file, not a directory -> failure */
    }
    if (REBAX_MKDIR(path, 0755) == 0) {
        return 1;
    }
    return (errno == EEXIST) ? 1 : 0;
}

project_create_result_t project_create(const char *project_name, const char *base_path) {
    if (project_name == NULL || project_name[0] == '\0'
        || base_path == NULL || base_path[0] == '\0') {
        return PROJECT_CREATE_ERROR_INVALID_INPUT;
    }

    char project_dir[900];
    snprintf(project_dir, sizeof(project_dir), "%s/%s", base_path, project_name);

    if (!ensure_dir(project_dir)) {
        return PROJECT_CREATE_ERROR_MKDIR;
    }

    char assets_dir[1024];
    snprintf(assets_dir, sizeof(assets_dir), "%s/Assets", project_dir);
    if (!ensure_dir(assets_dir)) {
        return PROJECT_CREATE_ERROR_MKDIR;
    }

    char settings_path[1024];
    snprintf(settings_path, sizeof(settings_path), "%s/project.rebax", project_dir);

    FILE *f = fopen(settings_path, "w");
    if (f == NULL) {
        return PROJECT_CREATE_ERROR_FILE;
    }
    fprintf(f, "name=%s\n", project_name);
    /* Current editor version is 0.1.0. At the first stable 1.0.0 release,
     * keep a versioned project reader/migration path for earlier formats. */
    fprintf(f, "engine_version=0.1.0\n");
    fprintf(f, "project_format=1\n");
    fclose(f);

    return PROJECT_CREATE_OK;
}
