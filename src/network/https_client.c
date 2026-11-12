#define _POSIX_C_SOURCE 200809L

#include "network/https_client.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

#define URL_CAP 2048
#define MAX_REDIRECTS 8
#define IO_TIMEOUT_MS 30000
#define HEADER_CAP 16384

static char g_ca_file[1024];
static unsigned char *g_ca_pem;
static size_t g_ca_pem_len;

void rebax_https_set_ca_pem(const unsigned char *pem, unsigned long length) {
    free(g_ca_pem);
    g_ca_pem = NULL;
    g_ca_pem_len = 0;
    if (!pem || length == 0) return;
    g_ca_pem = malloc((size_t)length + 1);
    if (!g_ca_pem) return;
    memcpy(g_ca_pem, pem, (size_t)length);
    g_ca_pem[length] = '\0';
    g_ca_pem_len = (size_t)length + 1;
}

void rebax_https_set_ca_file(const char *path) {
    snprintf(g_ca_file, sizeof(g_ca_file), "%s", path ? path : "");
}

typedef struct {
    char host[256];
    char port[8];
    char path[URL_CAP];
} url_t;

typedef struct {
    mbedtls_net_context net;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt ca;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    int open;
} conn_t;

static void set_err(char *error, unsigned long cap, const char *fmt, const char *a) {
    if (error && cap && error[0] == '\0') snprintf(error, cap, fmt, a ? a : "");
}

static void set_tls_err(char *error, unsigned long cap, const char *what, int code) {
    if (error && cap && error[0] == '\0') {
        char buf[160];
        mbedtls_strerror(code, buf, sizeof(buf));
        snprintf(error, cap, "%s: %s", what, buf);
    }
}

static int parse_url(const char *url, url_t *u) {
    if (strncmp(url, "https://", 8) != 0) return 0;
    const char *p = url + 8;
    const char *slash = strchr(p, '/');
    size_t hl = slash ? (size_t)(slash - p) : strlen(p);
    const char *colon = memchr(p, ':', hl);
    size_t nl = colon ? (size_t)(colon - p) : hl;
    if (nl == 0 || nl >= sizeof(u->host)) return 0;
    memcpy(u->host, p, nl);
    u->host[nl] = '\0';
    if (colon) {
        size_t pl = hl - nl - 1;
        if (pl == 0 || pl >= sizeof(u->port)) return 0;
        memcpy(u->port, colon + 1, pl);
        u->port[pl] = '\0';
    } else {
        snprintf(u->port, sizeof(u->port), "443");
    }
    if (slash) {
        if (strlen(slash) >= sizeof(u->path)) return 0;
        snprintf(u->path, sizeof(u->path), "%s", slash);
    } else {
        snprintf(u->path, sizeof(u->path), "/");
    }
    return 1;
}

static void conn_free(conn_t *c) {
    if (c->open) mbedtls_ssl_close_notify(&c->ssl);
    mbedtls_net_free(&c->net);
    mbedtls_ssl_free(&c->ssl);
    mbedtls_ssl_config_free(&c->conf);
    mbedtls_x509_crt_free(&c->ca);
    mbedtls_ctr_drbg_free(&c->drbg);
    mbedtls_entropy_free(&c->entropy);
}

