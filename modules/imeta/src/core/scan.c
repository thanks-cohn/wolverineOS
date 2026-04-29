/*
 * scan.c
 * Recursive filesystem scanner for imeta.
 *
 * Purpose:
 *   - walk a directory tree
 *   - log regular files
 *   - detect .imeta sidecars
 *   - collect basic stat info
 *   - compute lightweight file hashes
 *   - write results to .imeta/index.tsv through index.c
 *   - skip internal/build artifacts so the index stays clean
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

int imeta_index_open(const char *root_path);

int imeta_index_write_file(
    const char *kind,
    const char *path,
    long long size,
    long long inode,
    long long mtime,
    const char *hash
);

int imeta_index_close(void);

int imeta_hash_file_fnv1a64(
    const char *path,
    char *out_hex,
    size_t out_hex_size
);

typedef struct {
    unsigned long files_seen;
    unsigned long dirs_seen;
    unsigned long sidecars_seen;
    unsigned long links_skipped;
    unsigned long internal_skipped;
    unsigned long build_skipped;
    unsigned long other_skipped;
    unsigned long hash_errors;
    unsigned long errors_seen;
} ImetaScanStats;

static int has_suffix(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) return 0;

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static int is_build_artifact_name(const char *name) {
    if (!name) return 0;

    if (strcmp(name, "imeta") == 0) return 1;
    if (strcmp(name, "a.out") == 0) return 1;

    if (has_suffix(name, ".o")) return 1;
    if (has_suffix(name, ".so")) return 1;
    if (has_suffix(name, ".a")) return 1;
    if (has_suffix(name, ".dll")) return 1;
    if (has_suffix(name, ".exe")) return 1;
    if (has_suffix(name, ".tmp")) return 1;

    return 0;
}

static int should_skip_name(const char *name, ImetaScanStats *stats, const char *parent_path) {
    if (!name) return 1;

    if (strcmp(name, ".") == 0) return 1;
    if (strcmp(name, "..") == 0) return 1;

    if (strcmp(name, ".imeta") == 0) {
        if (stats) stats->internal_skipped++;
        printf("[SCAN] SKIPINT path=\"%s/.imeta\"\n", parent_path ? parent_path : ".");
        return 1;
    }

    if (is_build_artifact_name(name)) {
        if (stats) stats->build_skipped++;
        printf("[SCAN] SKIPBUILD path=\"%s/%s\"\n", parent_path ? parent_path : ".", name);
        return 1;
    }

    return 0;
}

static void print_file_report(
    const char *path,
    const struct stat *st,
    ImetaScanStats *stats
) {
    const char *kind = has_suffix(path, ".imeta") ? "SIDECAR" : "FILE";

    char hash_hex[32];
    memset(hash_hex, 0, sizeof(hash_hex));

    if (imeta_hash_file_fnv1a64(path, hash_hex, sizeof(hash_hex)) != 0) {
        snprintf(hash_hex, sizeof(hash_hex), "HASH_ERROR");
        stats->hash_errors++;
    }

    printf("[SCAN] %-7s path=\"%s\" size=%lld inode=%lld mtime=%lld hash=%s\n",
           kind,
           path,
           (long long)st->st_size,
           (long long)st->st_ino,
           (long long)st->st_mtime,
           hash_hex);

    if (imeta_index_write_file(
            kind,
            path,
            (long long)st->st_size,
            (long long)st->st_ino,
            (long long)st->st_mtime,
            hash_hex
        ) != 0) {
        fprintf(stderr, "[SCAN][ERROR] failed to write index row for: %s\n", path);
        stats->errors_seen++;
    }
}

static int scan_path_recursive(const char *path, ImetaScanStats *stats) {
    struct stat st;

    if (lstat(path, &st) != 0) {
        fprintf(stderr, "[SCAN][ERROR] lstat failed: %s: %s\n", path, strerror(errno));
        stats->errors_seen++;
        return 1;
    }

    if (S_ISLNK(st.st_mode)) {
        stats->links_skipped++;
        printf("[SCAN] SKIPLINK path=\"%s\"\n", path);
        return 0;
    }

    if (S_ISREG(st.st_mode)) {
        stats->files_seen++;

        if (has_suffix(path, ".imeta")) {
            stats->sidecars_seen++;
        }

        print_file_report(path, &st, stats);
        return 0;
    }

    if (!S_ISDIR(st.st_mode)) {
        stats->other_skipped++;
        printf("[SCAN] SKIPOTHER path=\"%s\"\n", path);
        return 0;
    }

    stats->dirs_seen++;

    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "[SCAN][ERROR] opendir failed: %s: %s\n", path, strerror(errno));
        stats->errors_seen++;
        return 1;
    }

    printf("[SCAN] DIR     path=\"%s\"\n", path);

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (should_skip_name(entry->d_name, stats, path)) {
            continue;
        }

        char child_path[PATH_MAX];

        int written = snprintf(
            child_path,
            sizeof(child_path),
            "%s/%s",
            path,
            entry->d_name
        );

        if (written < 0 || (size_t)written >= sizeof(child_path)) {
            fprintf(stderr, "[SCAN][ERROR] path too long under: %s\n", path);
            stats->errors_seen++;
            continue;
        }

        scan_path_recursive(child_path, stats);
    }

    closedir(dir);
    return 0;
}

int imeta_scan_path(const char *path) {
    if (!path || strlen(path) == 0) {
        fprintf(stderr, "[SCAN][ERROR] no path provided\n");
        return 1;
    }

    ImetaScanStats stats;
    memset(&stats, 0, sizeof(stats));

    printf("[SCAN] starting path=\"%s\"\n", path);

    if (imeta_index_open(path) != 0) {
        fprintf(stderr, "[SCAN][ERROR] could not open index\n");
        return 1;
    }

    int result = scan_path_recursive(path, &stats);

    if (imeta_index_close() != 0) {
        fprintf(stderr, "[SCAN][ERROR] failed to close index cleanly\n");
        stats.errors_seen++;
    }

    printf("\n[SCAN] summary\n");
    printf("[SCAN] files_seen=%lu\n", stats.files_seen);
    printf("[SCAN] dirs_seen=%lu\n", stats.dirs_seen);
    printf("[SCAN] sidecars_seen=%lu\n", stats.sidecars_seen);
    printf("[SCAN] links_skipped=%lu\n", stats.links_skipped);
    printf("[SCAN] internal_skipped=%lu\n", stats.internal_skipped);
    printf("[SCAN] build_skipped=%lu\n", stats.build_skipped);
    printf("[SCAN] other_skipped=%lu\n", stats.other_skipped);
    printf("[SCAN] hash_errors=%lu\n", stats.hash_errors);
    printf("[SCAN] errors_seen=%lu\n", stats.errors_seen);

    if (result != 0 || stats.errors_seen > 0) {
        printf("[SCAN] completed_with_errors\n");
        return 1;
    }

    printf("[SCAN] completed_ok\n");
    return 0;
}
