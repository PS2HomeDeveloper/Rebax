/* Project C/C++ source collection for native PS2 exports. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include "export_internal.h"
#include "rebax_fs.h"

#pragma GCC diagnostic ignored "-Wformat-truncation"

static int has_extension(const char *name, const char *ext) {
    size_t n = strlen(name), e = strlen(ext);
    return n >= e && strcmp(name + n - e, ext) == 0;
}

static int is_project_source(const char *name) {
    return has_extension(name, ".c") || has_extension(name, ".cc") ||
           has_extension(name, ".cpp") || has_extension(name, ".cxx") ||
           has_extension(name, ".h") || has_extension(name, ".hh") ||
           has_extension(name, ".hpp") || has_extension(name, ".hxx");
}

static int ignored_directory(const char *name) {
    return strcmp(name, ".git") == 0 || strcmp(name, ".svn") == 0 ||
           strcmp(name, "build") == 0 || strcmp(name, "Build") == 0 ||
           strcmp(name, "Temp") == 0 || strcmp(name, "temp") == 0 ||
           strcmp(name, ".rebax") == 0;
}

static void sort_names(char names[][256], int count) {
    for (int i = 1; i < count; i++) {
        char key[256];
        memcpy(key, names[i], sizeof(key));
        int j = i;
        while (j > 0 && strcmp(key, names[j - 1]) < 0) {
            memcpy(names[j], names[j - 1], sizeof(names[j]));
            j--;
        }
        memcpy(names[j], key, sizeof(names[j]));
    }
}

static int copy_tree_sources(const char *project_root, const char *relative,
                             const char *destination_root, int *copied_count) {
    char source_dir[2048];
    snprintf(source_dir, sizeof(source_dir), "%s%s%s", project_root,
             relative[0] ? "/" : "", relative);
    DIR *dir = opendir(source_dir);
    if (dir == NULL) return 1;

    char (*names)[256] = (char (*)[256])malloc(512 * 256);
    if (names == NULL) { closedir(dir); return 0; }
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < 512) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        strncpy(names[count], entry->d_name, sizeof(names[count]) - 1);
        names[count][sizeof(names[count]) - 1] = '\0';
        count++;
    }
    closedir(dir);
    sort_names(names, count);

    for (int i = 0; i < count; i++) {
        char child_relative[2048];
        snprintf(child_relative, sizeof(child_relative), "%s%s%s", relative,
                 relative[0] ? "/" : "", names[i]);
        char child_source[2048];
        snprintf(child_source, sizeof(child_source), "%s/%s", project_root, child_relative);
        struct stat st;
        if (stat(child_source, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (ignored_directory(names[i])) continue;
            if (!copy_tree_sources(project_root, child_relative, destination_root, copied_count)) { free(names); return 0; }
            continue;
        }
        if (!S_ISREG(st.st_mode) || !is_project_source(names[i])) continue;

        char destination[2048];
        snprintf(destination, sizeof(destination), "%s/project/%s", destination_root, child_relative);
        char destination_dir[2048];
        strncpy(destination_dir, destination, sizeof(destination_dir) - 1);
        destination_dir[sizeof(destination_dir) - 1] = '\0';
        char *slash = strrchr(destination_dir, '/');
        if (slash != NULL) {
            *slash = '\0';
            if (!rebax_fs_mkdir_p(destination_dir)) { free(names); return 0; }
        }
        if (!rebax_fs_copy_file(child_source, destination)) { free(names); return 0; }
        export_scan_file_for_export_flags(child_source);
        (*copied_count)++;
        log_pushf("[exporter] included project source: %s", child_relative);
    }
    free(names);
    return 1;
}

int export_project_copy_sources(const char *project_root, const char *dest_src_dir) {
    if (project_root == NULL || dest_src_dir == NULL) return 0;
    int copied_count = 0;
    if (!copy_tree_sources(project_root, "", dest_src_dir, &copied_count)) {
        log_push("[exporter] FAILED: could not copy a project C/C++ source file.");
        return 0;
    }
    log_pushf("[exporter] project source export complete: %d C/C++ source/header files", copied_count);
    return 1;
}
