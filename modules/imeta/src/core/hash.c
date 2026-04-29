/*
 * hash.c
 * Lightweight file hashing for imeta.
 *
 * Current hash:
 *   FNV-1a 64-bit
 *
 * Purpose:
 *   - fast file identity
 *   - good enough for early indexing/reconcile
 *   - later can be paired with SHA-256 for stronger identity
 *
 * Build with:
 *   gcc -Wall -Wextra -O2 -o imeta \
 *     src/cli/imeta.c src/core/scan.c src/core/index.c src/core/hash.c
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#define FNV1A_64_OFFSET 14695981039346656037ULL
#define FNV1A_64_PRIME  1099511628211ULL

int imeta_hash_file_fnv1a64(const char *path, char *out_hex, size_t out_hex_size) {
    if (!path || !out_hex || out_hex_size < 17) {
        fprintf(stderr, "[HASH][ERROR] invalid hash arguments\n");
        return 1;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "[HASH][ERROR] failed to open file: %s: %s\n", path, strerror(errno));
        return 1;
    }

    uint64_t hash = FNV1A_64_OFFSET;

    unsigned char buffer[8192];

    while (1) {
        size_t bytes_read = fread(buffer, 1, sizeof(buffer), fp);

        for (size_t i = 0; i < bytes_read; i++) {
            hash ^= (uint64_t)buffer[i];
            hash *= FNV1A_64_PRIME;
        }

        if (bytes_read < sizeof(buffer)) {
            if (ferror(fp)) {
                fprintf(stderr, "[HASH][ERROR] failed while reading file: %s\n", path);
                fclose(fp);
                return 1;
            }

            break;
        }
    }

    fclose(fp);

    snprintf(out_hex, out_hex_size, "%016llx", (unsigned long long)hash);

    fprintf(stderr, "[HASH] file=\"%s\" fnv1a64=%s\n", path, out_hex);

    return 0;
}
