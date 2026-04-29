/*
 * bind_missing.c
 * Creates .imeta sidecars for indexed files that do not already have them.
 *
 * Reads:
 *   .imeta/index.tsv
 *
 * Uses:
 *   imeta_bind_file(path)
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define IMETA_MAX_ROWS 4096
#define IMETA_MAX_PATH 4096

int imeta_bind_file(const char *file_path);

typedef struct {
    char kind[32];
    char path[IMETA_MAX_PATH];
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
    table->rows = calloc(capacity, sizeof(ImetaIndexRow));

    if (!table->rows) {
        fprintf(stderr, "[BIND-MISSING][ERROR] calloc failed\n");
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
    FILE *fp = fopen(".imeta/index.tsv", "r");

    if (!fp) {
        fprintf(stderr, "[BIND-MISSING][ERROR] cannot open .imeta/index.tsv: %s\n", strerror(errno));
        fprintf(stderr, "[BIND-MISSING][HINT] run: ./imeta scan . --verbose\n");
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
            fprintf(stderr, "[BIND-MISSING][ERROR] row limit reached: %zu\n", table->capacity);
            fclose(fp);
            return 1;
        }

        char *kind = strtok(line, "\t");
        char *path = strtok(NULL, "\t");

        if (!kind || !path) {
            fprintf(stderr, "[BIND-MISSING][WARN] malformed row at line %zu\n", line_number);
            continue;
        }

        ImetaIndexRow *row = &table->rows[table->count];

        safe_copy(row->kind, sizeof(row->kind), kind);
        safe_copy(row->path, sizeof(row->path), path);

        table->count++;
    }

    fclose(fp);
    return 0;
}

static int row_exists(const ImetaIndexTable *table, const char *path, const char *kind) {
    if (!table || !path || !kind) return 0;

    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->rows[i].path, path) == 0 &&
            strcmp(table->rows[i].kind, kind) == 0) {
            return 1;
        }
    }

    return 0;
}

int imeta_bind_missing_path(const char *path) {
    fprintf(stderr, "[BIND-MISSING] starting\n");
    fprintf(stderr, "[BIND-MISSING] target path: %s\n", path ? path : "(null)");

    ImetaIndexTable table;

    if (table_init(&table, IMETA_MAX_ROWS) != 0) {
        return 1;
    }

    if (load_index(&table) != 0) {
        table_free(&table);
        return 1;
    }

    unsigned long assets_checked = 0;
    unsigned long already_paired = 0;
    unsigned long created = 0;
    unsigned long failed = 0;

    for (size_t i = 0; i < table.count; i++) {
        ImetaIndexRow *row = &table.rows[i];

        if (strcmp(row->kind, "FILE") != 0) continue;
        if (has_suffix(row->path, ".imeta")) continue;

        assets_checked++;

        char expected_sidecar[IMETA_MAX_PATH];

        int written = snprintf(
            expected_sidecar,
            sizeof(expected_sidecar),
            "%s.imeta",
            row->path
        );

        if (written < 0 || (size_t)written >= sizeof(expected_sidecar)) {
            fprintf(stderr, "[BIND-MISSING][WARN] sidecar path too long: %s\n", row->path);
            failed++;
            continue;
        }

        if (row_exists(&table, expected_sidecar, "SIDECAR")) {
            already_paired++;
            printf("[SKIP] already paired: %s\n", row->path);
            continue;
        }

        printf("[CREATE] %s -> %s\n", row->path, expected_sidecar);

        if (imeta_bind_file(row->path) == 0) {
            created++;
        } else {
            failed++;
        }
    }

    printf("\nBind-missing summary\n");
    printf("--------------------\n");
    printf("Assets checked:        %lu\n", assets_checked);
    printf("Already paired:        %lu\n", already_paired);
    printf("Sidecars created:      %lu\n", created);
    printf("Failed:                %lu\n", failed);

    table_free(&table);

    fprintf(stderr, "[BIND-MISSING] completed\n");

    return failed > 0 ? 1 : 0;
}
