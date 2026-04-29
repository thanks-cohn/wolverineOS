/*
 * reconcile.c
 * Heap-safe sidecar reconciliation for imeta.
 *
 * Reads:
 *   .imeta/index.tsv
 *
 * Reports:
 *   - paired assets
 *   - missing sidecars
 *   - orphan sidecars
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define IMETA_MAX_ROWS 4096
#define IMETA_MAX_PATH 4096

typedef struct {
    char kind[32];
    char path[IMETA_MAX_PATH];
    char hash[64];
} ImetaIndexRow;

typedef struct {
    ImetaIndexRow *rows;
    size_t count;
    size_t capacity;
} ImetaIndexTable;

static int has_suffix(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) return 0;

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static void strip_newline(char *s) {
    if (!s) return;

    size_t len = strlen(s);

    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

static void safe_copy(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;

    if (!src) {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s", src);
}

static int table_init(ImetaIndexTable *table, size_t capacity) {
    if (!table) return 1;

    table->rows = calloc(capacity, sizeof(ImetaIndexRow));

    if (!table->rows) {
        fprintf(stderr, "[RECONCILE][ERROR] calloc failed\n");
        return 1;
    }

    table->count = 0;
    table->capacity = capacity;

    return 0;
}

static void table_free(ImetaIndexTable *table) {
    if (!table) return;

    free(table->rows);
    table->rows = NULL;
    table->count = 0;
    table->capacity = 0;
}

static int load_index(ImetaIndexTable *table) {
    const char *index_path = ".imeta/index.tsv";

    fprintf(stderr, "[RECONCILE] loading index: %s\n", index_path);

    FILE *fp = fopen(index_path, "r");

    if (!fp) {
        fprintf(stderr, "[RECONCILE][ERROR] cannot open index: %s\n", strerror(errno));
        fprintf(stderr, "[RECONCILE][HINT] run: ./imeta scan . --verbose\n");
        return 1;
    }

    char line[8192];
    size_t line_number = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_number++;
        strip_newline(line);

        if (line_number == 1) continue;
        if (line[0] == '\0') continue;

        if (table->count >= table->capacity) {
            fprintf(stderr, "[RECONCILE][ERROR] row limit reached: %zu\n", table->capacity);
            fclose(fp);
            return 1;
        }

        char *kind = strtok(line, "\t");
        char *path = strtok(NULL, "\t");
        strtok(NULL, "\t"); /* size */
        strtok(NULL, "\t"); /* inode */
        strtok(NULL, "\t"); /* mtime */
        char *hash = strtok(NULL, "\t");

        if (!kind || !path) {
            fprintf(stderr, "[RECONCILE][WARN] malformed row at line %zu\n", line_number);
            continue;
        }

        ImetaIndexRow *row = &table->rows[table->count];

        safe_copy(row->kind, sizeof(row->kind), kind);
        safe_copy(row->path, sizeof(row->path), path);
        safe_copy(row->hash, sizeof(row->hash), hash ? hash : "");

        table->count++;
    }

    fclose(fp);

    fprintf(stderr, "[RECONCILE] loaded rows: %zu\n", table->count);

    return 0;
}

static int find_row_by_path(const ImetaIndexTable *table, const char *path) {
    if (!table || !path) return -1;

    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->rows[i].path, path) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static int find_asset_for_sidecar(const ImetaIndexTable *table, const char *sidecar_path) {
    if (!table || !sidecar_path) return -1;
    if (!has_suffix(sidecar_path, ".imeta")) return -1;

    size_t sidecar_len = strlen(sidecar_path);
    size_t suffix_len = strlen(".imeta");

    if (sidecar_len <= suffix_len) return -1;
    if (sidecar_len >= IMETA_MAX_PATH) return -1;

    char asset_path[IMETA_MAX_PATH];

    memcpy(asset_path, sidecar_path, sidecar_len + 1);
    asset_path[sidecar_len - suffix_len] = '\0';

    int idx = find_row_by_path(table, asset_path);

    if (idx >= 0 && strcmp(table->rows[idx].kind, "FILE") == 0) {
        return idx;
    }

    return -1;
}

int imeta_reconcile_path(const char *path) {
    fprintf(stderr, "[RECONCILE] starting reconciliation\n");
    fprintf(stderr, "[RECONCILE] target path: %s\n", path ? path : "(null)");

    ImetaIndexTable table;

    if (table_init(&table, IMETA_MAX_ROWS) != 0) {
        return 1;
    }

    if (load_index(&table) != 0) {
        table_free(&table);
        return 1;
    }

    unsigned long files = 0;
    unsigned long sidecars = 0;
    unsigned long paired = 0;
    unsigned long missing = 0;
    unsigned long orphan = 0;

    printf("\nReconciliation report\n");
    printf("---------------------\n");

    for (size_t i = 0; i < table.count; i++) {
        ImetaIndexRow *row = &table.rows[i];

        if (strcmp(row->kind, "FILE") != 0) continue;
        if (has_suffix(row->path, ".imeta")) continue;

        files++;

        char expected_sidecar[IMETA_MAX_PATH];

        int written = snprintf(
            expected_sidecar,
            sizeof(expected_sidecar),
            "%s.imeta",
            row->path
        );

        if (written < 0 || (size_t)written >= sizeof(expected_sidecar)) {
            fprintf(stderr, "[RECONCILE][WARN] expected sidecar path too long: %s\n", row->path);
            missing++;
            continue;
        }

        int sidecar_idx = find_row_by_path(&table, expected_sidecar);

        if (sidecar_idx >= 0 && strcmp(table.rows[sidecar_idx].kind, "SIDECAR") == 0) {
            paired++;
            printf("[PAIR] asset=\"%s\" sidecar=\"%s\"\n", row->path, expected_sidecar);
        } else {
            missing++;
            printf("[MISSING] asset=\"%s\" expected_sidecar=\"%s\"\n",
                   row->path,
                   expected_sidecar);
        }
    }

    for (size_t i = 0; i < table.count; i++) {
        ImetaIndexRow *row = &table.rows[i];

        if (strcmp(row->kind, "SIDECAR") != 0) continue;

        sidecars++;

        if (find_asset_for_sidecar(&table, row->path) < 0) {
            orphan++;
            printf("[ORPHAN] sidecar=\"%s\"\n", row->path);
        }
    }

    printf("\nSummary\n");
    printf("-------\n");
    printf("Assets checked:        %lu\n", files);
    printf("Sidecars found:        %lu\n", sidecars);
    printf("Paired assets:         %lu\n", paired);
    printf("Missing sidecars:      %lu\n", missing);
    printf("Orphan sidecars:       %lu\n", orphan);

    if (missing == 0 && orphan == 0) {
        printf("Reconcile status:      clean\n");
    } else {
        printf("Reconcile status:      attention_needed\n");
    }

    table_free(&table);

    fprintf(stderr, "[RECONCILE] reconciliation completed\n");

    return 0;
}
