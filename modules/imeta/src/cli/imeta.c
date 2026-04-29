/*
 * imeta.c
 * Main CLI entry point for the .imeta system.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>

#define IMETA_VERSION "0.1.0"

int imeta_scan_path(const char *path);
int imeta_bind_file(const char *file_path);
int imeta_doctor_path(const char *path);
int imeta_reconcile_path(const char *path);
int imeta_bind_missing_path(const char *path);
int imeta_memory_remember(const char *field_path, const char *value, const char *source_file);
int imeta_memory_list(void);

typedef enum {
    LOG_TRACE = 0,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

static LogLevel CURRENT_LOG_LEVEL = LOG_INFO;

static const char *level_to_string(LogLevel level) {
    switch (level) {
        case LOG_TRACE: return "TRACE";
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO ";
        case LOG_WARN:  return "WARN ";
        case LOG_ERROR: return "ERROR";
        default:        return "LOG  ";
    }
}

static void imeta_log(LogLevel level, const char *fmt, ...) {
    if (level < CURRENT_LOG_LEVEL) return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];

    if (tm_info) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        snprintf(timestamp, sizeof(timestamp), "unknown-time");
    }

    fprintf(stderr, "[%s] [%s] ", timestamp, level_to_string(level));

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}

#define LOG_TRACE(...) imeta_log(LOG_TRACE, __VA_ARGS__)
#define LOG_DEBUG(...) imeta_log(LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  imeta_log(LOG_INFO,  __VA_ARGS__)
#define LOG_WARN(...)  imeta_log(LOG_WARN,  __VA_ARGS__)
#define LOG_ERROR(...) imeta_log(LOG_ERROR, __VA_ARGS__)

static void print_banner(void) {
    printf("imeta %s\n", IMETA_VERSION);
    printf("Universal metadata, recovery, and training layer.\n");
}

static void print_usage(void) {
    print_banner();

    printf("\nUsage:\n");
    printf("  imeta <command> [path] [options]\n");

    printf("\nCommands:\n");
    printf("  scan <path>                    Scan files and build/update the imeta index\n");
    printf("  watch <path>                   Start watcher daemon mode for a path\n");
    printf("  doctor <path>                  Report dataset/index health\n");
    printf("  bind <file>                    Create or bind .imeta sidecar to a file\n");
    printf("  bind-missing <path>            Create .imeta sidecars for indexed files missing them\n");
    printf("  reconcile <path>               Recover missing or separated sidecars/files\n");
    printf("  remember <field> <value> [src] Remember a metadata field/value for GUI suggestions\n");
    printf("  memory                         Print remembered metadata field/value history\n");
    printf("  export <path>                  Export metadata for training pipelines\n");
    printf("  version                        Show version\n");
    printf("  help                           Show this help\n");

    printf("\nOptions:\n");
    printf("  --verbose                      Enable debug logs\n");
    printf("  --trace                        Enable trace logs\n");
    printf("  --quiet                        Only show warnings/errors\n");

    printf("\nExamples:\n");
    printf("  imeta scan ./images --verbose\n");
    printf("  imeta doctor ./dataset\n");
    printf("  imeta bind ./image.jpg --verbose\n");
    printf("  imeta bind-missing . --verbose\n");
    printf("  imeta reconcile ./messy_folder --verbose\n");
    printf("  imeta remember annotations.tags red ./image.jpg\n");
    printf("  imeta remember annotations.tag_meta.red.shade crimson ./image.jpg\n");
    printf("  imeta memory\n");
}

static int path_is_present(const char *path) {
    return path != NULL && strlen(path) > 0;
}

static int cmd_scan(const char *path) {
    if (!path_is_present(path)) {
        LOG_ERROR("scan requires a path");
        return 1;
    }

    LOG_INFO("scan started");
    LOG_INFO("target path: %s", path);
    LOG_DEBUG("routing to scan engine");

    int rc = imeta_scan_path(path);

    if (rc == 0) {
        LOG_INFO("scan finished successfully");
    } else {
        LOG_ERROR("scan finished with errors");
    }

    return rc;
}

static int cmd_watch(const char *path) {
    if (!path_is_present(path)) {
        LOG_ERROR("watch requires a path");
        return 1;
    }

    LOG_INFO("watch mode requested");
    LOG_INFO("target path: %s", path);
    LOG_WARN("watch command is handled by imeta-watchd/systemd for now");
    return 0;
}

static int cmd_doctor(const char *path) {
    if (!path_is_present(path)) {
        LOG_ERROR("doctor requires a path");
        return 1;
    }

    LOG_INFO("doctor started");
    LOG_INFO("target path: %s", path);
    LOG_DEBUG("routing to doctor engine");

    int rc = imeta_doctor_path(path);

    if (rc == 0) {
        LOG_INFO("doctor finished successfully");
    } else {
        LOG_WARN("doctor finished with warnings or errors");
    }

    return rc;
}

static int cmd_bind(const char *file) {
    if (!path_is_present(file)) {
        LOG_ERROR("bind requires a file path");
        return 1;
    }

    LOG_INFO("bind started");
    LOG_INFO("target file: %s", file);
    LOG_DEBUG("routing to bind engine");

    int rc = imeta_bind_file(file);

    if (rc == 0) {
        LOG_INFO("bind finished successfully");
    } else {
        LOG_ERROR("bind finished with errors");
    }

    return rc;
}

static int cmd_bind_missing(const char *path) {
    if (!path_is_present(path)) {
        LOG_ERROR("bind-missing requires a path");
        return 1;
    }

    LOG_INFO("bind-missing started");
    LOG_INFO("target path: %s", path);
    LOG_DEBUG("routing to bind-missing engine");

    int rc = imeta_bind_missing_path(path);

    if (rc == 0) {
        LOG_INFO("bind-missing finished successfully");
    } else {
        LOG_WARN("bind-missing finished with warnings or errors");
    }

    return rc;
}

static int cmd_reconcile(const char *path) {
    if (!path_is_present(path)) {
        LOG_ERROR("reconcile requires a path");
        return 1;
    }

    LOG_INFO("reconcile started");
    LOG_INFO("target path: %s", path);
    LOG_DEBUG("routing to reconcile engine");

    int rc = imeta_reconcile_path(path);

    if (rc == 0) {
        LOG_INFO("reconcile finished successfully");
    } else {
        LOG_WARN("reconcile finished with warnings or errors");
    }

    return rc;
}

static int cmd_remember(int argc, char **argv) {
    if (argc < 4) {
        LOG_ERROR("remember requires <field_path> and <value>");
        fprintf(stderr, "Usage: imeta remember <field_path> <value> [source_file]\n");
        return 1;
    }

    const char *field_path = argv[2];
    const char *value = argv[3];
    const char *source_file = (argc >= 5 && strncmp(argv[4], "--", 2) != 0) ? argv[4] : "";

    LOG_INFO("memory remember started");
    LOG_INFO("field path: %s", field_path);
    LOG_INFO("value: %s", value);

    int rc = imeta_memory_remember(field_path, value, source_file);

    if (rc == 0) {
        LOG_INFO("memory remember finished successfully");
    } else {
        LOG_ERROR("memory remember failed");
    }

    return rc;
}

static int cmd_memory(void) {
    LOG_INFO("memory list started");

    int rc = imeta_memory_list();

    if (rc == 0) {
        LOG_INFO("memory list finished successfully");
    } else {
        LOG_WARN("memory list finished with warnings or errors");
    }

    return rc;
}

static int cmd_export(const char *path) {
    if (!path_is_present(path)) {
        LOG_ERROR("export requires a path");
        return 1;
    }

    LOG_INFO("export started");
    LOG_INFO("target path: %s", path);
    LOG_WARN("export engine not implemented yet");
    LOG_INFO("export finished");

    return 0;
}

static void parse_global_flags(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            CURRENT_LOG_LEVEL = LOG_DEBUG;
        } else if (strcmp(argv[i], "--trace") == 0) {
            CURRENT_LOG_LEVEL = LOG_TRACE;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            CURRENT_LOG_LEVEL = LOG_WARN;
        }
    }
}

static const char *first_non_flag_after_command(int argc, char **argv) {
    for (int i = 2; i < argc; i++) {
        if (strncmp(argv[i], "--", 2) != 0) {
            return argv[i];
        }
    }

    return NULL;
}

int main(int argc, char **argv) {
    parse_global_flags(argc, argv);

    LOG_DEBUG("imeta process started");
    LOG_DEBUG("argc=%d", argc);

    if (argc < 2) {
        LOG_WARN("no command provided");
        print_usage();
        return 1;
    }

    const char *command = argv[1];
    const char *path = first_non_flag_after_command(argc, argv);

    LOG_DEBUG("command=%s", command);

    if (path) {
        LOG_DEBUG("path=%s", path);
    } else {
        LOG_DEBUG("path=(none)");
    }

    if (
        strcmp(command, "help") == 0 ||
        strcmp(command, "--help") == 0 ||
        strcmp(command, "-h") == 0
    ) {
        print_usage();
        return 0;
    }

    if (
        strcmp(command, "version") == 0 ||
        strcmp(command, "--version") == 0 ||
        strcmp(command, "-v") == 0
    ) {
        print_banner();
        return 0;
    }

    if (strcmp(command, "scan") == 0) {
        return cmd_scan(path);
    }

    if (strcmp(command, "watch") == 0) {
        return cmd_watch(path);
    }

    if (strcmp(command, "doctor") == 0) {
        return cmd_doctor(path);
    }

    if (strcmp(command, "bind") == 0) {
        return cmd_bind(path);
    }

    if (strcmp(command, "bind-missing") == 0) {
        return cmd_bind_missing(path);
    }

    if (strcmp(command, "reconcile") == 0) {
        return cmd_reconcile(path);
    }

    if (strcmp(command, "remember") == 0) {
        return cmd_remember(argc, argv);
    }

    if (strcmp(command, "memory") == 0) {
        return cmd_memory();
    }

    if (strcmp(command, "export") == 0) {
        return cmd_export(path);
    }

    LOG_ERROR("unknown command: %s", command);
    print_usage();

    return 1;
}
