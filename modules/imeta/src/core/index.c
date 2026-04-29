/*
 * index.c
 * Simple TSV index writer for imeta.
 *
 * Output:
 *   .imeta/index.tsv
 *
 * Format:
 *   kind	path	size	inode	mtime	hash
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static FILE *g_index_file = NULL;

int imeta_index_open(const char *root_path) {
    (void)root_path;

    if (mkdir(".imeta", 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "[INDEX][ERROR] failed to create .imeta directory: %s\n", strerror(errno));
        return 1;
    }

    g_index_file = fopen(".imeta/index.tsv", "w");

    if (!g_index_file) {
        fprintf(stderr, "[INDEX][ERROR] failed to open .imeta/index.tsv: %s\n", strerror(errno));
        return 1;
    }

    fprintf(g_index_file, "kind\tpath\tsize\tinode\tmtime\thash\n");
    fprintf(stderr, "[INDEX] opened .imeta/index.tsv\n");

    return 0;
}

int imeta_index_write_file(
    const char *kind,
    const char *path,
    long long size,
    long long inode,
    long long mtime,
    const char *hash
) {
    if (!g_index_file) {
        fprintf(stderr, "[INDEX][ERROR] index file is not open\n");
        return 1;
    }

    fprintf(
        g_index_file,
        "%s\t%s\t%lld\t%lld\t%lld\t%s\n",
        kind,
        path,
        size,
        inode,
        mtime,
        hash ? hash : ""
    );

    return 0;
}

int imeta_index_close(void) {
    if (!g_index_file) {
        return 0;
    }

    fclose(g_index_file);
    g_index_file = NULL;

    fprintf(stderr, "[INDEX] closed .imeta/index.tsv\n");

    return 0;
}
