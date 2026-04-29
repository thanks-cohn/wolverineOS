/*
 * memory.c
 * imeta semantic memory store.
 *
 * Purpose:
 *   Remember user-created metadata paths and values so the GUI can suggest
 *   previously used fields, subfields, tags, moods, regions, etc.
 *
 * Storage:
 *   .imeta/memory.jsonl
 *
 * Format:
 *   one JSON object per line
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <limits.h>

static int ensure_imeta_dir(void) {
    if (mkdir(".imeta", 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "[MEMORY][ERROR] failed to create .imeta directory: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

static void write_json_string(FILE *fp, const char *value) {
    fputc('"', fp);

    if (!value) {
        fputc('"', fp);
        return;
    }

    for (const char *p = value; *p; p++) {
        switch (*p) {
            case '\\': fputs("\\\\", fp); break;
            case '"':  fputs("\\\"", fp); break;
            case '\n': fputs("\\n", fp); break;
            case '\r': fputs("\\r", fp); break;
            case '\t': fputs("\\t", fp); break;
            default:   fputc(*p, fp); break;
        }
    }

    fputc('"', fp);
}

int imeta_memory_remember(
    const char *field_path,
    const char *value,
    const char *source_file
) {
    if (!field_path || strlen(field_path) == 0) {
        fprintf(stderr, "[MEMORY][ERROR] field path required\n");
        return 1;
    }

    if (ensure_imeta_dir() != 0) {
        return 1;
    }

    const char *memory_path = ".imeta/memory.jsonl";

    FILE *fp = fopen(memory_path, "a");

    if (!fp) {
        fprintf(stderr, "[MEMORY][ERROR] failed to open memory file: %s: %s\n",
                memory_path,
                strerror(errno));
        return 1;
    }

    time_t now = time(NULL);

    fprintf(fp, "{");

    fprintf(fp, "\"type\":\"field_memory\",");
    fprintf(fp, "\"field_path\":");
    write_json_string(fp, field_path);
    fprintf(fp, ",");

    fprintf(fp, "\"value\":");
    write_json_string(fp, value ? value : "");
    fprintf(fp, ",");

    fprintf(fp, "\"source_file\":");
    write_json_string(fp, source_file ? source_file : "");
    fprintf(fp, ",");

    fprintf(fp, "\"created_unix\":%lld", (long long)now);

    fprintf(fp, "}\n");

    fclose(fp);

    fprintf(stderr, "[MEMORY] remembered field=\"%s\" value=\"%s\"\n",
            field_path,
            value ? value : "");

    return 0;
}

int imeta_memory_list(void) {
    const char *memory_path = ".imeta/memory.jsonl";

    FILE *fp = fopen(memory_path, "r");

    if (!fp) {
        fprintf(stderr, "[MEMORY][WARN] no memory file yet: %s\n", memory_path);
        return 0;
    }

    char line[8192];

    printf("\nImeta memory\n");
    printf("------------\n");

    while (fgets(line, sizeof(line), fp)) {
        fputs(line, stdout);
    }

    fclose(fp);

    return 0;
}
