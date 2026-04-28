#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include "ivault.h"

typedef struct {
    char target_root[PATH_MAX];
    char vault_root[PATH_MAX];
    char objects_dir[PATH_MAX];
    FILE *manifest;
    unsigned long files_sealed;
    unsigned long dirs_seen;
    unsigned long skipped;
    unsigned long errors;
} SealContext;

static int has_suffix(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t a = strlen(str);
    size_t b = strlen(suffix);
    return b <= a && strcmp(str + a - b, suffix) == 0;
}

static int should_skip_name(const char *name) {
    if (!name) return 1;
    if (strcmp(name, ".") == 0) return 1;
    if (strcmp(name, "..") == 0) return 1;
    if (strcmp(name, ".imeta") == 0) return 1;
    if (has_suffix(name, ".tmp")) return 1;
    if (has_suffix(name, ".swp")) return 1;
    if (has_suffix(name, "~")) return 1;
    return 0;
}

static int make_timestamp(char *out, size_t out_size) {
    time_t now = time(NULL);
    struct tm tm_now;

    if (!localtime_r(&now, &tm_now)) {
        return 1;
    }

    if (strftime(out, out_size, "%Y%m%d-%H%M%S", &tm_now) == 0) {
        return 1;
    }

    return 0;
}

static const char *relative_path(SealContext *ctx, const char *path) {
    size_t root_len = strlen(ctx->target_root);

    if (strncmp(path, ctx->target_root, root_len) == 0) {
        const char *rel = path + root_len;
        if (*rel == '/') rel++;
        if (*rel == '\0') return ".";
        return rel;
    }

    return path;
}

static int object_path_for_hash_seal(SealContext *ctx, const char *hash, char *out, size_t out_size) {
    if (!hash || strlen(hash) < 2) return 1;

    char shard_dir[PATH_MAX];
    int written = snprintf(
        shard_dir,
        sizeof(shard_dir),
        "%s/%c%c",
        ctx->objects_dir,
        hash[0],
        hash[1]
    );

    if (written < 0 || (size_t)written >= sizeof(shard_dir)) {
        return 1;
    }

    if (ivault_mkdir_p(shard_dir) != 0) {
        return 1;
    }

    written = snprintf(out, out_size, "%s/%s.blob", shard_dir, hash);

    return (written < 0 || (size_t)written >= out_size) ? 1 : 0;
}

static int seal_file(SealContext *ctx, const char *path, const struct stat *st) {
    char hash[IVAULT_HASH_HEX_SIZE];

    if (ivault_hash_file_fnv1a64(path, hash, sizeof(hash)) != 0) {
        ivault_log("ERROR", "hash failed: %s", path);
        ctx->errors++;
        return 1;
    }

    char object_path[PATH_MAX];

    if (object_path_for_hash_seal(ctx, hash, object_path, sizeof(object_path)) != 0) {
        ivault_log("ERROR", "could not create object path for hash: %s", hash);
        ctx->errors++;
        return 1;
    }

    if (!ivault_is_regular_file(object_path)) {
        if (ivault_copy_file(path, object_path) != 0) {
            ivault_log("ERROR", "copy failed: %s -> %s", path, object_path);
            ctx->errors++;
            return 1;
        }

        printf("[SEAL] OBJECT  hash=%s path=\"%s\"\n", hash, object_path);
    } else {
        printf("[SEAL] REUSE   hash=%s path=\"%s\"\n", hash, object_path);
    }

    const char *rel = relative_path(ctx, path);

    fprintf(
        ctx->manifest,
        "FILE\t%s\t%lld\t%lld\t%lld\t%s\n",
        rel,
        (long long)st->st_size,
        (long long)st->st_ino,
        (long long)st->st_mtime,
        hash
    );

    printf(
        "[SEAL] FILE    rel=\"%s\" size=%lld inode=%lld mtime=%lld hash=%s\n",
        rel,
        (long long)st->st_size,
        (long long)st->st_ino,
        (long long)st->st_mtime,
        hash
    );

    ctx->files_sealed++;
    return 0;
}

