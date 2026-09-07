#include "history.h"
#include "stdbool.h"
#include "string.h"

static char lines[MAX_SAVED_LINES][HISTORY_LINE_LEN];
static int total_saved      = 0;
static bool has_initialized = false;

void history_init(void) {
    if (!has_initialized) {
        memset(lines, 0, sizeof(lines));
        has_initialized = true;
    }
}

void history_add(const char *line) {
    if (line == NULL || line[0] == '\0')
        return;

    history_init();

    int slot = total_saved % MAX_SAVED_LINES;
    strncpy(lines[slot], line, HISTORY_LINE_LEN - 1);
    lines[slot][HISTORY_LINE_LEN - 1] = '\0';

    total_saved++;
}

int history_count(void) {
    return total_saved;
}

const char *history_get(int index) {
    if (index < 0 || index >= total_saved || index < (total_saved - MAX_SAVED_LINES)) {
        return NULL;
    }
    return lines[index % MAX_SAVED_LINES];
}
