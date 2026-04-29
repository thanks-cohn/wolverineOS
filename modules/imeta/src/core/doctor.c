/*
 * doctor.c
 * Reads .imeta/index.tsv and prints a basic health report.
 *
 * Build with:
 *   gcc -Wall -Wextra -O2 -o imeta \
 *     src/cli/imeta.c \
 *     src/core/scan.c \
 *     src/core/index.c \
 *     src/core/hash.c \
 *     src/core/bind.c \
 *     src/core/doctor.c
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct {
    unsigned long total_rows;
    unsigned long files;
    unsigned long sidecars;
    unsigned long malformed;
} ImetaDoctorStats;

static int line_starts_with(const char *line, const char *prefix) {
    if (!line || !prefix) return 0;
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

int imeta_doctor_path(const char *path) {
    (void)path;

    const char *index_path = ".imeta/index.tsv";

    fprintf(stderr, "[DOCTOR] starting health check\n");
    fprintf(stderr, "[DOCTOR] index path: %s\n", index_path);

    FILE *fp = fopen(index_path, "r");

    if (!fp) {
        fprintf(stderr, "[DOCTOR][ERROR] failed to open index: %s: %s\n",
                index_path,
                strerror(errno));
        fprintf(stderr, "[DOCTOR][HINT] run: ./imeta scan . --verbose\n");
        return 1;
    }

    ImetaDoctorStats stats;
    memset(&stats, 0, sizeof(stats));

    char line[8192];
    unsigned long line_number = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_number++;

        if (line_number == 1 && line_starts_with(line, "kind\t")) {
            continue;
        }

        if (line_starts_with(line, "FILE\t")) {
            stats.files++;
            stats.total_rows++;
            continue;
        }

        if (line_starts_with(line, "SIDECAR\t")) {
            stats.sidecars++;
            stats.total_rows++;
            continue;
        }

        if (strlen(line) > 1) {
            stats.malformed++;
            fprintf(stderr, "[DOCTOR][WARN] malformed index row at line %lu\n", line_number);
        }
    }

    fclose(fp);

    unsigned long assets = stats.files;
    unsigned long sidecars = stats.sidecars;

    double sidecar_ratio = 0.0;

    if (assets > 0) {
        sidecar_ratio = ((double)sidecars / (double)assets) * 100.0;
    }

    printf("\nDataset health report\n");
    printf("---------------------\n");
    printf("Index:                 %s\n", index_path);
    printf("Total indexed rows:    %lu\n", stats.total_rows);
    printf("Regular files:         %lu\n", stats.files);
    printf("Sidecars:              %lu\n", stats.sidecars);
    printf("Malformed rows:        %lu\n", stats.malformed);
    printf("Sidecar coverage:      %.2f%%\n", sidecar_ratio);

    printf("\nStatus\n");
    printf("------\n");

    if (stats.malformed > 0) {
        printf("Index health:          warning\n");
    } else {
        printf("Index health:          clean\n");
    }

    if (stats.files == 0) {
        printf("Dataset state:         empty or not scanned\n");
    } else if (stats.sidecars == 0) {
        printf("Dataset state:         files indexed, no sidecars yet\n");
    } else {
        printf("Dataset state:         sidecar layer detected\n");
    }

    fprintf(stderr, "[DOCTOR] health check completed\n");

    return stats.malformed > 0 ? 1 : 0;
}
