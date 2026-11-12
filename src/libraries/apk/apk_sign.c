#include "libraries/apk/apk_sign.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#include "mbedtls/rsa.h"
#include "mbedtls/sha256.h"
#include "mbedtls/x509_crt.h"

#include "filesystem/rebax_fs.h"

#define KEY_FILE "rebax_debug_key.pem"
#define CERT_FILE "rebax_debug_cert.der"
#define V2_BLOCK_ID 0x7109871au
#define ALGO_RSA_PKCS1_SHA256 0x0103u

struct apk_key {
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    unsigned char *cert;
    size_t cert_len;
};

static void set_err(char *err, size_t cap, const char *msg) {
    if (err && cap) snprintf(err, cap, "%s", msg);
}

static int read_file(const char *path, unsigned char **data, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return 0; }
    *data = (unsigned char *)malloc((size_t)n);
    if (!*data) { fclose(f); return 0; }
    if (fread(*data, 1, (size_t)n, f) != (size_t)n) { free(*data); *data = NULL; fclose(f); return 0; }
    fclose(f);
    *len = (size_t)n;
    return 1;
}

static int write_file(const char *path, const void *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = fwrite(data, 1, len, f) == len;
    return fclose(f) == 0 && ok;
}

static int create_key(apk_key_t *k, const char *key_path, const char *cert_path, char *err, size_t cap) {
    printf("Creating the APK signing key (only the first time)\n");
    fflush(stdout);
    const mbedtls_pk_info_t *info = mbedtls_pk_info_from_type(MBEDTLS_PK_RSA);
    if (mbedtls_pk_setup(&k->pk, info) != 0) { set_err(err, cap, "cannot prepare the signing key"); return 0; }
    if (mbedtls_rsa_gen_key(mbedtls_pk_rsa(k->pk), mbedtls_ctr_drbg_random, &k->drbg, 2048, 65537) != 0) {
        set_err(err, cap, "cannot generate the signing key");
        return 0;
    }
    unsigned char pem[4096];
    memset(pem, 0, sizeof(pem));
    if (mbedtls_pk_write_key_pem(&k->pk, pem, sizeof(pem)) != 0) { set_err(err, cap, "cannot encode the signing key"); return 0; }

    mbedtls_x509write_cert crt;
    mbedtls_x509write_crt_init(&crt);
    unsigned char serial[1] = {1};
    unsigned char *der = (unsigned char *)malloc(4096);
    int rc = -1;
    int der_len = 0;
    if (der) {
        mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
        mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
        mbedtls_x509write_crt_set_subject_key(&crt, &k->pk);
        mbedtls_x509write_crt_set_issuer_key(&crt, &k->pk);
        rc = mbedtls_x509write_crt_set_subject_name(&crt, "CN=Rebax,O=Rebax,C=US");
        if (rc == 0) rc = mbedtls_x509write_crt_set_issuer_name(&crt, "CN=Rebax,O=Rebax,C=US");
        if (rc == 0) rc = mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof(serial));
        if (rc == 0) rc = mbedtls_x509write_crt_set_validity(&crt, "20240101000000", "20640101000000");
        if (rc == 0) {
            der_len = mbedtls_x509write_crt_der(&crt, der, 4096, mbedtls_ctr_drbg_random, &k->drbg);
            rc = der_len > 0 ? 0 : -1;
        }
    }
    mbedtls_x509write_crt_free(&crt);
    if (rc != 0) { free(der); set_err(err, cap, "cannot create the signing certificate"); return 0; }
    k->cert = (unsigned char *)malloc((size_t)der_len);
    if (!k->cert) { free(der); return 0; }
    memcpy(k->cert, der + 4096 - der_len, (size_t)der_len);
    k->cert_len = (size_t)der_len;
    free(der);

    if (!write_file(key_path, pem, strlen((char *)pem)) || !write_file(cert_path, k->cert, k->cert_len)) {
        set_err(err, cap, "cannot save the signing key");
        return 0;
    }
    return 1;
}

