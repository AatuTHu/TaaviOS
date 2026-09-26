#ifndef HISTORY_H
#define HISTORY_H

#define MAX_SAVED_LINES 32
#define HISTORY_LINE_LEN 512

void history_add(const char *line);
const char *history_get_next();
const char *history_get_prev();

#endif