static int conn_open(conn_t *c, const url_t *u, char *error, unsigned long cap) {
    int rc;
    memset(c, 0, sizeof(*c));
    mbedtls_net_init(&c->net);
    mbedtls_ssl_init(&c->ssl);
    mbedtls_ssl_config_init(&c->conf);
    mbedtls_x509_crt_init(&c->ca);
    mbedtls_entropy_init(&c->entropy);
    mbedtls_ctr_drbg_init(&c->drbg);

    if (g_ca_file[0] == '\0' && !g_ca_pem) {
        set_err(error, cap, "the certificate list was not set%s", "");
        return 0;
    }
    rc = mbedtls_ctr_drbg_seed(&c->drbg, mbedtls_entropy_func, &c->entropy, (const unsigned char *)"rebax", 5);
    if (rc != 0) { set_tls_err(error, cap, "random generator", rc); return 0; }
    if (g_ca_pem) rc = mbedtls_x509_crt_parse(&c->ca, g_ca_pem, g_ca_pem_len);
    else rc = mbedtls_x509_crt_parse_file(&c->ca, g_ca_file);
    if (rc < 0) {
        if (error && cap && error[0] == '\0') snprintf(error, cap, "cannot read the certificate list%s", "");
        return 0;
    }
    rc = mbedtls_ssl_config_defaults(&c->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (rc != 0) { set_tls_err(error, cap, "tls setup", rc); return 0; }
    mbedtls_ssl_conf_authmode(&c->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&c->conf, &c->ca, NULL);
    mbedtls_ssl_conf_rng(&c->conf, mbedtls_ctr_drbg_random, &c->drbg);
    mbedtls_ssl_conf_read_timeout(&c->conf, IO_TIMEOUT_MS);
    rc = mbedtls_ssl_setup(&c->ssl, &c->conf);
    if (rc != 0) { set_tls_err(error, cap, "tls setup", rc); return 0; }
    rc = mbedtls_ssl_set_hostname(&c->ssl, u->host);
    if (rc != 0) { set_tls_err(error, cap, "tls hostname", rc); return 0; }
    rc = mbedtls_net_connect(&c->net, u->host, u->port, MBEDTLS_NET_PROTO_TCP);
    if (rc != 0) {
        if (error && cap && error[0] == '\0') snprintf(error, cap, "cannot connect to %s (no internet?)", u->host);
        return 0;
    }
    mbedtls_ssl_set_bio(&c->ssl, &c->net, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);
    while ((rc = mbedtls_ssl_handshake(&c->ssl)) != 0) {
        if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE) {
            if (rc == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
                uint32_t flags = mbedtls_ssl_get_verify_result(&c->ssl);
                char buf[256];
                mbedtls_x509_crt_verify_info(buf, sizeof(buf), "", flags);
                if (error && cap && error[0] == '\0') snprintf(error, cap, "the server certificate of %s was rejected: %s", u->host, buf);
            } else {
                set_tls_err(error, cap, "secure connection failed", rc);
            }
            return 0;
        }
    }
    c->open = 1;
    return 1;
}

static int conn_write(conn_t *c, const char *data, size_t len) {
    size_t off = 0;
    while (off < len) {
        int rc = mbedtls_ssl_write(&c->ssl, (const unsigned char *)data + off, len - off);
        if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (rc <= 0) return 0;
        off += (size_t)rc;
    }
    return 1;
}

static int conn_read(conn_t *c, unsigned char *buf, size_t len) {
    for (;;) {
        int rc = mbedtls_ssl_read(&c->ssl, buf, len);
        if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return 0;
        return rc;
    }
}

typedef struct {
    conn_t *c;
    unsigned char buf[16384];
    size_t pos;
    size_t fill;
    int eof;
    int failed;
} reader_t;

static int rd_byte(reader_t *r) {
    if (r->pos == r->fill) {
        if (r->eof) return -1;
        int n = conn_read(r->c, r->buf, sizeof(r->buf));
        if (n <= 0) {
            r->eof = 1;
            if (n < 0) r->failed = 1;
            return -1;
        }
        r->pos = 0;
        r->fill = (size_t)n;
    }
    return r->buf[r->pos++];
}

static size_t rd_block(reader_t *r, unsigned char *out, size_t want) {
    if (r->pos == r->fill) {
        if (r->eof) return 0;
        int n = conn_read(r->c, r->buf, sizeof(r->buf));
        if (n <= 0) {
            r->eof = 1;
            if (n < 0) r->failed = 1;
            return 0;
        }
        r->pos = 0;
        r->fill = (size_t)n;
    }
    size_t take = r->fill - r->pos;
    if (take > want) take = want;
    memcpy(out, r->buf + r->pos, take);
    r->pos += take;
    return take;
}

static int rd_line(reader_t *r, char *line, size_t cap) {
    size_t n = 0;
    for (;;) {
        int ch = rd_byte(r);
        if (ch < 0) return 0;
        if (ch == '\n') break;
        if (ch != '\r' && n + 1 < cap) line[n++] = (char)ch;
    }
    line[n] = '\0';
    return 1;
}

static int ieq_prefix(const char *s, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return 0;
        s++;
        prefix++;
    }
    return 1;
}

static void absolute_location(const url_t *base, const char *loc, char *out, size_t cap) {
    if (strncmp(loc, "https://", 8) == 0) snprintf(out, cap, "%s", loc);
    else if (loc[0] == '/') snprintf(out, cap, "https://%.255s%s%.7s%.1700s", base->host, strcmp(base->port, "443") ? ":" : "", strcmp(base->port, "443") ? base->port : "", loc);
    else out[0] = '\0';
}

