#ifndef NODE_SOURCE_H
#define NODE_SOURCE_H

typedef void (*node_source_name_cb)(const char *name, void *user);

int node_source_list(node_source_name_cb callback, void *user);
int node_source_copy(const char *name, const char *dest_path);
char *node_source_read(const char *name, long *out_size);

#endif
