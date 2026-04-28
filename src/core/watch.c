#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

#include "ivault.h"

static void sleep_seconds_int(int seconds) {
    if (seconds <= 0) seconds = 5;
    sleep((unsigned int)seconds);
}

static void watch_timestamp(char *out, size_t out_size) {
    time_t now = time(NULL);
    struct tm *local_tm = localtime(&now);

    if (local_tm) {
        strftime(out, out_size, "%Y-%m-%dT%H:%M:%S%z", local_tm);
    } else {
        snprintf(out, out_size, "unknown-time");
    }
}

int ivault_watch_path(
    const char *target_path,
    const char *vault_path,
    int interval_seconds,
    int cycles,
    int auto_restore,
    int auto_prune,
    int delete_mode
) {
    if (!target_path || !vault_path) {
        fprintf(stderr, "[IVAULT][ERROR] watch requires target and vault\n");
        return 1;
    }

    if (interval_seconds <= 0) {
        interval_seconds = 5;
    }

    printf("ivault watch\n");
    printf("target:        %s\n", target_path);
    printf("vault:         %s\n", vault_path);
    printf("interval:      %d seconds\n", interval_seconds);
    printf("cycles:        %s\n", cycles <= 0 ? "infinite" : "bounded");
    if (cycles > 0) {
        printf("cycle count:   %d\n", cycles);
    }
    printf("auto-restore:  %s\n", auto_restore ? "on" : "off");
    printf("auto-prune:    %s\n", auto_prune ? "on" : "off");
    printf("prune mode:    %s\n\n", delete_mode ? "delete" : "quarantine");

    ivault_ledger_append(
        "WATCH_START",
        target_path,
        vault_path,
        "STARTED",
        auto_restore ? (auto_prune ? "auto_restore auto_prune" : "auto_restore") : "observe_only"
    );

    int cycle = 0;
    int damage_events = 0;
    int restore_events = 0;
    int prune_events = 0;
    int clean_events = 0;
    int error_events = 0;

    while (cycles <= 0 || cycle < cycles) {
        cycle++;

        char ts[64];
        watch_timestamp(ts, sizeof(ts));

        printf("\n[WATCH] cycle=%d time=%s\n", cycle, ts);

        int verify_rc = ivault_verify_path(target_path, vault_path);

        if (verify_rc == 0) {
            clean_events++;
            printf("[WATCH] status=CLEAN cycle=%d\n", cycle);
            ivault_ledger_append("WATCH_VERIFY", target_path, vault_path, "CLEAN", "watch cycle clean");
        } else {
            damage_events++;
            printf("[WATCH] status=DAMAGE_DETECTED cycle=%d\n", cycle);
            ivault_ledger_append("WATCH_VERIFY", target_path, vault_path, "DAMAGE", "watch cycle detected damage");

            if (auto_restore) {
                printf("[WATCH] auto-restore=RUNNING cycle=%d\n", cycle);
                int restore_rc = ivault_restore_path(target_path, vault_path);

                if (restore_rc == 0) {
                    restore_events++;
                    ivault_ledger_append("WATCH_RESTORE", target_path, vault_path, "RESTORE_COMPLETE", "watch auto-restore complete");
                } else {
                    error_events++;
                    ivault_ledger_append("WATCH_RESTORE", target_path, vault_path, "ERROR", "watch auto-restore failed");
                }
            }

            if (auto_prune) {
                printf("[WATCH] auto-prune=RUNNING cycle=%d\n", cycle);
                int prune_rc = ivault_prune_path(target_path, vault_path, delete_mode);

                if (prune_rc == 0) {
                    prune_events++;
                    ivault_ledger_append("WATCH_PRUNE", target_path, vault_path, "PRUNE_COMPLETE", delete_mode ? "delete" : "quarantine");
                } else {
                    error_events++;
                    ivault_ledger_append("WATCH_PRUNE", target_path, vault_path, "ERROR", delete_mode ? "delete failed" : "quarantine failed");
                }
            }

            if (auto_restore || auto_prune) {
                printf("[WATCH] post-heal verify cycle=%d\n", cycle);
                int post_rc = ivault_verify_path(target_path, vault_path);
                if (post_rc == 0) {
                    clean_events++;
                    ivault_ledger_append("WATCH_POST_VERIFY", target_path, vault_path, "CLEAN", "post-heal clean");
                } else {
                    error_events++;
                    ivault_ledger_append("WATCH_POST_VERIFY", target_path, vault_path, "DAMAGE", "post-heal still damaged");
                }
            }
        }

        if (cycles <= 0 || cycle < cycles) {
            sleep_seconds_int(interval_seconds);
        }
    }

    printf("\nIVAULT WATCH REPORT\n");
    printf("CYCLES: %d\n", cycle);
    printf("CLEAN EVENTS: %d\n", clean_events);
    printf("DAMAGE EVENTS: %d\n", damage_events);
    printf("RESTORE EVENTS: %d\n", restore_events);
    printf("PRUNE EVENTS: %d\n", prune_events);
    printf("ERROR EVENTS: %d\n", error_events);
    printf("STATUS: %s\n", error_events == 0 ? "WATCH COMPLETE" : "WATCH COMPLETE WITH ERRORS");

    ivault_ledger_append(
        "WATCH_END",
        target_path,
        vault_path,
        error_events == 0 ? "WATCH_COMPLETE" : "WATCH_ERRORS",
        "watch command ended"
    );

    return error_events == 0 ? 0 : 1;
}
