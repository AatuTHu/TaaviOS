#ifndef STAND_H
#define STAND_H

void write(const char *msg);
int read(char *buf);
int exec(const char *filename);
void terminate_program();
void idle(void);
#endif