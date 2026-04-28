#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include "ivault.h"

typedef struct {
    char target_root[PATH_MAX];
    char vault_path[PATH_MAX];
    char quarantine_root[PATH_MAX];
    IvaultManifest manifest;
    unsigned long kept;
    unsigned long quarantined;
    unsigned long deleted;
    unsigned long errors;
    int delete_mode;
} PruneContext;

static const char *relative_path(PruneContext *ctx, const char *path) {
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

static int is_inside_quarantine(PruneContext *ctx, const char *path) {
    if (ctx->quarantine_root[0] == '\0') return 0;
    size_t n = strlen(ctx->quarantine_root);
    return strncmp(path, ctx->quarantine_root, n) == 0;
}

static int quarantine_file(PruneContext *ctx, const char *path, const char *rel) {
    char dst[PATH_MAX];
    if (ivault_path_join(ctx->quarantine_root, rel, dst, sizeof(dst)) != 0) {
        printf("[PRUNE] ERROR      rel=\"%s\" reason=\"quarantine_path_too_long\"\n", rel);
        ctx->errors++;
        return 1;
    }

    if (ivault_mkdir_parent_for_file(dst) != 0) {
        printf("[PRUNE] ERROR      rel=\"%s\" reason=\"mkdir_parent_failed\"\n", rel);
        ctx->errors++;
        return 1;
    }

    if (rename(path, dst) == 0) {
        printf("[PRUNE] QUARANTINE rel=\"%s\" to=\"%s\"\n", rel, dst);
        ctx->quarantined++;
        return 0;
    }

    if (ivault_copy_file(path, dst) == 0 && unlink(path) == 0) {
        printf("[PRUNE] QUARANTINE rel=\"%s\" to=\"%s\"\n", rel, dst);
        ctx->quarantined++;
        return 0;
    }

    printf("[PRUNE] ERROR      rel=\"%s\" reason=\"quarantine_failed\"\n", rel);
    ctx->errors++;
    return 1;
}

static void prune_file(PruneContext *ctx, const char *path, const struct stat *st) {
    (void)st;
    const char *rel = relative_path(ctx, path);

    if (ivault_manifest_find(&ctx->manifest, rel)) {
        printf("[PRUNE] KEEP       rel=\"%s\"\n", rel);
        ctx->kept++;
        return;
    }

    if (ctx->delete_mode) {
        if (unlink(path) == 0) {
            printf("[PRUNE] DELETE     rel=\"%s\"\n", rel);
            ctx->deleted++;
        } else {
            printf("[PRUNE] ERROR      rel=\"%s\" reason=\"delete_failed\"\n", rel);
            ctx->errors++;
        }
        return;
    }

    quarantine_file(ctx, path, rel);
}

static int walk_prune(PruneContext *ctx, const char *path) {
    if (is_inside_quarantine(ctx, path)) return 0;

    struct stat st;
    if (lstat(path, &st) != 0) {
        ivault_log("ERROR", "lstat failed: %s: %s", path, strerror(errno));
        ctx->errors++;
        return 1;
    }

    if (S_ISLNK(st.st_mode)) return 0;

    if (S_ISREG(st.st_mode)) {
        prune_file(ctx, path, &st);
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
        walk_prune(ctx, child);
    }

    closedir(dir);
    return 0;
}

int ivault_prune_path(const char *target_path, const char *vault_path, int delete_mode) {
    if (!target_path || !vault_path) {
        ivault_log("ERROR", "prune requires target and vault");
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

    PruneContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    snprintf(ctx.target_root, sizeof(ctx.target_root), "%s", resolved_target);
    snprintf(ctx.vault_path, sizeof(ctx.vault_path), "%s", vault_path);
    ctx.delete_mode = delete_mode;

    if (ivault_manifest_load(vault_path, &ctx.manifest) != 0) return 1;

    if (!delete_mode) {
        time_t now = time(NULL);
        struct tm tm_buf;
        localtime_r(&now, &tm_buf);
        char stamp[64];
        strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tm_buf);

        char state_dir[PATH_MAX];
        if (ivault_path_join("state", "quarantine", state_dir, sizeof(state_dir)) != 0) {
            ivault_manifest_free(&ctx.manifest);
            return 1;
        }
        if (ivault_path_join(state_dir, stamp, ctx.quarantine_root, sizeof(ctx.quarantine_root)) != 0) {
            ivault_manifest_free(&ctx.manifest);
            return 1;
        }
        if (ivault_mkdir_p(ctx.quarantine_root) != 0) {
            ivault_log("ERROR", "could not create quarantine: %s", ctx.quarantine_root);
            ivault_manifest_free(&ctx.manifest);
            return 1;
        }
    }

    printf("ivault prune\n");
    printf("target: %s\n", ctx.target_root);
    printf("vault:  %s\n", vault_path);
    printf("mode:   %s\n", delete_mode ? "delete" : "quarantine");
    if (!delete_mode) printf("quarantine: %s\n", ctx.quarantine_root);
    printf("\n");

    walk_prune(&ctx, ctx.target_root);

    printf("\nIVAULT PRUNE REPORT\n");
    printf("EXPECTED: %zu\n", ctx.manifest.count);
    printf("KEPT: %lu\n", ctx.kept);
    printf("QUARANTINED: %lu\n", ctx.quarantined);
    printf("DELETED: %lu\n", ctx.deleted);
    printf("ERRORS: %lu\n", ctx.errors);
    printf("STATUS: %s\n", ctx.errors == 0 ? "PRUNE COMPLETE" : "PRUNE HAD ERRORS");

    ivault_manifest_free(&ctx.manifest);
    return ctx.errors == 0 ? 0 : 2;
}
