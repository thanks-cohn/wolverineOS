/*
 * imeta_watchd.c
 * Minimal Linux inotify daemon for imeta.
 *
 * Watches one directory and auto-binds regular files only after
 * writes are complete.
 *
 * Important behavior:
 *   - binds on IN_CLOSE_WRITE and IN_MOVED_TO
 *   - does NOT bind on IN_CREATE
 *   - ignores .imeta files and build artifacts
 *
 * Build:
 *   make
 *
 * Run:
 *   ./imeta-watchd .
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <limits.h>
#include <signal.h>

#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + NAME_MAX + 1))

int imeta_bind_file(const char *file_path);

static volatile sig_atomic_t g_running = 1;

static int has_suffix(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) return 0;

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static int should_ignore_name(const char *name) {
    if (!name) return 1;

    if (strcmp(name, ".") == 0) return 1;
    if (strcmp(name, "..") == 0) return 1;
    if (strcmp(name, ".imeta") == 0) return 1;
    if (strcmp(name, "imeta") == 0) return 1;
    if (strcmp(name, "imeta-watchd") == 0) return 1;

    if (has_suffix(name, ".imeta")) return 1;
    if (has_suffix(name, ".o")) return 1;
    if (has_suffix(name, ".so")) return 1;
    if (has_suffix(name, ".a")) return 1;
    if (has_suffix(name, ".tmp")) return 1;
    if (has_suffix(name, ".swp")) return 1;
    if (has_suffix(name, "~")) return 1;

    return 0;
}

static void handle_signal(int signal_number) {
    (void)signal_number;
    g_running = 0;
}

static int is_regular_file(const char *path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        fprintf(stderr, "[WATCHD][WARN] stat failed: %s: %s\n", path, strerror(errno));
        return 0;
    }

    return S_ISREG(st.st_mode);
}

static int build_child_path(
    const char *dir,
    const char *name,
    char *out,
    size_t out_size
) {
    int written = snprintf(out, out_size, "%s/%s", dir, name);

    if (written < 0 || (size_t)written >= out_size) {
        fprintf(stderr, "[WATCHD][ERROR] path too long: %s/%s\n", dir, name);
        return 1;
    }

    return 0;
}

static void maybe_bind_finished_file(const char *watch_dir, const char *name) {
    if (should_ignore_name(name)) {
        fprintf(stderr, "[WATCHD] ignored: %s\n", name);
        return;
    }

    char path[PATH_MAX];

    if (build_child_path(watch_dir, name, path, sizeof(path)) != 0) {
        return;
    }

    if (!is_regular_file(path)) {
        fprintf(stderr, "[WATCHD] ignored non-regular path: %s\n", path);
        return;
    }

    fprintf(stderr, "[WATCHD] binding completed file: %s\n", path);

    if (imeta_bind_file(path) != 0) {
        fprintf(stderr, "[WATCHD][ERROR] bind failed: %s\n", path);
    } else {
        fprintf(stderr, "[WATCHD] bind ok: %s\n", path);
    }
}

static void describe_event_mask(uint32_t mask, char *out, size_t out_size) {
    if (!out || out_size == 0) return;

    out[0] = '\0';

    if (mask & IN_CLOSE_WRITE) {
        strncat(out, "IN_CLOSE_WRITE ", out_size - strlen(out) - 1);
    }

    if (mask & IN_MOVED_TO) {
        strncat(out, "IN_MOVED_TO ", out_size - strlen(out) - 1);
    }

    if (mask & IN_CREATE) {
        strncat(out, "IN_CREATE ", out_size - strlen(out) - 1);
    }

    if (mask & IN_ISDIR) {
        strncat(out, "IN_ISDIR ", out_size - strlen(out) - 1);
    }

    if (out[0] == '\0') {
        snprintf(out, out_size, "mask=0x%x", mask);
    }
}

int main(int argc, char **argv) {
    const char *watch_dir = ".";

    if (argc >= 2) {
        watch_dir = argv[1];
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    fprintf(stderr, "[WATCHD] imeta-watchd starting\n");
    fprintf(stderr, "[WATCHD] watching directory: %s\n", watch_dir);
    fprintf(stderr, "[WATCHD] binding policy: IN_CLOSE_WRITE | IN_MOVED_TO only\n");

    int fd = inotify_init1(0);

    if (fd < 0) {
        fprintf(stderr, "[WATCHD][ERROR] inotify_init1 failed: %s\n", strerror(errno));
        return 1;
    }

    int wd = inotify_add_watch(
        fd,
        watch_dir,
        IN_MOVED_TO | IN_CLOSE_WRITE
    );

    if (wd < 0) {
        fprintf(stderr, "[WATCHD][ERROR] inotify_add_watch failed: %s: %s\n",
                watch_dir,
                strerror(errno));
        close(fd);
        return 1;
    }

    fprintf(stderr, "[WATCHD] ready\n");

    char buffer[EVENT_BUF_LEN];

    while (g_running) {
        ssize_t length = read(fd, buffer, sizeof(buffer));

        if (length < 0) {
            if (errno == EINTR) {
                continue;
            }

            fprintf(stderr, "[WATCHD][ERROR] read failed: %s\n", strerror(errno));
            break;
        }

        ssize_t offset = 0;

        while (offset < length) {
            struct inotify_event *event = (struct inotify_event *)&buffer[offset];

            if (event->len > 0) {
                char mask_desc[128];
                describe_event_mask(event->mask, mask_desc, sizeof(mask_desc));

                fprintf(stderr, "[WATCHD] event name=%s mask=%s\n", event->name, mask_desc);

                if (event->mask & IN_ISDIR) {
                    fprintf(stderr, "[WATCHD] ignored directory event: %s\n", event->name);
                } else if (
                    (event->mask & IN_MOVED_TO) ||
                    (event->mask & IN_CLOSE_WRITE)
                ) {
                    maybe_bind_finished_file(watch_dir, event->name);
                }
            }

            offset += (ssize_t)(sizeof(struct inotify_event) + event->len);
        }
    }

    fprintf(stderr, "[WATCHD] shutting down\n");

    inotify_rm_watch(fd, wd);
    close(fd);

    return 0;
}
