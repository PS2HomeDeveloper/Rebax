/*
 * ============================================================
 * project_create.h
 * ============================================================
 * Creates an actual new project structure on disk: a folder named after
 * the project inside the given base path, an empty Assets/ folder inside
 * it, and the project settings file project.rebax. Creates any missing
 * intermediate directories automatically.
 * ============================================================
 */

#ifndef PROJECT_CREATE_H
#define PROJECT_CREATE_H

typedef enum {
    PROJECT_CREATE_OK = 0,
    PROJECT_CREATE_ERROR_INVALID_INPUT, /* Empty name or path */
    PROJECT_CREATE_ERROR_MKDIR,         /* Failed to create directory (permissions, etc.) */
    PROJECT_CREATE_ERROR_FILE           /* Failed to write project.rebax */
} project_create_result_t;

/* Creates the full project folder: <base_path>/<project_name>/ containing
 * Assets/ (empty) and project.rebax (initial settings). */
project_create_result_t project_create(const char *project_name, const char *base_path);

#endif /* PROJECT_CREATE_H */
