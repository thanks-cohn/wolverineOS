#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "ivault.h"

static int manifest_append(IvaultManifest *manifest, const IvaultManifestEntry *entry) {
    if (manifest->count >= manifest->capacity) {
        size_t new_capacity = manifest->capacity == 0 ? 128 : manifest->capacity * 2;

        IvaultManifestEntry *new_entries = realloc(
            manifest->entries,
            new_capacity * sizeof(IvaultManifestEntry)
        );

        if (!new_entries) {
            return 1;
        }

        manifest->entries = new_entries;
        manifest->capacity = new_capacity;
    }

    manifest->entries[manifest->count++] = *entry;
    return 0;
}

int ivault_manifest_load(const char *vault_path, IvaultManifest *manifest) {
    if (!vault_path || !manifest) return 1;

    memset(manifest, 0, sizeof(*manifest));

    char manifest_path[PATH_MAX];

    if (ivault_path_join(vault_path, "manifest.tsv", manifest_path, sizeof(manifest_path)) != 0) {
        return 1;
    }

    FILE *fp = fopen(manifest_path, "r");
    if (!fp) {
        ivault_log("ERROR", "could not open manifest: %s", manifest_path);
        return 1;
    }

    char line[8192];
    int line_no = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_no++;

        if (line_no == 1 && strncmp(line, "kind\t", 5) == 0) {
            continue;
        }

        char *kind = strtok(line, "\t");
        char *relpath = strtok(NULL, "\t");
        char *size_s = strtok(NULL, "\t");
        char *inode_s = strtok(NULL, "\t");
        char *mtime_s = strtok(NULL, "\t");
        char *hash_s = strtok(NULL, "\t\r\n");

        if (!kind || !relpath || !size_s || !inode_s || !mtime_s || !hash_s) {
            ivault_log("ERROR", "bad manifest line %d", line_no);
            fclose(fp);
            ivault_manifest_free(manifest);
            return 1;
        }

        if (strcmp(kind, "FILE") != 0) {
            continue;
        }

        IvaultManifestEntry entry;
        memset(&entry, 0, sizeof(entry));

        snprintf(entry.relpath, sizeof(entry.relpath), "%s", relpath);
        entry.size = atoll(size_s);
        entry.inode = atoll(inode_s);
        entry.mtime = atoll(mtime_s);
        snprintf(entry.hash, sizeof(entry.hash), "%s", hash_s);

        if (manifest_append(manifest, &entry) != 0) {
            fclose(fp);
            ivault_manifest_free(manifest);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

void ivault_manifest_free(IvaultManifest *manifest) {
    if (!manifest) return;

    free(manifest->entries);
    manifest->entries = NULL;
    manifest->count = 0;
    manifest->capacity = 0;
}

IvaultManifestEntry *ivault_manifest_find(IvaultManifest *manifest, const char *relpath) {
    if (!manifest || !relpath) return NULL;

    for (size_t i = 0; i < manifest->count; i++) {
        if (strcmp(manifest->entries[i].relpath, relpath) == 0) {
            return &manifest->entries[i];
        }
    }

    return NULL;
}
