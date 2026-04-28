#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ivault.h"

static void usage(void) {
    printf("ivault - WolverineOS resurrection vault\n\n");
    printf("Usage:\n");
    printf("  ivault seal <target-folder>\n");
    printf("  ivault verify <target-folder> <vault-folder>\n");
    printf("  ivault restore <target-folder> <vault-folder>\n");
    printf("  ivault prune <target-folder> <vault-folder> [--delete]\n");
    printf("  ivault diff <vault-a> <vault-b>\n");
    printf("  ivault latest\n");
    printf("  ivault inspect <vault-folder>\n");
    printf("  ivault recover-file <vault-folder> <relpath> <output-path>\n");
    printf("  ivault timeline\n");
    printf("  ivault history\n");
    printf("  ivault audit\n");
    printf("  ivault watch <target-folder> <vault-folder> [--interval N] [--cycles N] [--auto-restore] [--auto-prune] [--delete]\n");
}

static int parse_positive_int(const char *s, int fallback) {
    if (!s || !*s) return fallback;

    char *end = NULL;
    long v = strtol(s, &end, 10);

    if (end == s || *end != '\0' || v <= 0 || v > 86400) {
        return fallback;
    }

    return (int)v;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], "seal") == 0) {
        if (argc != 3) {
            usage();
            return 1;
        }

        int rc = ivault_seal_path(argv[2]);
        ivault_ledger_append("SEAL", argv[2], "vaults/latest", rc == 0 ? "SEALED" : "ERROR", "seal complete");
        return rc;
    }

    if (strcmp(argv[1], "verify") == 0) {
        if (argc != 4) {
            usage();
            return 1;
        }

        int rc = ivault_verify_path(argv[2], argv[3]);
        ivault_ledger_append("VERIFY", argv[2], argv[3], rc == 0 ? "CLEAN" : "DAMAGE", "verify complete");
        return rc;
    }

    if (strcmp(argv[1], "restore") == 0) {
        if (argc != 4) {
            usage();
            return 1;
        }

        int rc = ivault_restore_path(argv[2], argv[3]);
        ivault_ledger_append("RESTORE", argv[2], argv[3], rc == 0 ? "RESTORE_COMPLETE" : "ERROR", "restore complete");
        return rc;
    }

    if (strcmp(argv[1], "prune") == 0) {
        if (argc != 4 && argc != 5) {
            usage();
            return 1;
        }

        int delete_mode = 0;
        if (argc == 5) {
            if (strcmp(argv[4], "--delete") != 0) {
                usage();
                return 1;
            }
            delete_mode = 1;
        }

        int rc = ivault_prune_path(argv[2], argv[3], delete_mode);
        ivault_ledger_append("PRUNE", argv[2], argv[3], rc == 0 ? "PRUNE_COMPLETE" : "ERROR", delete_mode ? "delete" : "quarantine");
        return rc;
    }

    if (strcmp(argv[1], "diff") == 0) {
        if (argc != 4) {
            usage();
            return 1;
        }

        return ivault_diff_vaults(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "latest") == 0) {
        if (argc != 2) {
            usage();
            return 1;
        }

        return ivault_latest_print();
    }

    if (strcmp(argv[1], "inspect") == 0) {
        if (argc != 3) {
            usage();
            return 1;
        }

        return ivault_inspect_vault(argv[2]);
    }

    if (strcmp(argv[1], "recover-file") == 0) {
        if (argc != 5) {
            usage();
            return 1;
        }

        return ivault_recover_file(argv[2], argv[3], argv[4]);
    }

    if (strcmp(argv[1], "timeline") == 0) {
        if (argc != 2) {
            usage();
            return 1;
        }

        return ivault_timeline_print();
    }

    if (strcmp(argv[1], "history") == 0) {
        if (argc != 2) {
            usage();
            return 1;
        }

        return ivault_history_print();
    }

    if (strcmp(argv[1], "audit") == 0) {
        if (argc != 2) {
            usage();
            return 1;
        }

        return ivault_audit_print();
    }

    if (strcmp(argv[1], "watch") == 0) {
        if (argc < 4) {
            usage();
            return 1;
        }

        int interval = 5;
        int cycles = 0;
        int auto_restore = 0;
        int auto_prune = 0;
        int delete_mode = 0;

        for (int i = 4; i < argc; i++) {
            if (strcmp(argv[i], "--interval") == 0) {
                if (i + 1 >= argc) {
                    usage();
                    return 1;
                }
                interval = parse_positive_int(argv[i + 1], 5);
                i++;
            } else if (strcmp(argv[i], "--cycles") == 0) {
                if (i + 1 >= argc) {
                    usage();
                    return 1;
                }
                cycles = parse_positive_int(argv[i + 1], 1);
                i++;
            } else if (strcmp(argv[i], "--auto-restore") == 0) {
                auto_restore = 1;
            } else if (strcmp(argv[i], "--auto-prune") == 0) {
                auto_prune = 1;
            } else if (strcmp(argv[i], "--delete") == 0) {
                delete_mode = 1;
            } else {
                usage();
                return 1;
            }
        }

        return ivault_watch_path(argv[2], argv[3], interval, cycles, auto_restore, auto_prune, delete_mode);
    }

    usage();
    return 1;
}
