/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
 * |  _ <| |___| |_) / ___ \  /  \ 
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libraries/apk/apk_sign.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/md.h"
#include "mbedtls/pem.h"
#include "mbedtls/pk.h"
#include "mbedtls/rsa.h"
#include "mbedtls/sha256.h"
#include "mbedtls/x509_crt.h"

#include "filesystem/rebax_fs.h"

#define SIGN_FILE "rebax_signing.pem"
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

static int create_key(apk_key_t *k, const char *sign_path, char *err, size_t cap) {
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

    unsigned char *cert_pem = (unsigned char *)malloc(4096);
    size_t cert_pem_len = 0;
    if (!cert_pem || mbedtls_pem_write_buffer("-----BEGIN CERTIFICATE-----\n", "-----END CERTIFICATE-----\n", k->cert, k->cert_len, cert_pem, 4096, &cert_pem_len) != 0) {
        free(cert_pem);
        set_err(err, cap, "cannot encode the signing certificate");
        return 0;
    }
    size_t key_len = strlen((char *)pem);
    unsigned char *all = (unsigned char *)malloc(key_len + cert_pem_len);
    if (!all) { free(cert_pem); return 0; }
    memcpy(all, pem, key_len);
    memcpy(all + key_len, cert_pem, cert_pem_len);
    int saved = write_file(sign_path, all, key_len + cert_pem_len);
    free(all);
    free(cert_pem);
    if (!saved) {
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
    char sign_path[1024];
    snprintf(sign_path, sizeof(sign_path), "%s/%s", dir, SIGN_FILE);
    if (rebax_fs_exists(sign_path)) {
        mbedtls_x509_crt crt;
        mbedtls_x509_crt_init(&crt);
        int ok = mbedtls_pk_parse_keyfile(&k->pk, sign_path, NULL, mbedtls_ctr_drbg_random, &k->drbg) == 0 && mbedtls_x509_crt_parse_file(&crt, sign_path) == 0 && crt.raw.len > 0;
        if (ok) {
            k->cert = (unsigned char *)malloc(crt.raw.len);
            if (k->cert) {
                memcpy(k->cert, crt.raw.p, crt.raw.len);
                k->cert_len = crt.raw.len;
            } else {
                ok = 0;
            }
        }
        mbedtls_x509_crt_free(&crt);
        if (ok) return k;
        set_err(err, err_cap, "the signing file cannot be read; it must hold an unencrypted private key followed by its certificate in PEM format");
        apk_key_close(k);
        return NULL;
    }
    rebax_fs_mkdir_p(dir);
    if (!create_key(k, sign_path, err, err_cap)) {
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

static void put_prefixed(byte_buf_t *b, const byte_buf_t *inner) {
    byte_buf_u32(b, (uint32_t)inner->size);
    byte_buf_put(b, inner->data, inner->size);
}

int apk_sign_v2_block(apk_key_t *k, const unsigned char top_digest[32], byte_buf_t *block, char *err, size_t err_cap) {
    byte_buf_t digest_entry, signed_data, sig_entry, sigs, pub, signer, signers, value;
    byte_buf_t *all[] = {&digest_entry, &signed_data, &sig_entry, &sigs, &pub, &signer, &signers, &value};
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) byte_buf_init(all[i]);

    byte_buf_u32(&digest_entry, ALGO_RSA_PKCS1_SHA256);
    byte_buf_u32(&digest_entry, 32);
    byte_buf_put(&digest_entry, top_digest, 32);

    {
        byte_buf_t d, c;
        byte_buf_init(&d);
        byte_buf_init(&c);
        put_prefixed(&d, &digest_entry);
        byte_buf_u32(&c, (uint32_t)k->cert_len);
        byte_buf_put(&c, k->cert, k->cert_len);
        byte_buf_u32(&signed_data, (uint32_t)d.size);
        byte_buf_put(&signed_data, d.data, d.size);
        byte_buf_u32(&signed_data, (uint32_t)c.size);
        byte_buf_put(&signed_data, c.data, c.size);
        byte_buf_u32(&signed_data, 0);
        byte_buf_free(&d);
        byte_buf_free(&c);
    }

    unsigned char hash[32];
    mbedtls_sha256(signed_data.data, signed_data.size, hash, 0);
    unsigned char sig[512];
    size_t sig_len = 0;
    if (mbedtls_pk_sign(&k->pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), sig, sizeof(sig), &sig_len, mbedtls_ctr_drbg_random, &k->drbg) != 0) {
        set_err(err, err_cap, "signing failed");
        for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) byte_buf_free(all[i]);
        return 0;
    }
    byte_buf_u32(&sig_entry, ALGO_RSA_PKCS1_SHA256);
    byte_buf_u32(&sig_entry, (uint32_t)sig_len);
    byte_buf_put(&sig_entry, sig, sig_len);
    put_prefixed(&sigs, &sig_entry);

    unsigned char pubder[1024];
    int pub_len = mbedtls_pk_write_pubkey_der(&k->pk, pubder, sizeof(pubder));
    if (pub_len <= 0) {
        set_err(err, err_cap, "cannot encode the public key");
        for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) byte_buf_free(all[i]);
        return 0;
    }
    byte_buf_put(&pub, pubder + sizeof(pubder) - pub_len, (size_t)pub_len);

    put_prefixed(&signer, &signed_data);
    put_prefixed(&signer, &sigs);
    put_prefixed(&signer, &pub);
    put_prefixed(&signers, &signer);
    put_prefixed(&value, &signers);

    uint64_t pair_len = 4 + (uint64_t)value.size;
    uint64_t block_size = 8 + pair_len + 8 + 16;
    byte_buf_u64(block, block_size);
    byte_buf_u64(block, pair_len);
    byte_buf_u32(block, V2_BLOCK_ID);
    byte_buf_put(block, value.data, value.size);
    byte_buf_u64(block, block_size);
    byte_buf_put(block, "APK Sig Block 42", 16);

    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) byte_buf_free(all[i]);
    return !block->failed;
}
