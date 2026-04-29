#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT 32

typedef enum {
    SCREEN_HOME,
    SCREEN_VAULTS,
    SCREEN_MEMORY,
    SCREEN_LOGS,
    SCREEN_STATUS
} Screen;

static Screen stack[16];
static int stack_top = 0;

void push(Screen s) {
    if (stack_top < 15) stack[++stack_top] = s;
}

void pop(void) {
    if (stack_top > 0) stack_top--;
}

Screen current(void) {
    return stack[stack_top];
}

void draw_home(void) {
    printf("\nWolverineOS\n\n");
    printf("(1) Vaults\n");
    printf("(2) Memory\n");
    printf("(3) Logs\n");
    printf("(4) Status\n\n");

    printf("Commands:\n");
    printf("[number] select\n");
    printf("(b) back\n");
    printf("(q) quit\n");
    printf("(h) help\n\n");

    printf("> ");
}

void draw_simple(const char *title) {
    printf("\n%s\n\n", title);
    printf("Nothing here yet.\n\n");

    printf("Commands:\n");
    printf("(b) back\n");
    printf("(q) quit\n");
    printf("(h) help\n\n");

    printf("> ");
}

void draw(void) {
    switch (current()) {
        case SCREEN_HOME:   draw_home(); break;
        case SCREEN_VAULTS: draw_simple("Vaults"); break;
        case SCREEN_MEMORY: draw_simple("Memory"); break;
        case SCREEN_LOGS:   draw_simple("Logs"); break;
        case SCREEN_STATUS: draw_simple("Status"); break;
    }
}

void help(void) {
    printf("\nHelp\n\n");
    printf("number = move forward\n");
    printf("b      = go back\n");
    printf("q      = quit\n");
    printf("h      = help\n\n");
    printf("Press enter to return...");
    getchar();
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
            else {
                printf("\nUnknown choice.\n");
            }
        } else {
            printf("\nUnknown choice.\n");
        }
    }

    return 0;
}
