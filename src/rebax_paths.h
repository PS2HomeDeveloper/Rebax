#ifndef REBAX_PATHS_H
#define REBAX_PATHS_H

/* Creates and returns the per-user Rebax root and its standard directories. */
const char *rebax_root_dir(void);
const char *rebax_toolchains_dir(void);       /* Rebax/Engine/toolchains */
const char *rebax_toolchain_dir(void);        /* Rebax/Engine/toolchains/ps2dev */
const char *rebax_make_path(void);            /* Rebax/Engine/toolchains/make[.exe] */
const char *rebax_node_resources_dir(void);   /* Rebax/Engine/resources/nodes */
const char *rebax_temp_export_dir(void);      /* Rebax/Temp/export */
const char *rebax_settings_dir(void);         /* Rebax/Settings */

/* First-run extraction of embedded toolchains and node resources. */
int rebax_paths_is_setup_needed(void);
void rebax_paths_setup_start(void);
void rebax_paths_setup_update(void);
int rebax_paths_setup_done(void);
int rebax_paths_setup_failed(void);

#endif