apk_key_t *apk_key_open(const char *dir, char *err, size_t err_cap) {
    apk_key_t *k = (apk_key_t *)calloc(1, sizeof(*k));
    if (!k) return NULL;
    mbedtls_pk_init(&k->pk);
    mbedtls_entropy_init(&k->entropy);
    mbedtls_ctr_drbg_init(&k->drbg);
    if (mbedtls_ctr_drbg_seed(&k->drbg, mbedtls_entropy_func, &k->entropy, (const unsigned char *)"rebax", 5) != 0) {
        set_err(err, err_cap, "no random source is available");
        apk_key_close(k);
        return NULL;
    }
    char key_path[1024], cert_path[1024];
    snprintf(key_path, sizeof(key_path), "%s/%s", dir, KEY_FILE);
    snprintf(cert_path, sizeof(cert_path), "%s/%s", dir, CERT_FILE);
    if (rebax_fs_exists(key_path) && rebax_fs_exists(cert_path)) {
        if (mbedtls_pk_parse_keyfile(&k->pk, key_path, NULL, mbedtls_ctr_drbg_random, &k->drbg) == 0 && read_file(cert_path, &k->cert, &k->cert_len)) return k;
        set_err(err, err_cap, "the saved signing key cannot be read; delete the keystore folder to create a new one");
        apk_key_close(k);
        return NULL;
    }
    rebax_fs_mkdir_p(dir);
    if (!create_key(k, key_path, cert_path, err, err_cap)) {
        apk_key_close(k);
        return NULL;
    }
    return k;
}

void apk_key_close(apk_key_t *k) {
    if (!k) return;
    mbedtls_pk_free(&k->pk);
    mbedtls_ctr_drbg_free(&k->drbg);
    mbedtls_entropy_free(&k->entropy);
    free(k->cert);
    free(k);
}

static void put_prefixed(apk_buf_t *b, const apk_buf_t *inner) {
    apk_buf_u32(b, (uint32_t)inner->size);
    apk_buf_put(b, inner->data, inner->size);
}

int apk_sign_v2_block(apk_key_t *k, const unsigned char top_digest[32], apk_buf_t *block, char *err, size_t err_cap) {
    apk_buf_t digest_entry, signed_data, sig_entry, sigs, pub, signer, signers, value;
    apk_buf_t *all[] = {&digest_entry, &signed_data, &sig_entry, &sigs, &pub, &signer, &signers, &value};
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) apk_buf_init(all[i]);

    apk_buf_u32(&digest_entry, ALGO_RSA_PKCS1_SHA256);
    apk_buf_u32(&digest_entry, 32);
    apk_buf_put(&digest_entry, top_digest, 32);

    {
        apk_buf_t d, c;
        apk_buf_init(&d);
        apk_buf_init(&c);
        put_prefixed(&d, &digest_entry);
        apk_buf_u32(&c, (uint32_t)k->cert_len);
        apk_buf_put(&c, k->cert, k->cert_len);
        apk_buf_u32(&signed_data, (uint32_t)d.size);
        apk_buf_put(&signed_data, d.data, d.size);
        apk_buf_u32(&signed_data, (uint32_t)c.size);
        apk_buf_put(&signed_data, c.data, c.size);
        apk_buf_u32(&signed_data, 0);
        apk_buf_free(&d);
        apk_buf_free(&c);
    }

    unsigned char hash[32];
    mbedtls_sha256(signed_data.data, signed_data.size, hash, 0);
    unsigned char sig[512];
    size_t sig_len = 0;
    if (mbedtls_pk_sign(&k->pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), sig, sizeof(sig), &sig_len, mbedtls_ctr_drbg_random, &k->drbg) != 0) {
        set_err(err, err_cap, "signing failed");
        for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) apk_buf_free(all[i]);
        return 0;
    }
    apk_buf_u32(&sig_entry, ALGO_RSA_PKCS1_SHA256);
    apk_buf_u32(&sig_entry, (uint32_t)sig_len);
    apk_buf_put(&sig_entry, sig, sig_len);
    put_prefixed(&sigs, &sig_entry);

    unsigned char pubder[1024];
    int pub_len = mbedtls_pk_write_pubkey_der(&k->pk, pubder, sizeof(pubder));
    if (pub_len <= 0) {
        set_err(err, err_cap, "cannot encode the public key");
        for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) apk_buf_free(all[i]);
        return 0;
    }
    apk_buf_put(&pub, pubder + sizeof(pubder) - pub_len, (size_t)pub_len);

    put_prefixed(&signer, &signed_data);
    put_prefixed(&signer, &sigs);
    put_prefixed(&signer, &pub);
    put_prefixed(&signers, &signer);
    put_prefixed(&value, &signers);

    uint64_t pair_len = 4 + (uint64_t)value.size;
    uint64_t block_size = 8 + pair_len + 8 + 16;
    apk_buf_u64(block, block_size);
    apk_buf_u64(block, pair_len);
    apk_buf_u32(block, V2_BLOCK_ID);
    apk_buf_put(block, value.data, value.size);
    apk_buf_u64(block, block_size);
    apk_buf_put(block, "APK Sig Block 42", 16);

    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) apk_buf_free(all[i]);
    return !block->failed;
}
