/**
 * bench_crypto.c — Performance benchmarks for crypto operations
 */
#include "wireless_crypto.h"
#include <stdio.h>
#include <time.h>

#define ITERS 10000

int main(void) {
    printf("=== Crypto Performance Benchmarks (%d iterations) ===

", ITERS);
    clock_t start, end;
    aes_ctx_t aes;
    uint8_t key[16] = {0}, pt[16] = {0}, ct[16];

    /* AES-128 setup */
    start = clock();
    for (int i = 0; i < ITERS; i++)
        aes_key_setup(&aes, key, 128);
    end = clock();
    printf("AES-128 Key Setup:  %.2f us/op
",
           1000000.0 * (end - start) / CLOCKS_PER_SEC / ITERS);

    /* AES-128 encrypt */
    aes_key_setup(&aes, key, 128);
    start = clock();
    for (int i = 0; i < ITERS * 10; i++)
        aes_encrypt_block(&aes, pt, ct);
    end = clock();
    printf("AES-128 Encrypt:    %.3f us/op
",
           1000000.0 * (end - start) / CLOCKS_PER_SEC / (ITERS * 10));

    /* SHA-256 */
    uint8_t data[256], hash[32];
    for (int i = 0; i < 256; i++) data[i] = (uint8_t)i;
    start = clock();
    for (int i = 0; i < ITERS; i++)
        sha256_hash(data, 256, hash);
    end = clock();
    printf("SHA-256 (256B):     %.2f us/op
",
           1000000.0 * (end - start) / CLOCKS_PER_SEC / ITERS);

    /* HMAC-SHA256 */
    uint8_t hmac_key[32] = {0}, mac[32];
    start = clock();
    for (int i = 0; i < ITERS; i++)
        hmac_sha256(hmac_key, 32, data, 64, mac);
    end = clock();
    printf("HMAC-SHA256 (64B):  %.2f us/op
",
           1000000.0 * (end - start) / CLOCKS_PER_SEC / ITERS);

    /* PBKDF2-4096 */
    start = clock();
    pbkdf2_hmac_sha256((const uint8_t*)"bench", 5, (const uint8_t*)"salt", 4, 4096, hash, 32);
    end = clock();
    printf("PBKDF2 (4096 iter): %.2f ms/op
",
           1000.0 * (end - start) / CLOCKS_PER_SEC);

    printf("
=== Bench complete ===
");
    return 0;
}
