/* أدوات أسماء ومسارات مشتركة بلا اعتماد على الواجهة. */
#ifndef PATH_UTILS_H
#define PATH_UTILS_H

int path_utils_compare_names(const void *a, const void *b);
int path_utils_compare_names_ci(const void *a, const void *b);
const char *path_utils_basename(const char *path);

#endif /* PATH_UTILS_H */
