#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include "ivault.h"

static int make_ledger_timestamp(char *out, size_t out_size) {
    time_t now = time(NULL);
    struct tm tm_buf;
    if (localtime_r(&now, &tm_buf) == NULL) return 1;
    if (strftime(out, out_size, "%Y-%m-%dT%H:%M:%S%z", &tm_buf) == 0) return 1;
    return 0;
}

static int ensure_state_dir(void) {
    return ivault_mkdir_p("state");
}

static const char *safe(const char *s) {
    return s ? s : "-";
}

int ivault_ledger_append(const char *event, const char *target, const char *vault, const char *status, const char *detail) {
    if (ensure_state_dir() != 0) return 1;

    FILE *f = fopen("state/ledger.tsv", "a");
    if (!f) return 1;

    char ts[64];
    if (make_ledger_timestamp(ts, sizeof(ts)) != 0) {
        fclose(f);
        return 1;
    }

    fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\n", ts, safe(event), safe(target), safe(vault), safe(status), safe(detail));
    fclose(f);
    return 0;
}

int ivault_history_print(void) {
    FILE *f = fopen("state/ledger.tsv", "r");
    if (!f) {
        printf("ivault history\n\n");
        printf("No ledger found yet. Run seal/verify/restore/prune first.\n");
        return 0;
    }

    printf("ivault history\n");
    printf("ledger: state/ledger.tsv\n\n");
    printf("time\tevent\ttarget\tvault\tstatus\tdetail\n");

    char line[8192];
    while (fgets(line, sizeof(line), f)) {
        fputs(line, stdout);
    }

    fclose(f);
    return 0;
}

int ivault_audit_print(void) {
    FILE *f = fopen("state/ledger.tsv", "r");
    if (!f) {
        printf("ivault audit\n\nNo ledger found yet.\n");
        return 0;
    }

    unsigned long total = 0;
    unsigned long seals = 0, verifies = 0, restores = 0, prunes = 0;
    unsigned long clean = 0, damage = 0, failed = 0;
    char last_line[8192] = {0};
    char line[8192];

    while (fgets(line, sizeof(line), f)) {
        total++;
        snprintf(last_line, sizeof(last_line), "%s", line);
        if (strstr(line, "\tSEAL\t")) seals++;
        if (strstr(line, "\tVERIFY\t")) verifies++;
        if (strstr(line, "\tRESTORE\t")) restores++;
        if (strstr(line, "\tPRUNE\t")) prunes++;
        if (strstr(line, "\tCLEAN\t") || strstr(line, "\tSEALED\t") || strstr(line, "\tRESTORE_COMPLETE\t") || strstr(line, "\tPRUNE_COMPLETE\t")) clean++;
        if (strstr(line, "DAMAGE") || strstr(line, "CORRUPTED") || strstr(line, "MISSING") || strstr(line, "EXTRA")) damage++;
        if (strstr(line, "FAILED") || strstr(line, "ERROR")) failed++;
    }

    fclose(f);

    printf("ivault audit\n");
    printf("ledger: state/ledger.tsv\n\n");
    printf("TOTAL EVENTS: %lu\n", total);
    printf("SEALS: %lu\n", seals);
    printf("VERIFIES: %lu\n", verifies);
    printf("RESTORES: %lu\n", restores);
    printf("PRUNES: %lu\n", prunes);
    printf("CLEAN/SUCCESS EVENTS: %lu\n", clean);
    printf("DAMAGE-RELATED EVENTS: %lu\n", damage);
    printf("FAILED/ERROR EVENTS: %lu\n", failed);
    if (last_line[0]) printf("LAST EVENT: %s", last_line);
    return 0;
}

static int manifest_contains_hash(IvaultManifest *m, const char *hash) {
    for (size_t i = 0; i < m->count; i++) {
        if (strcmp(m->entries[i].hash, hash) == 0) return 1;
    }
    return 0;
}

int ivault_diff_vaults(const char *vault_a, const char *vault_b) {
    IvaultManifest a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));

    if (ivault_manifest_load(vault_a, &a) != 0) return 1;
    if (ivault_manifest_load(vault_b, &b) != 0) {
        ivault_manifest_free(&a);
        return 1;
    }

    unsigned long same = 0, changed = 0, added = 0, removed = 0, renamed_or_moved = 0;

    printf("ivault diff\n");
    printf("vault A: %s\n", vault_a);
    printf("vault B: %s\n\n", vault_b);

    for (size_t i = 0; i < a.count; i++) {
        IvaultManifestEntry *be = ivault_manifest_find(&b, a.entries[i].relpath);
        if (!be) {
            if (manifest_contains_hash(&b, a.entries[i].hash)) {
                printf("[DIFF] MOVED/RENAMED old=\"%s\" hash=%s\n", a.entries[i].relpath, a.entries[i].hash);
                renamed_or_moved++;
            } else {
                printf("[DIFF] REMOVED rel=\"%s\" hash=%s\n", a.entries[i].relpath, a.entries[i].hash);
                removed++;
            }
            continue;
        }

        if (strcmp(a.entries[i].hash, be->hash) == 0 && a.entries[i].size == be->size) {
            same++;
        } else {
            printf("[DIFF] CHANGED rel=\"%s\" old=%s new=%s old_size=%lld new_size=%lld\n",
                   a.entries[i].relpath, a.entries[i].hash, be->hash, a.entries[i].size, be->size);
            changed++;
        }
    }

    for (size_t i = 0; i < b.count; i++) {
        IvaultManifestEntry *ae = ivault_manifest_find(&a, b.entries[i].relpath);
        if (!ae) {
            if (!manifest_contains_hash(&a, b.entries[i].hash)) {
                printf("[DIFF] ADDED rel=\"%s\" hash=%s\n", b.entries[i].relpath, b.entries[i].hash);
                added++;
            }
        }
    }

    printf("\nIVAULT DIFF REPORT\n");
    printf("A FILES: %zu\n", a.count);
    printf("B FILES: %zu\n", b.count);
    printf("SAME: %lu\n", same);
    printf("CHANGED: %lu\n", changed);
    printf("ADDED: %lu\n", added);
    printf("REMOVED: %lu\n", removed);
    printf("MOVED/RENAMED CANDIDATES: %lu\n", renamed_or_moved);
    printf("STATUS: %s\n", (changed == 0 && added == 0 && removed == 0 && renamed_or_moved == 0) ? "NO DIFFERENCE" : "DIFFERENCE DETECTED");

    char detail[256];
    snprintf(detail, sizeof(detail), "same=%lu changed=%lu added=%lu removed=%lu moved_or_renamed=%lu", same, changed, added, removed, renamed_or_moved);
    ivault_ledger_append("DIFF", vault_a, vault_b, (changed == 0 && added == 0 && removed == 0 && renamed_or_moved == 0) ? "NO_DIFFERENCE" : "DIFFERENCE_DETECTED", detail);

    ivault_manifest_free(&a);
    ivault_manifest_free(&b);
    return (changed == 0 && added == 0 && removed == 0 && renamed_or_moved == 0) ? 0 : 2;
}
