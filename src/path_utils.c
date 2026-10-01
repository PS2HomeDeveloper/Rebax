#include <ctype.h>
#include <string.h>
#include "path_utils.h"

int path_utils_compare_names(const void *a, const void *b) {
    return strcmp(*(const char * const *)a, *(const char * const *)b);
}

int path_utils_compare_names_ci(const void *a, const void *b) {
    const unsigned char *left = (const unsigned char *)*(const char * const *)a;
    const unsigned char *right = (const unsigned char *)*(const char * const *)b;
    while (*left != '\0' && *right != '\0') {
        int diff = tolower(*left) - tolower(*right);
        if (diff != 0) return diff;
        left++; right++;
    }
    return tolower(*left) - tolower(*right);
}

const char *path_utils_basename(const char *path) {
    if (path == NULL) return "";
    const char *slash = strrchr(path, '/');
    return (slash != NULL) ? slash + 1 : path;
}
