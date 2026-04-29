#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>

#include "wos_memory.h"

#define MEM_DIR ".wolverine/memory/files"
#define INDEX_FILE ".wolverine/memory/index.wmeta.jsonl"
#define LOG_DIR ".wolverine/logs"
#define QUAR_DIR ".wolverine/quarantine/jsons"

typedef struct {
    int updated;
    int skipped;
    int errors;
} MemCounts;

typedef struct {
    char tags_json[4096];
    char note_json[4096];
    char summary_json[4096];
    char custom_json[8192];
} MemoryDoc;

static void ensure_dirs(void) {
    mkdir(".wolverine", 0755);
    mkdir(".wolverine/memory", 0755);
    mkdir(MEM_DIR, 0755);
    mkdir(LOG_DIR, 0755);
    mkdir(".wolverine/quarantine", 0755);
    mkdir(QUAR_DIR, 0755);
}

static int is_regular_file(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int is_directory(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void hash_path(const char *input, char *output) {
    unsigned long hash = 5381;
    int c;
    while ((c = *input++))
        hash = ((hash << 5) + hash) + c;
    snprintf(output, 32, "%lx", hash);
}

static void get_memory_path(const char *path, char *out) {
    char hash[32];
    hash_path(path, hash);
    snprintf(out, PATH_MAX, "%s/%s.wmeta.json", MEM_DIR, hash);
}

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);

    if (n < 0) {
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);

    if (out_len) *out_len = got;
    return buf;
}

static void buf_putc(char *out, size_t sz, size_t *pos, char c) {
    if (*pos + 1 < sz) {
        out[*pos] = c;
        (*pos)++;
        out[*pos] = '\0';
    }
}

static void buf_puts(char *out, size_t sz, size_t *pos, const char *s) {
    while (s && *s) {
        buf_putc(out, sz, pos, *s);
        s++;
    }
}

static void buf_json_escape(char *out, size_t sz, size_t *pos, const char *s) {
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;

        if (c == '"') buf_puts(out, sz, pos, "\\\"");
        else if (c == '\\') buf_puts(out, sz, pos, "\\\\");
        else if (c == '\n') buf_puts(out, sz, pos, "\\n");
        else if (c == '\r') buf_puts(out, sz, pos, "\\r");
        else if (c == '\t') buf_puts(out, sz, pos, "\\t");
        else if (c < 0x20) {
            char tmp[16];
            snprintf(tmp, sizeof(tmp), "\\u%04x", c);
            buf_puts(out, sz, pos, tmp);
        } else {
            buf_putc(out, sz, pos, (char)c);
        }
    }
}

static void make_json_string(char *out, size_t sz, const char *text) {
    size_t pos = 0;
    out[0] = '\0';
    buf_putc(out, sz, &pos, '"');
    buf_json_escape(out, sz, &pos, text ? text : "");
    buf_putc(out, sz, &pos, '"');
}

static void make_values_array(char *out, size_t sz, const char *values) {
    size_t pos = 0;
    out[0] = '\0';

    char *copy = strdup(values ? values : "");
    if (!copy) {
        snprintf(out, sz, "[]");
        return;
    }

    buf_putc(out, sz, &pos, '[');

    int first = 1;
    char *save = NULL;
    char *tok = strtok_r(copy, "|", &save);

    while (tok) {
        while (*tok && isspace((unsigned char)*tok)) tok++;

        char *end = tok + strlen(tok);
        while (end > tok && isspace((unsigned char)*(end - 1))) {
            *(--end) = '\0';
        }

        if (*tok) {
            if (!first) buf_puts(out, sz, &pos, ", ");
            buf_putc(out, sz, &pos, '"');
            buf_json_escape(out, sz, &pos, tok);
            buf_putc(out, sz, &pos, '"');
            first = 0;
        }

        tok = strtok_r(NULL, "|", &save);
    }

    buf_putc(out, sz, &pos, ']');
    free(copy);
}

static void make_custom_object(char *out, size_t sz, const char *field, const char *values) {
    size_t pos = 0;
    char arr[4096];

    out[0] = '\0';
    make_values_array(arr, sizeof(arr), values);

    buf_putc(out, sz, &pos, '{');
    if (field && *field) {
        buf_putc(out, sz, &pos, '"');
        buf_json_escape(out, sz, &pos, field);
        buf_puts(out, sz, &pos, "\": ");
        buf_puts(out, sz, &pos, arr);
    }
    buf_putc(out, sz, &pos, '}');
}

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static int parse_string(const char **pp) {
    const char *p = skip_ws(*pp);
    if (*p != '"') return 0;
    p++;

    while (*p) {
        if (*p == '\\') {
            p++;
            if (!*p) return 0;
            p++;
            continue;
        }
        if (*p == '"') {
            *pp = p + 1;
            return 1;
        }
        if ((unsigned char)*p < 0x20) return 0;
        p++;
    }

    return 0;
}

