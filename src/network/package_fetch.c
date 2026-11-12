#include "network/package_fetch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/file_hash.h"
#include "network/https_client.h"
#include "filesystem/rebax_fs.h"

static int read_whole(const char *path, char **out) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > 8 * 1024 * 1024) { fclose(f); return 0; }
    char *s = malloc((size_t)n + 1);
    if (!s) { fclose(f); return 0; }
    size_t got = fread(s, 1, (size_t)n, f);
    fclose(f);
    s[got] = '\0';
    *out = s;
    return 1;
}

static int find_entry(const char *json, const char *file_name, unsigned long long *size, char sha[65]) {
    char key[600];
    snprintf(key, sizeof(key), "\"file\": \"%s\"", file_name);
    const char *p = strstr(json, key);
    if (!p) {
        snprintf(key, sizeof(key), "\"file\":\"%s\"", file_name);
        p = strstr(json, key);
    }
    if (!p) return 0;
    const char *end = strchr(p, '}');
    const char *sz = strstr(p, "\"size\"");
    const char *sh = strstr(p, "\"sha256\"");
    if (!sz || !sh || (end && (sz > end || sh > end))) return 0;
    sz = strchr(sz + 6, ':');
    sh = strchr(sh + 8, '"');
    if (!sz || !sh) return 0;
    *size = strtoull(sz + 1, NULL, 10);
    sh++;
    size_t i = 0;
    while (i < 64 && sh[i] && sh[i] != '"') { sha[i] = sh[i]; i++; }
    sha[i] = '\0';
    return i == 64;
}

static void warn(const rebax_fetch_hooks_t *h, const char *text) {
    if (h && h->on_warning) h->on_warning(text, h->user);
}

int rebax_package_download(const char *base_url, const char *file_name, const char *work_dir, const char *dest_path, const rebax_fetch_hooks_t *hooks, char *error, unsigned long error_cap) {
    char url[2048], manifest[1200];
    if (error && error_cap) error[0] = '\0';
    if (!rebax_fs_mkdir_p(work_dir)) {
        if (error && error_cap) snprintf(error, error_cap, "cannot create the folder %s", work_dir);
        return 0;
    }
    snprintf(manifest, sizeof(manifest), "%s/.manifest.json", work_dir);

    unsigned long long want_size = 0;
    char want_sha[65];
    int have_entry = 0;
    snprintf(url, sizeof(url), "%s/manifest.json", base_url);
    char merr[256];
    if (rebax_https_download(url, manifest, NULL, NULL, hooks ? hooks->cancel : NULL, merr, sizeof(merr))) {
        char *json = NULL;
        if (read_whole(manifest, &json)) {
            have_entry = find_entry(json, file_name, &want_size, want_sha);
            free(json);
        }
        remove(manifest);
        if (!have_entry) warn(hooks, "the package list has no entry for this file, so it was not verified");
    } else {
        warn(hooks, "the package list could not be downloaded, so the file will not be verified");
    }

    snprintf(url, sizeof(url), "%s/%s", base_url, file_name);
    if (!rebax_https_download(url, dest_path, hooks ? hooks->on_download : NULL, hooks ? hooks->user : NULL, hooks ? hooks->cancel : NULL, error, error_cap)) {
        remove(dest_path);
        return 0;
    }

    if (have_entry) {
        char got_sha[65];
        unsigned long long got_size = 0;
        if (rebax_file_sha256(dest_path, got_sha, &got_size)) {
            if (got_size != want_size) warn(hooks, "the downloaded size differs from the package list");
            if (strcmp(got_sha, want_sha) != 0) warn(hooks, "the sha256 of the downloaded file differs from the package list");
        }
    }
    return 1;
}

int rebax_package_fetch(const char *base_url, const char *file_name, const char *work_dir, const char *extract_dir, const rebax_fetch_hooks_t *hooks, char *error, unsigned long error_cap) {
    char archive[1200];
    if (error && error_cap) error[0] = '\0';
    if (!rebax_fs_mkdir_p(extract_dir)) {
        if (error && error_cap) snprintf(error, error_cap, "cannot create the folder %s", extract_dir);
        return 0;
    }
    snprintf(archive, sizeof(archive), "%s/.download.tar.xz", work_dir);
    if (!rebax_package_download(base_url, file_name, work_dir, archive, hooks, error, error_cap)) return 0;
    int ok = rebax_archive_extract_tar_xz(archive, extract_dir, hooks ? hooks->on_extract : NULL, hooks ? hooks->user : NULL, error, error_cap);
    remove(archive);
    return ok;
}
