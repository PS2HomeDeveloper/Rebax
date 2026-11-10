#ifndef PACKAGE_MANAGER_H
#define PACKAGE_MANAGER_H

#define PKG_PLATFORM_COUNT 7
#define PKG_MAX_ARCHS 4

typedef enum {
    PKG_STATE_NOT_INSTALLED,
    PKG_STATE_PARTIAL,
    PKG_STATE_INSTALLED,
    PKG_STATE_UNAVAILABLE
} pkg_state_t;

typedef enum {
    PKG_MSG_NONE,
    PKG_MSG_INFO,
    PKG_MSG_WARNING,
    PKG_MSG_ERROR
} pkg_msg_kind_t;

typedef struct {
    int busy;
    int percent;
    int extracting;
    char title[160];
    pkg_msg_kind_t message_kind;
    char message[400];
    unsigned long long revision;
} pkg_status_t;

void package_manager_init(void);
void package_manager_shutdown(void);

const char *package_platform_name(int platform);
int package_platform_arch_count(int platform);
const char *package_arch_name(int platform, int arch);
int package_platform_has_archs(int platform);

const char *package_host_text(void);
pkg_state_t package_host_compiler_state(void);
int package_host_supported(void);

pkg_state_t package_arch_state(int platform, int arch);
pkg_state_t package_platform_state(int platform);

int package_install_arch(int platform, int arch);
int package_install_platform(int platform);
int package_install_host_compiler(void);
int package_uninstall_arch(int platform, int arch);
int package_uninstall_platform(int platform);

void package_refresh_states(void);
unsigned long long package_states_revision(void);
void package_get_status(pkg_status_t *out);
int package_is_busy(void);

#endif