int rebax_https_download(const char *url, const char *dest_path, rebax_progress_fn progress, void *user, const volatile int *cancel, char *error, unsigned long error_cap) {
    char current[URL_CAP];
    snprintf(current, sizeof(current), "%s", url);
    if (error && error_cap) error[0] = '\0';

    for (int hop = 0; hop <= MAX_REDIRECTS; hop++) {
        url_t u;
        if (!parse_url(current, &u)) {
            set_err(error, error_cap, "unsupported address: %s", current);
            return 0;
        }
        conn_t c;
        if (!conn_open(&c, &u, error, error_cap)) {
            conn_free(&c);
            return 0;
        }
        char req[URL_CAP * 2];
        snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Rebax\r\nAccept: */*\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n", u.path, u.host);
        if (!conn_write(&c, req, strlen(req))) {
            set_err(error, error_cap, "cannot send the request to %s", u.host);
            conn_free(&c);
            return 0;
        }
        reader_t *r = calloc(1, sizeof(*r));
        if (!r) {
            conn_free(&c);
            set_err(error, error_cap, "not enough memory%s", "");
            return 0;
        }
        r->c = &c;
        char line[HEADER_CAP];
        if (!rd_line(r, line, sizeof(line)) || strncmp(line, "HTTP/", 5) != 0) {
            set_err(error, error_cap, "bad answer from %s", u.host);
            free(r);
            conn_free(&c);
            return 0;
        }
        int status = 0;
        const char *sp = strchr(line, ' ');
        if (sp) status = atoi(sp + 1);
        long long length = -1;
        int chunked = 0;
        char location[URL_CAP];
        location[0] = '\0';
        for (;;) {
            if (!rd_line(r, line, sizeof(line))) {
                set_err(error, error_cap, "the connection to %s closed early", u.host);
                free(r);
                conn_free(&c);
                return 0;
            }
            if (line[0] == '\0') break;
            if (ieq_prefix(line, "content-length:")) length = atoll(line + 15);
            else if (ieq_prefix(line, "transfer-encoding:") && strstr(line + 18, "chunked")) chunked = 1;
            else if (ieq_prefix(line, "location:")) {
                const char *v = line + 9;
                while (*v == ' ') v++;
                snprintf(location, sizeof(location), "%.2047s", v);
            }
        }
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            char next[URL_CAP];
            absolute_location(&u, location, next, sizeof(next));
            free(r);
            conn_free(&c);
            if (!next[0]) {
                set_err(error, error_cap, "bad redirect from %s", u.host);
                return 0;
            }
            snprintf(current, sizeof(current), "%s", next);
            continue;
        }
        if (status != 200) {
            char msg[32];
            snprintf(msg, sizeof(msg), "%d", status);
            if (error && error_cap && error[0] == '\0') snprintf(error, error_cap, "the server answered %s for %s", msg, current);
            free(r);
            conn_free(&c);
            return 0;
        }
        FILE *f = fopen(dest_path, "wb");
        if (!f) {
            set_err(error, error_cap, "cannot write %s", dest_path);
            free(r);
            conn_free(&c);
            return 0;
        }
        unsigned char *buf = malloc(65536);
        unsigned long long done = 0, total = length > 0 ? (unsigned long long)length : 0;
        int ok = buf != NULL;
        if (ok && chunked) {
            for (;;) {
                if (!rd_line(r, line, sizeof(line))) { ok = 0; break; }
                unsigned long long chunk = strtoull(line, NULL, 16);
                if (chunk == 0) break;
                while (chunk > 0) {
                    size_t n = rd_block(r, buf, chunk < 65536 ? (size_t)chunk : 65536);
                    if (n == 0) { ok = 0; break; }
                    if (fwrite(buf, 1, n, f) != n) { ok = 0; break; }
                    chunk -= n;
                    done += n;
                    if (progress) progress(done, total, user);
                    if (cancel && *cancel) { ok = 0; set_err(error, error_cap, "cancelled%s", ""); break; }
                }
                if (!ok) break;
                if (!rd_line(r, line, sizeof(line))) { ok = 0; break; }
            }
        } else if (ok) {
            for (;;) {
                if (length >= 0 && done >= (unsigned long long)length) break;
                size_t want = 65536;
                if (length >= 0 && (unsigned long long)length - done < want) want = (size_t)((unsigned long long)length - done);
                size_t n = rd_block(r, buf, want);
                if (n == 0) {
                    if (length >= 0 || r->failed) ok = 0;
                    break;
                }
                if (fwrite(buf, 1, n, f) != n) { ok = 0; break; }
                done += n;
                if (progress) progress(done, total, user);
                if (cancel && *cancel) { ok = 0; set_err(error, error_cap, "cancelled%s", ""); break; }
            }
        }
        if (fclose(f) != 0) ok = 0;
        free(buf);
        free(r);
        conn_free(&c);
        if (!ok) {
            set_err(error, error_cap, "the download was interrupted: %s", current);
            remove(dest_path);
            return 0;
        }
        return 1;
    }
    set_err(error, error_cap, "too many redirects for %s", url);
    return 0;
}
