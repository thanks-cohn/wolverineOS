#include <stdio.h>
#include <stdint.h>
#include "ivault.h"

int ivault_hash_file_fnv1a64(
    const char *path,
    char *out_hex,
    size_t out_hex_size
) {
    if (!path || !out_hex || out_hex_size < IVAULT_HASH_HEX_SIZE) {
        return 1;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return 1;
    }

    uint64_t hash = 1469598103934665603ULL;
    const uint64_t prime = 1099511628211ULL;

    unsigned char buffer[8192];

    while (1) {
        size_t n = fread(buffer, 1, sizeof(buffer), fp);

        for (size_t i = 0; i < n; i++) {
            hash ^= (uint64_t)buffer[i];
            hash *= prime;
        }

        if (n < sizeof(buffer)) {
            if (ferror(fp)) {
                fclose(fp);
                return 1;
            }
            break;
        }
    }

    fclose(fp);
    snprintf(out_hex, out_hex_size, "%016llx", (unsigned long long)hash);
    return 0;
}
