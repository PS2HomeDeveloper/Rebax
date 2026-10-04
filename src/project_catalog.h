#ifndef PROJECT_CATALOG_H
#define PROJECT_CATALOG_H

#define PROJECT_CATALOG_MAX 64
#define PROJECT_CATALOG_PATH_MAX 600
#define PROJECT_CATALOG_NAME_MAX 128

typedef struct {
    char path[PROJECT_CATALOG_PATH_MAX];
    char name[PROJECT_CATALOG_NAME_MAX];
    long long last_opened;
} project_catalog_entry_t;

int project_catalog_load(void);
int project_catalog_count(void);
const project_catalog_entry_t *project_catalog_get(int index);
int project_catalog_add(const char *project_path);
int project_catalog_remove(int index);
int project_catalog_is_valid(const char *project_path);

#endif
