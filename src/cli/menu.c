#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_INPUT 32
#define MAX_VAULTS 256
#define MAX_NAME 256

typedef enum {
    SCREEN_HOME,
    SCREEN_VAULTS,
    SCREEN_MEMORY,
    SCREEN_LOGS,
    SCREEN_STATUS
} Screen;

static Screen stack[16];
static int stack_top = 0;

static char vault_names[MAX_VAULTS][MAX_NAME];
static int vault_count = 0;

void push(Screen s) {
    if (stack_top < 15) stack[++stack_top] = s;
}

void pop(void) {
    if (stack_top > 0) stack_top--;
}

Screen current(void) {
    return stack[stack_top];
}

void scan_vaults(void) {
    vault_count = 0;

    DIR *dir = opendir("./vaults");
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && vault_count < MAX_VAULTS) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        strncpy(vault_names[vault_count], entry->d_name, MAX_NAME - 1);
        vault_names[vault_count][MAX_NAME - 1] = '\0';
        vault_count++;
    }

    closedir(dir);
}

void draw_home(void) {
    printf("\nWolverineOS\n\n");
    printf("(1) Vaults\n");
    printf("(2) Memory\n");
    printf("(3) Logs\n");
    printf("(4) Status\n\n");
    printf("Commands:\n[number] select\n(b) back\n(q) quit\n(h) help\n\n> ");
}

void draw_vaults(void) {
    scan_vaults();

    printf("\nVaults\n\n");

    if (vault_count == 0) {
        printf("Nothing here yet.\n\n");
    } else {
        for (int i = 0; i < vault_count; i++) {
            printf("(%d) %s\n", i + 1, vault_names[i]);
        }
        printf("\n");
    }

    printf("Commands:\n[number] open vault\n(b) back\n(q) quit\n(h) help\n\n> ");
}

void draw_simple(const char *title) {
    printf("\n%s\n\n", title);
    printf("Nothing here yet.\n\n");
    printf("Commands:\n(b) back\n(q) quit\n(h) help\n\n> ");
}

void draw(void) {
    switch (current()) {
        case SCREEN_HOME:   draw_home(); break;
        case SCREEN_VAULTS: draw_vaults(); break;
        case SCREEN_MEMORY: draw_simple("Memory"); break;
        case SCREEN_LOGS:   draw_simple("Logs"); break;
        case SCREEN_STATUS: draw_simple("Status"); break;
    }
}

void help(void) {
    printf("\nHelp\n\n");
    printf("number = move forward / open item\n");
    printf("b      = go back\n");
    printf("q      = quit\n");
    printf("h      = help\n\n");
    printf("Press enter to return...");
    getchar();
}

void open_vault(int index) {
    if (index < 0 || index >= vault_count) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "xdg-open './vaults/%s' >/dev/null 2>&1 &",
             vault_names[index]);
    system(cmd);
}

int main(void) {
    char input[MAX_INPUT];

    stack[0] = SCREEN_HOME;

    while (1) {
        draw();

        if (!fgets(input, sizeof(input), stdin))
            continue;

        if (input[0] == 'q') {
            printf("\nExiting.\n");
            break;
        }

        if (input[0] == 'b') {
            pop();
            continue;
        }

        if (input[0] == 'h') {
            help();
            continue;
        }

        if (current() == SCREEN_HOME) {
            if (input[0] == '1') push(SCREEN_VAULTS);
            else if (input[0] == '2') push(SCREEN_MEMORY);
            else if (input[0] == '3') push(SCREEN_LOGS);
            else if (input[0] == '4') push(SCREEN_STATUS);
            else printf("\nUnknown choice.\n");
        }
        else if (current() == SCREEN_VAULTS) {
            int choice = atoi(input);
            if (choice >= 1 && choice <= vault_count) {
                open_vault(choice - 1);
            } else {
                printf("\nUnknown choice.\n");
            }
        }
        else {
            printf("\nUnknown choice.\n");
        }
    }

    return 0;
}
