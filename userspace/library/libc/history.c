#include "history.h"
#include "string.h"
#include <stdbool.h>
#include <stdint.h>

static char lines[MAX_SAVED_LINES][HISTORY_LINE_LEN];
static uint8_t total_saved  = 0;
static uint8_t current_line = 0;
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

    if (total_saved >= MAX_SAVED_LINES) {
        total_saved = 0;
    }

    current_line = total_saved;
    strncpy(lines[current_line], line, HISTORY_LINE_LEN - 1);
    lines[current_line][HISTORY_LINE_LEN - 1] = '\0';

    total_saved++;
}

const char *history_get_next() {
    history_init();
    if (total_saved == 0) {
        return NULL;
    }

    if (current_line >= MAX_SAVED_LINES) {
        current_line = total_saved - 1;
    }

    const char *cmd = lines[current_line];
    current_line--;
    return cmd;
}

const char *history_get_prev() {
    history_init();
    if (total_saved == 0) {
        return NULL;
    }

    if (current_line >= MAX_SAVED_LINES || current_line >= total_saved) {
        current_line = total_saved - 1;
    }

    const char *cmd = lines[current_line];
    current_line++;
    return cmd;
}