static int parse_value(const char **pp);

static int parse_array(const char **pp) {
    const char *p = skip_ws(*pp);
    if (*p != '[') return 0;
    p++;
    p = skip_ws(p);

    if (*p == ']') {
        *pp = p + 1;
        return 1;
    }

    while (*p) {
        if (!parse_value(&p)) return 0;
        p = skip_ws(p);

        if (*p == ',') {
            p++;
            continue;
        }

        if (*p == ']') {
            *pp = p + 1;
            return 1;
        }

        return 0;
    }

    return 0;
}

static int parse_object(const char **pp) {
    const char *p = skip_ws(*pp);
    if (*p != '{') return 0;
    p++;
    p = skip_ws(p);

    if (*p == '}') {
        *pp = p + 1;
        return 1;
    }

    while (*p) {
        if (!parse_string(&p)) return 0;
        p = skip_ws(p);

        if (*p != ':') return 0;
        p++;

        if (!parse_value(&p)) return 0;
        p = skip_ws(p);

        if (*p == ',') {
            p++;
            continue;
        }

        if (*p == '}') {
            *pp = p + 1;
            return 1;
        }

        return 0;
    }

    return 0;
}

static int parse_number(const char **pp) {
    const char *p = skip_ws(*pp);

    if (*p == '-') p++;
    if (!isdigit((unsigned char)*p)) return 0;

    if (*p == '0') {
        p++;
    } else {
        while (isdigit((unsigned char)*p)) p++;
    }

    if (*p == '.') {
        p++;
        if (!isdigit((unsigned char)*p)) return 0;
        while (isdigit((unsigned char)*p)) p++;
    }

    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!isdigit((unsigned char)*p)) return 0;
        while (isdigit((unsigned char)*p)) p++;
    }

    *pp = p;
    return 1;
}

static int match_lit(const char **pp, const char *lit) {
    const char *p = skip_ws(*pp);
    size_t n = strlen(lit);
    if (strncmp(p, lit, n) != 0) return 0;
    *pp = p + n;
    return 1;
}

static int parse_value(const char **pp) {
    const char *p = skip_ws(*pp);

    if (*p == '"') return parse_string(pp);
    if (*p == '{') return parse_object(pp);
    if (*p == '[') return parse_array(pp);
    if (*p == '-' || isdigit((unsigned char)*p)) return parse_number(pp);
    if (match_lit(pp, "true")) return 1;
    if (match_lit(pp, "false")) return 1;
    if (match_lit(pp, "null")) return 1;

    return 0;
}

static int is_valid_json_doc(const char *buf, char *reason, size_t reason_sz) {
    const char *p = buf;

    if (!parse_object(&p)) {
        snprintf(reason, reason_sz, "invalid JSON object");
        return 0;
    }

    p = skip_ws(p);
    if (*p != '\0') {
        snprintf(reason, reason_sz, "extra data after JSON object");
        return 0;
    }

    if (!strstr(buf, "\"version\"")) {
        snprintf(reason, reason_sz, "missing required field: version");
        return 0;
    }

    if (!strstr(buf, "\"path\"")) {
        snprintf(reason, reason_sz, "missing required field: path");
        return 0;
    }

    if (!strstr(buf, "\"tags\"")) {
        snprintf(reason, reason_sz, "missing required field: tags");
        return 0;
    }

    reason[0] = '\0';
    return 1;
}

static int extract_json_field(const char *buf, const char *key, char *out, size_t out_sz) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(buf, pattern);
    if (!p) return 0;

    p += strlen(pattern);
    p = skip_ws(p);

    if (*p != ':') return 0;
    p++;
    p = skip_ws(p);

    const char *start = p;
    if (!parse_value(&p)) return 0;

    size_t n = (size_t)(p - start);
    if (n >= out_sz) n = out_sz - 1;

    memcpy(out, start, n);
    out[n] = '\0';

    return 1;
}

static void memory_doc_defaults(MemoryDoc *doc) {
    snprintf(doc->tags_json, sizeof(doc->tags_json), "[]");
    snprintf(doc->note_json, sizeof(doc->note_json), "\"\"");
    snprintf(doc->summary_json, sizeof(doc->summary_json), "\"\"");
    snprintf(doc->custom_json, sizeof(doc->custom_json), "{}");
}

static void load_existing_doc(const char *mem_path, MemoryDoc *doc) {
    memory_doc_defaults(doc);

    size_t len = 0;
    char *buf = read_file(mem_path, &len);
    if (!buf) return;

    char reason[256];
    if (!is_valid_json_doc(buf, reason, sizeof(reason))) {
        free(buf);
        return;
    }

    extract_json_field(buf, "tags", doc->tags_json, sizeof(doc->tags_json));
    extract_json_field(buf, "note", doc->note_json, sizeof(doc->note_json));
    extract_json_field(buf, "summary", doc->summary_json, sizeof(doc->summary_json));
    extract_json_field(buf, "custom", doc->custom_json, sizeof(doc->custom_json));

    free(buf);
}

