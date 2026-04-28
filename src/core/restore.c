#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "ivault.h"

typedef struct {
    char target_root[PATH_MAX];
    char vault_root[PATH_MAX];
    IvaultManifest manifest;
    unsigned long restored_missing;
    unsigned long restored_corrupted;
    unsigned long already_ok;
    unsigned long unrecoverable;
    unsigned long errors;
} RestoreContext;

static int restore_entry(RestoreContext *ctx, IvaultManifestEntry *entry) {
    char live_path[PATH_MAX];
    char object_path[PATH_MAX];

    if (ivault_path_join(ctx->target_root, entry->relpath, live_path, sizeof(live_path)) != 0) {
        printf("[RESTORE] ERROR       rel=\"%s\" reason=\"live_path_too_long\"\n", entry->relpath);
        ctx->errors++;
        return 1;
    }

    if (ivault_object_path_for_hash(ctx->vault_root, entry->hash, object_path, sizeof(object_path)) != 0) {
        printf("[RESTORE] ERROR       rel=\"%s\" reason=\"object_path_too_long\"\n", entry->relpath);
        ctx->errors++;
        return 1;
    }

    if (!ivault_is_regular_file(object_path)) {
        printf("[RESTORE] UNRECOVERABLE rel=\"%s\" missing_object=\"%s\"\n", entry->relpath, object_path);
        ctx->unrecoverable++;
        return 1;
    }

    int exists = ivault_is_regular_file(live_path);
    int needs_restore = 0;
    int is_corrupt = 0;

    if (!exists) {
        needs_restore = 1;
    } else {
        char live_hash[IVAULT_HASH_HEX_SIZE];

        if (ivault_hash_file_fnv1a64(live_path, live_hash, sizeof(live_hash)) != 0) {
            needs_restore = 1;
            is_corrupt = 1;
        } else if (strcmp(live_hash, entry->hash) != 0) {
            needs_restore = 1;
            is_corrupt = 1;
        }
    }

    if (!needs_restore) {
        printf("[RESTORE] OK          rel=\"%s\"\n", entry->relpath);
        ctx->already_ok++;
        return 0;
    }

    if (ivault_copy_file(object_path, live_path) != 0) {
        printf("[RESTORE] ERROR       rel=\"%s\" reason=\"copy_failed\"\n", entry->relpath);
        ctx->errors++;
        return 1;
    }

    if (is_corrupt) {
        printf("[RESTORE] CORRUPTED   rel=\"%s\" action=\"restored\"\n", entry->relpath);
        ctx->restored_corrupted++;
    } else {
        printf("[RESTORE] MISSING     rel=\"%s\" action=\"restored\"\n", entry->relpath);
        ctx->restored_missing++;
    }

    return 0;
}

int ivault_restore_path(const char *target_path, const char *vault_path) {
    if (!target_path || !vault_path) {
        ivault_log("ERROR", "restore requires target and vault");
        return 1;
    }

    char resolved_target[PATH_MAX];

    if (realpath(target_path, resolved_target) == NULL) {
        if (ivault_mkdir_p(target_path) != 0) {
            ivault_log("ERROR", "target does not exist and could not be created: %s", target_path);
            return 1;
        }

        if (realpath(target_path, resolved_target) == NULL) {
            ivault_log("ERROR", "target could not be resolved after create: %s", target_path);
            return 1;
        }
    }

    RestoreContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    snprintf(ctx.target_root, sizeof(ctx.target_root), "%s", resolved_target);
    snprintf(ctx.vault_root, sizeof(ctx.vault_root), "%s", vault_path);

    if (ivault_manifest_load(vault_path, &ctx.manifest) != 0) {
        return 1;
    }

    printf("ivault restore\n");
    printf("target: %s\n", ctx.target_root);
    printf("vault:  %s\n\n", ctx.vault_root);

    for (size_t i = 0; i < ctx.manifest.count; i++) {
        restore_entry(&ctx, &ctx.manifest.entries[i]);
    }

    printf("\nIVAULT RESTORE REPORT\n");
    printf("EXPECTED: %zu\n", ctx.manifest.count);
    printf("ALREADY OK: %lu\n", ctx.already_ok);
    printf("RESTORED MISSING: %lu\n", ctx.restored_missing);
    printf("RESTORED CORRUPTED: %lu\n", ctx.restored_corrupted);
    printf("UNRECOVERABLE: %lu\n", ctx.unrecoverable);
    printf("ERRORS: %lu\n", ctx.errors);

    int ok = (ctx.unrecoverable == 0 && ctx.errors == 0);
    printf("STATUS: %s\n", ok ? "RESTORE COMPLETE" : "RESTORE INCOMPLETE");

    ivault_manifest_free(&ctx.manifest);
    return ok ? 0 : 1;
}
