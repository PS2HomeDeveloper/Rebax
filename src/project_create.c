/*
 * ============================================================
 * project_create.c
 * ============================================================
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "project_create.h"

/* ينشئ مجلداً واحداً لو غير موجود مسبقاً. يرجع 1 عند النجاح (أو
 * لو كان المجلد موجوداً أصلاً)، و0 عند فشل حقيقي (صلاحيات مثلاً) */
static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 1 : 0; /* موجود لكنه ملف عادي مو مجلد -> فشل */
    }
    if (mkdir(path, 0755) == 0) {
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
    fprintf(f, "engine_version=0.1\n");
    fclose(f);

    return PROJECT_CREATE_OK;
}