static int write_doc_atomic(const char *mem_path, const char *target_path, const MemoryDoc *doc) {
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", mem_path);

    FILE *f = fopen(tmp, "w");
    if (!f) {
        fprintf(stderr, "ERROR: could not write memory temp file: %s\n", tmp);
        return 1;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"version\": 1,\n");
    fprintf(f, "  \"path\": \"");

    size_t dummy = 0;
    char escaped_path[PATH_MAX * 2];
    escaped_path[0] = '\0';
    buf_json_escape(escaped_path, sizeof(escaped_path), &dummy, target_path);

    fprintf(f, "%s", escaped_path);
    fprintf(f, "\",\n");
    fprintf(f, "  \"tags\": %s,\n", doc->tags_json);
    fprintf(f, "  \"note\": %s,\n", doc->note_json);
    fprintf(f, "  \"summary\": %s,\n", doc->summary_json);
    fprintf(f, "  \"custom\": %s\n", doc->custom_json);
    fprintf(f, "}\n");

    fclose(f);

    if (rename(tmp, mem_path) != 0) {
        fprintf(stderr, "ERROR: could not replace memory file: %s\n", mem_path);
        return 1;
    }

    return 0;
}

static int create_template(const char *file_path, const char *target_path) {
    MemoryDoc doc;
    memory_doc_defaults(&doc);
    return write_doc_atomic(file_path, target_path, &doc);
}

static int update_memory_file(const char *target_path, const char *mode, const char *a, const char *b) {
    ensure_dirs();

    char abs[PATH_MAX];
    if (!realpath(target_path, abs)) {
        fprintf(stderr, "ERROR: path not found: %s\n", target_path);
        return 1;
    }

    char mem_path[PATH_MAX];
    get_memory_path(abs, mem_path);

    MemoryDoc doc;
    load_existing_doc(mem_path, &doc);

    if (strcmp(mode, "tag") == 0) {
        make_values_array(doc.tags_json, sizeof(doc.tags_json), a);
    } else if (strcmp(mode, "note") == 0) {
        make_json_string(doc.note_json, sizeof(doc.note_json), a);
    } else if (strcmp(mode, "summary") == 0) {
        make_json_string(doc.summary_json, sizeof(doc.summary_json), a);
    } else if (strcmp(mode, "custom") == 0) {
        make_custom_object(doc.custom_json, sizeof(doc.custom_json), a, b);
    } else {
        return 1;
    }

    return write_doc_atomic(mem_path, abs, &doc);
}

static void write_compact_jsonl(FILE *out, const char *buf) {
    int in_string = 0;
    int escape = 0;

    for (const char *p = buf; *p; p++) {
        char c = *p;

        if (in_string) {
            fputc(c, out);

            if (escape) escape = 0;
            else if (c == '\\') escape = 1;
            else if (c == '"') in_string = 0;
            continue;
        }

        if (c == '"') {
            in_string = 1;
            fputc(c, out);
            continue;
        }

        if (!isspace((unsigned char)c)) {
            fputc(c, out);
        }
    }

    fputc('\n', out);
}

static void copy_to_quarantine(const char *src, const char *name) {
    char dest[PATH_MAX];
    snprintf(dest, sizeof(dest), "%s/%s.rejected", QUAR_DIR, name);

    FILE *in = fopen(src, "rb");
    if (!in) return;

    FILE *out = fopen(dest, "wb");
    if (!out) {
        fclose(in);
        return;
    }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }

    fclose(in);
    fclose(out);
}

static int apply_memory_recursive(const char *path, int nested, const char *mode, const char *a, const char *b, MemCounts *counts) {
    if (is_regular_file(path)) {
        int rc = update_memory_file(path, mode, a, b);
        if (rc == 0) counts->updated++;
        else counts->errors++;
        return rc;
    }

    if (!is_directory(path)) {
        fprintf(stderr, "ERROR: not file or directory: %s\n", path);
        counts->errors++;
        return 1;
    }

    DIR *d = opendir(path);
    if (!d) {
        fprintf(stderr, "ERROR: could not open directory: %s\n", path);
        counts->errors++;
        return 1;
    }

    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;

        char child[PATH_MAX];
        snprintf(child, sizeof(child), "%s/%s", path, e->d_name);

        if (is_regular_file(child)) {
            apply_memory_recursive(child, nested, mode, a, b, counts);
        } else if (is_directory(child)) {
            if (nested) apply_memory_recursive(child, nested, mode, a, b, counts);
            else counts->skipped++;
        } else {
            counts->skipped++;
        }
    }

    closedir(d);
    return counts->errors ? 1 : 0;
}

