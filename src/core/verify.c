#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include "ivault.h"

typedef struct {
    char relpath[4096];
} SeenPath;

typedef struct {
    SeenPath *items;
    size_t count;
    size_t capacity;
} SeenSet;

typedef struct {
    char target_root[PATH_MAX];
    IvaultManifest manifest;
    SeenSet seen;
    unsigned long ok;
    unsigned long missing;
    unsigned long corrupted;
    unsigned long extra;
    unsigned long errors;
} VerifyContext;

static int seen_add(SeenSet *seen, const char *relpath) {
    if (seen->count >= seen->capacity) {
        size_t new_capacity = seen->capacity == 0 ? 128 : seen->capacity * 2;
        SeenPath *new_items = realloc(seen->items, new_capacity * sizeof(SeenPath));
        if (!new_items) return 1;
        seen->items = new_items;
        seen->capacity = new_capacity;
    }

    snprintf(seen->items[seen->count].relpath, sizeof(seen->items[seen->count].relpath), "%s", relpath);
    seen->count++;
    return 0;
}

static int seen_contains(SeenSet *seen, const char *relpath) {
    for (size_t i = 0; i < seen->count; i++) {
        if (strcmp(seen->items[i].relpath, relpath) == 0) return 1;
    }
    return 0;
}

static void seen_free(SeenSet *seen) {
    free(seen->items);
    seen->items = NULL;
    seen->count = 0;
    seen->capacity = 0;
}

static const char *relative_path(VerifyContext *ctx, const char *path) {
    size_t root_len = strlen(ctx->target_root);

    if (strncmp(path, ctx->target_root, root_len) == 0) {
        const char *rel = path + root_len;
        if (*rel == '/') rel++;
        if (*rel == '\0') return ".";
        return rel;
    }

    return path;
}

static int should_skip_name(const char *name) {
    if (!name) return 1;
    if (strcmp(name, ".") == 0) return 1;
    if (strcmp(name, "..") == 0) return 1;
    return 0;
}

static void verify_live_file(VerifyContext *ctx, const char *path, const struct stat *st) {
    const char *rel = relative_path(ctx, path);

    if (seen_add(&ctx->seen, rel) != 0) {
        ctx->errors++;
        return;
    }

    IvaultManifestEntry *entry = ivault_manifest_find(&ctx->manifest, rel);

    if (!entry) {
        printf("[VERIFY] EXTRA     rel=\"%s\" size=%lld\n", rel, (long long)st->st_size);
        ctx->extra++;
        return;
    }

    char hash[IVAULT_HASH_HEX_SIZE];

    if (ivault_hash_file_fnv1a64(path, hash, sizeof(hash)) != 0) {
        printf("[VERIFY] ERROR     rel=\"%s\" reason=\"hash_failed\"\n", rel);
        ctx->errors++;
        return;
    }

    if (strcmp(hash, entry->hash) == 0) {
        printf("[VERIFY] OK        rel=\"%s\" hash=%s\n", rel, hash);
        ctx->ok++;
    } else {
        printf(
            "[VERIFY] CORRUPTED rel=\"%s\" expected=%s actual=%s\n",
            rel,
            entry->hash,
            hash
        );
        ctx->corrupted++;
    }
}

static int walk_live(VerifyContext *ctx, const char *path) {
    struct stat st;

    if (lstat(path, &st) != 0) {
        ivault_log("ERROR", "lstat failed: %s: %s", path, strerror(errno));
        ctx->errors++;
        return 1;
    }

    if (S_ISLNK(st.st_mode)) return 0;

    if (S_ISREG(st.st_mode)) {
        verify_live_file(ctx, path, &st);
        return 0;
    }

    if (!S_ISDIR(st.st_mode)) return 0;

    DIR *dir = opendir(path);
    if (!dir) {
        ivault_log("ERROR", "opendir failed: %s: %s", path, strerror(errno));
        ctx->errors++;
        return 1;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (should_skip_name(entry->d_name)) continue;

        char child[PATH_MAX];

        if (ivault_path_join(path, entry->d_name, child, sizeof(child)) != 0) {
            ivault_log("ERROR", "path too long under: %s", path);
            ctx->errors++;
            continue;
        }

        walk_live(ctx, child);
    }

    closedir(dir);
    return 0;
}

int ivault_verify_path(const char *target_path, const char *vault_path) {
    if (!target_path || !vault_path) {
        ivault_log("ERROR", "verify requires target and vault");
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

    VerifyContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    snprintf(ctx.target_root, sizeof(ctx.target_root), "%s", resolved_target);

    if (ivault_manifest_load(vault_path, &ctx.manifest) != 0) {
        return 1;
    }

    printf("ivault verify\n");
    printf("target: %s\n", ctx.target_root);
    printf("vault:  %s\n\n", vault_path);

    walk_live(&ctx, ctx.target_root);

    for (size_t i = 0; i < ctx.manifest.count; i++) {
        if (!seen_contains(&ctx.seen, ctx.manifest.entries[i].relpath)) {
            printf("[VERIFY] MISSING   rel=\"%s\" expected=%s\n",
                   ctx.manifest.entries[i].relpath,
                   ctx.manifest.entries[i].hash);
            ctx.missing++;
        }
    }

    printf("\nIVAULT VERIFY REPORT\n");
    printf("EXPECTED: %zu\n", ctx.manifest.count);
    printf("OK: %lu\n", ctx.ok);
    printf("MISSING: %lu\n", ctx.missing);
    printf("CORRUPTED: %lu\n", ctx.corrupted);
    printf("EXTRA: %lu\n", ctx.extra);
    printf("ERRORS: %lu\n", ctx.errors);

    int clean = (ctx.missing == 0 && ctx.corrupted == 0 && ctx.extra == 0 && ctx.errors == 0);

    printf("STATUS: %s\n", clean ? "CLEAN" : "DAMAGE DETECTED");

    ivault_manifest_free(&ctx.manifest);
    seen_free(&ctx.seen);

    return clean ? 0 : 2;
}
