#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "ivault.h"

int ivault_is_regular_file(const char *path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return 0;
    }

    return S_ISREG(st.st_mode);
}

int ivault_is_directory(const char *path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return 0;
    }

    return S_ISDIR(st.st_mode);
}

int ivault_path_join(const char *a, const char *b, char *out, size_t out_size) {
    if (!a || !b || !out || out_size == 0) return 1;

    int written;

    if (a[0] == '\0') {
        written = snprintf(out, out_size, "%s", b);
    } else if (a[strlen(a) - 1] == '/') {
        written = snprintf(out, out_size, "%s%s", a, b);
    } else {
        written = snprintf(out, out_size, "%s/%s", a, b);
    }

    return (written < 0 || (size_t)written >= out_size) ? 1 : 0;
}

int ivault_mkdir_p(const char *path) {
    if (!path || !*path) return 1;

    char tmp[PATH_MAX];
    int written = snprintf(tmp, sizeof(tmp), "%s", path);

    if (written < 0 || (size_t)written >= sizeof(tmp)) {
        return 1;
    }

    size_t len = strlen(tmp);

    if (len == 0) return 1;

    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';

            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return 1;
            }

            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return 1;
    }

    return 0;
}

int ivault_mkdir_parent_for_file(const char *path) {
    if (!path || !*path) return 1;

    char tmp[PATH_MAX];
    int written = snprintf(tmp, sizeof(tmp), "%s", path);

    if (written < 0 || (size_t)written >= sizeof(tmp)) return 1;

    char *slash = strrchr(tmp, '/');
    if (!slash) return 0;

    *slash = '\0';

    if (tmp[0] == '\0') return 0;

    return ivault_mkdir_p(tmp);
}

int ivault_copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return 1;

    if (ivault_mkdir_parent_for_file(dst) != 0) {
        fclose(in);
        return 1;
    }

    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return 1;
    }

    unsigned char buffer[65536];

    while (1) {
        size_t n = fread(buffer, 1, sizeof(buffer), in);

        if (n > 0) {
            if (fwrite(buffer, 1, n, out) != n) {
                fclose(in);
                fclose(out);
                return 1;
            }
        }

        if (n < sizeof(buffer)) {
            if (ferror(in)) {
                fclose(in);
                fclose(out);
                return 1;
            }
            break;
        }
    }

    if (fclose(out) != 0) {
        fclose(in);
        return 1;
    }

    fclose(in);
    return 0;
}

int ivault_object_path_for_hash(const char *vault_path, const char *hash, char *out, size_t out_size) {
    if (!vault_path || !hash || strlen(hash) < 2 || !out) return 1;

    int written = snprintf(
        out,
        out_size,
        "%s/objects/%c%c/%s.blob",
        vault_path,
        hash[0],
        hash[1],
        hash
    );

    return (written < 0 || (size_t)written >= out_size) ? 1 : 0;
}