static int apply_to_path(const char *path, int nested, const char *mode, const char *a, const char *b) {
    MemCounts counts = {0, 0, 0};

    int rc = apply_memory_recursive(path, nested, mode, a, b, &counts);

    printf("ivault %s\n", mode);
    printf("Target: %s\n", path);
    printf("Nested: %s\n", nested ? "yes" : "no");
    printf("Updated: %d\n", counts.updated);
    printf("Skipped: %d\n", counts.skipped);
    printf("Errors: %d\n", counts.errors);

    return rc;
}

int wos_memory_edit(const char *path) {
    ensure_dirs();

    char abs[PATH_MAX];
    if (!realpath(path, abs)) {
        fprintf(stderr, "ERROR: path not found: %s\n", path);
        return 1;
    }

    char mem_path[PATH_MAX];
    get_memory_path(abs, mem_path);

    if (access(mem_path, F_OK) != 0) {
        create_template(mem_path, abs);
    }

    const char *editor = getenv("EDITOR");
    if (!editor) editor = "nano";

    char cmd[PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "%s \"%s\"", editor, mem_path);

    return system(cmd);
}

int wos_memory_view(const char *path) {
    char abs[PATH_MAX];
    if (!realpath(path, abs)) {
        fprintf(stderr, "ERROR: path not found: %s\n", path);
        return 1;
    }

    char mem_path[PATH_MAX];
    get_memory_path(abs, mem_path);

    FILE *f = fopen(mem_path, "r");
    if (!f) {
        printf("No memory found.\n");
        return 1;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        printf("%s", line);
    }

    fclose(f);
    return 0;
}

int wos_memory_push(void) {
    ensure_dirs();

    DIR *d = opendir(MEM_DIR);
    if (!d) {
        printf("No memory directory.\n");
        return 1;
    }

    time_t now = time(NULL);
    char log_path[PATH_MAX];
    snprintf(log_path, sizeof(log_path), "%s/push-%ld.log", LOG_DIR, (long)now);

    FILE *log = fopen(log_path, "w");
    if (!log) log = stderr;

    FILE *index = fopen(INDEX_FILE ".tmp", "w");
    if (!index) {
        if (log != stderr) fclose(log);
        closedir(d);
        return 1;
    }

    fprintf(log, "IVAULT PUSH REPORT\n");
    fprintf(log, "started=%ld\n\n", (long)now);

    int scanned = 0, updated = 0, rejected = 0, read_errors = 0;

    struct dirent *entry;
    while ((entry = readdir(d))) {
        if (!strstr(entry->d_name, ".wmeta.json"))
            continue;

        scanned++;

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", MEM_DIR, entry->d_name);

        size_t len = 0;
        char *buf = read_file(full, &len);

        if (!buf) {
            read_errors++;
            fprintf(log, "FILE %s\nSTATUS READ_ERROR\n\n", full);
            continue;
        }

        char reason[256];
        if (!is_valid_json_doc(buf, reason, sizeof(reason))) {
            rejected++;
            fprintf(log, "FILE %s\nSTATUS REJECTED\nREASON %s\nACTION previous index not poisoned; copied to quarantine\n\n", full, reason);
            copy_to_quarantine(full, entry->d_name);
            free(buf);
            continue;
        }

        write_compact_jsonl(index, buf);
        fprintf(log, "FILE %s\nSTATUS UPDATED\nBYTES %zu\n\n", full, len);
        updated++;
        free(buf);
    }

    closedir(d);
    fclose(index);

    if (rename(INDEX_FILE ".tmp", INDEX_FILE) != 0) {
        fprintf(log, "ERROR failed to replace index\n");
        if (log != stderr) fclose(log);
        return 1;
    }

    fprintf(log, "SUMMARY scanned=%d updated=%d rejected=%d read_errors=%d\n",
            scanned, updated, rejected, read_errors);

    if (log != stderr) fclose(log);

    printf("\nivault push\n");
    printf("Scanned: %d\n", scanned);
    printf("Updated: %d\n", updated);
    printf("Rejected: %d\n", rejected);
    printf("Read errors: %d\n", read_errors);
    printf("Log: %s\n\n", log_path);

    return rejected || read_errors ? 2 : 0;
}

int wos_memory_tag(const char *path, const char *tags, int nested) {
    return apply_to_path(path, nested, "tag", tags, NULL);
}

int wos_memory_note(const char *path, const char *note, int nested) {
    return apply_to_path(path, nested, "note", note, NULL);
}

int wos_memory_summary(const char *path, const char *summary, int nested) {
    return apply_to_path(path, nested, "summary", summary, NULL);
}

int wos_memory_custom(const char *path, const char *field, const char *values, int nested) {
    return apply_to_path(path, nested, "custom", field, values);
}
