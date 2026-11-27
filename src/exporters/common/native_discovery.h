#ifndef REBAX_NATIVE_DISCOVERY_H
#define REBAX_NATIVE_DISCOVERY_H
int export_native_discover(const char *source_root, const char *sdk_root);
int export_native_unresolved_count(void);
const char *export_native_unresolved_at(int index);
int export_native_finish(const char *src_dir);
int export_native_section_count(void);
#endif
