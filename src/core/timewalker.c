#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include "ivault.h"

static int is_vault_dir(const char *path) {
    char manifest[PATH_MAX];
    if (ivault_path_join(path, "manifest.tsv", manifest, sizeof(manifest)) != 0) return 0;
    return ivault_is_regular_file(manifest);
}

static int newest_vault(char *out, size_t out_size) {
    DIR *dir = opendir("vaults");
    if (!dir) return 1;
    char best[PATH_MAX] = {0};
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char candidate[PATH_MAX];
        if (ivault_path_join("vaults", entry->d_name, candidate, sizeof(candidate)) != 0) continue;
        if (!ivault_is_directory(candidate)) continue;
        if (!is_vault_dir(candidate)) continue;
        if (best[0] == '\0' || strcmp(candidate, best) > 0) snprintf(best, sizeof(best), "%s", candidate);
    }
    closedir(dir);
    if (best[0] == '\0') return 1;
    snprintf(out, out_size, "%s", best);
    return 0;
}

int ivault_latest_print(void) {
    char latest[PATH_MAX];
    printf("ivault latest\n\n");
    if (newest_vault(latest, sizeof(latest)) != 0) {
        printf("No vaults found. Run: ./ivault seal <target-folder>\n");
        return 1;
    }
    printf("%s\n", latest);
    ivault_ledger_append("LATEST", "-", latest, "FOUND", "latest vault printed");
    return 0;
}

int ivault_inspect_vault(const char *vault_path) {
    IvaultManifest manifest;
    memset(&manifest, 0, sizeof(manifest));
    if (ivault_manifest_load(vault_path, &manifest) != 0) {
        ivault_ledger_append("INSPECT", "-", vault_path, "ERROR", "manifest load failed");
        return 1;
    }
    unsigned long long total_bytes = 0;
    unsigned long pdfs = 0, sidecars = 0, textish = 0, other = 0;
    for (size_t i = 0; i < manifest.count; i++) {
        total_bytes += (unsigned long long)manifest.entries[i].size;
        const char *p = manifest.entries[i].relpath;
        size_t n = strlen(p);
        if (n >= 4 && strcmp(p + n - 4, ".pdf") == 0) pdfs++;
        else if (n >= 6 && strcmp(p + n - 6, ".imeta") == 0) sidecars++;
        else if (n >= 4 && strcmp(p + n - 4, ".txt") == 0) textish++;
        else other++;
    }
    printf("ivault inspect\n");
    printf("vault: %s\n\n", vault_path);
    printf("FILES: %zu\n", manifest.count);
    printf("TOTAL BYTES: %llu\n", total_bytes);
    printf("PDFS: %lu\n", pdfs);
    printf("SIDECARS: %lu\n", sidecars);
    printf("TEXT: %lu\n", textish);
    printf("OTHER: %lu\n", other);
    printf("MANIFEST: %s/manifest.tsv\n", vault_path);
    printf("OBJECTS: %s/objects\n", vault_path);
    printf("STATUS: INSPECTED\n");
    ivault_manifest_free(&manifest);
    ivault_ledger_append("INSPECT", "-", vault_path, "INSPECTED", "vault summary printed");
    return 0;
}

int ivault_recover_file(const char *vault_path, const char *relpath, const char *output_path) {
    IvaultManifest manifest;
    memset(&manifest, 0, sizeof(manifest));
    if (ivault_manifest_load(vault_path, &manifest) != 0) {
        ivault_ledger_append("RECOVER_FILE", output_path, vault_path, "ERROR", "manifest load failed");
        return 1;
    }
    IvaultManifestEntry *entry = ivault_manifest_find(&manifest, relpath);
    if (!entry) {
        fprintf(stderr, "[IVAULT][ERROR] relpath not found in vault: %s\n", relpath);
        ivault_manifest_free(&manifest);
        ivault_ledger_append("RECOVER_FILE", output_path, vault_path, "MISSING", relpath);
        return 1;
    }
    char object_path[PATH_MAX];
    if (ivault_object_path_for_hash(vault_path, entry->hash, object_path, sizeof(object_path)) != 0 || !ivault_is_regular_file(object_path)) {
        fprintf(stderr, "[IVAULT][ERROR] vault object missing for hash: %s\n", entry->hash);
        ivault_manifest_free(&manifest);
        ivault_ledger_append("RECOVER_FILE", output_path, vault_path, "UNRECOVERABLE", relpath);
        return 1;
    }
    if (ivault_copy_file(object_path, output_path) != 0) {
        fprintf(stderr, "[IVAULT][ERROR] recover copy failed: %s -> %s\n", object_path, output_path);
        ivault_manifest_free(&manifest);
        ivault_ledger_append("RECOVER_FILE", output_path, vault_path, "ERROR", "copy failed");
        return 1;
    }
    printf("ivault recover-file\n");
    printf("vault:  %s\n", vault_path);
    printf("rel:    %s\n", relpath);
    printf("output: %s\n", output_path);
    printf("hash:   %s\n", entry->hash);
    printf("STATUS: RECOVERED\n");
    ivault_manifest_free(&manifest);
    ivault_ledger_append("RECOVER_FILE", output_path, vault_path, "RECOVERED", relpath);
    return 0;
}

int ivault_timeline_print(void) {
    DIR *dir = opendir("vaults");
    if (!dir) { printf("ivault timeline\n\nNo vaults directory found.\n"); return 1; }
    printf("ivault timeline\n");
    printf("vaults: vaults/\n\n");
    printf("vault\tfiles\tbytes\n");
    unsigned long vault_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char vault_path[PATH_MAX];
        if (ivault_path_join("vaults", entry->d_name, vault_path, sizeof(vault_path)) != 0) continue;
        if (!ivault_is_directory(vault_path) || !is_vault_dir(vault_path)) continue;
        IvaultManifest manifest;
        memset(&manifest, 0, sizeof(manifest));
        if (ivault_manifest_load(vault_path, &manifest) != 0) continue;
        unsigned long long total_bytes = 0;
        for (size_t i = 0; i < manifest.count; i++) total_bytes += (unsigned long long)manifest.entries[i].size;
        printf("%s\t%zu\t%llu\n", vault_path, manifest.count, total_bytes);
        ivault_manifest_free(&manifest);
        vault_count++;
    }
    closedir(dir);
    printf("\nTOTAL VAULTS: %lu\n", vault_count);
    printf("STATUS: TIMELINE PRINTED\n");
    ivault_ledger_append("TIMELINE", "-", "vaults", "PRINTED", "timeline printed");
    return vault_count > 0 ? 0 : 1;
}