static int walk_path(SealContext *ctx, const char *path) {
    struct stat st;

    if (lstat(path, &st) != 0) {
        ivault_log("ERROR", "lstat failed: %s: %s", path, strerror(errno));
        ctx->errors++;
        return 1;
    }

    if (S_ISLNK(st.st_mode)) {
        printf("[SEAL] SKIPLINK path=\"%s\"\n", path);
        ctx->skipped++;
        return 0;
    }

    if (S_ISREG(st.st_mode)) {
        return seal_file(ctx, path, &st);
    }

    if (!S_ISDIR(st.st_mode)) {
        printf("[SEAL] SKIPOTHER path=\"%s\"\n", path);
        ctx->skipped++;
        return 0;
    }

    ctx->dirs_seen++;

    DIR *dir = opendir(path);
    if (!dir) {
        ivault_log("ERROR", "opendir failed: %s: %s", path, strerror(errno));
        ctx->errors++;
        return 1;
    }

    printf("[SEAL] DIR     path=\"%s\"\n", path);

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (should_skip_name(entry->d_name)) {
            continue;
        }

        char child[PATH_MAX];

        if (ivault_path_join(path, entry->d_name, child, sizeof(child)) != 0) {
            ivault_log("ERROR", "path too long under: %s", path);
            ctx->errors++;
            continue;
        }

        walk_path(ctx, child);
    }

    closedir(dir);
    return 0;
}

int ivault_seal_path(const char *target_path) {
    if (!target_path || strlen(target_path) == 0) {
        ivault_log("ERROR", "no target path provided");
        return 1;
    }

    char resolved_target[PATH_MAX];

    if (realpath(target_path, resolved_target) == NULL) {
        ivault_log("ERROR", "target does not exist: %s", target_path);
        return 1;
    }

    if (!ivault_is_directory(resolved_target)) {
        ivault_log("ERROR", "target is not a directory: %s", resolved_target);
        return 1;
    }

    SealContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    snprintf(ctx.target_root, sizeof(ctx.target_root), "%s", resolved_target);

    char timestamp[64];

    if (make_timestamp(timestamp, sizeof(timestamp)) != 0) {
        ivault_log("ERROR", "could not create timestamp");
        return 1;
    }

    int written = snprintf(ctx.vault_root, sizeof(ctx.vault_root), "vaults/%s", timestamp);
    if (written < 0 || (size_t)written >= sizeof(ctx.vault_root)) {
        ivault_log("ERROR", "vault path too long");
        return 1;
    }

    written = snprintf(ctx.objects_dir, sizeof(ctx.objects_dir), "%s/objects", ctx.vault_root);
    if (written < 0 || (size_t)written >= sizeof(ctx.objects_dir)) {
        ivault_log("ERROR", "objects path too long");
        return 1;
    }

    if (ivault_mkdir_p(ctx.objects_dir) != 0) {
        ivault_log("ERROR", "could not create objects dir: %s", ctx.objects_dir);
        return 1;
    }

    char manifest_path[PATH_MAX];
    if (ivault_path_join(ctx.vault_root, "manifest.tsv", manifest_path, sizeof(manifest_path)) != 0) {
        ivault_log("ERROR", "manifest path too long");
        return 1;
    }

    ctx.manifest = fopen(manifest_path, "w");
    if (!ctx.manifest) {
        ivault_log("ERROR", "could not open manifest: %s", manifest_path);
        return 1;
    }

    fprintf(ctx.manifest, "kind\trelpath\tsize\tinode\tmtime\thash\n");

    printf("ivault seal\n");
    printf("target: %s\n", ctx.target_root);
    printf("vault:  %s\n", ctx.vault_root);
    printf("manifest: %s\n\n", manifest_path);

    int result = walk_path(&ctx, ctx.target_root);

    if (fclose(ctx.manifest) != 0) {
        ivault_log("ERROR", "failed to close manifest cleanly");
        ctx.errors++;
    }

    printf("\nIVAULT SEAL REPORT\n");
    printf("TARGET: %s\n", ctx.target_root);
    printf("VAULT: %s\n", ctx.vault_root);
    printf("FILES SEALED: %lu\n", ctx.files_sealed);
    printf("DIRS SEEN: %lu\n", ctx.dirs_seen);
    printf("SKIPPED: %lu\n", ctx.skipped);
    printf("ERRORS: %lu\n", ctx.errors);

    if (result != 0 || ctx.errors > 0) {
        printf("STATUS: SEAL FAILED\n");
        return 1;
    }

    printf("STATUS: SEALED\n");
    return 0;
}
