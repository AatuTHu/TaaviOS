#ifndef HISTORY_H
#define HISTORY_H

#define MAX_SAVED_LINES 32
#define HISTORY_LINE_LEN 512

void history_init(void);
void history_add(const char *line);
const char *history_get(int index);
int history_count(void);

#endif
