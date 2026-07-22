#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>

#define MAX_SYSCALLS 256

#define SYS_EXIT 1
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_CHDIR 12
#define SYS_GETPID 20
#define SYS_EXEC 11
#define SYS_MKDIR 39
#define SYS_IDLE 112
#define SYS_GETDENTS 141
#define SYS_YIELD 158
#define SYS_CONWI 250

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    uint32_t color;
    uint32_t *pixels;
} gui_calls_t;

typedef enum {
    W_CREATE,
    W_PAINT,
    W_MOVE,
    W_SET_OPERATOR,
    W_CH_ACT_W,
    W_CH_BG_COLOR,
    W_CH_FG_COLOR,
    W_DRAW_BUFFER,
} window_operations;

typedef enum {
    WRITE,
    READ,
    UPDATE, // write at offset?
    DELETE,
    FREE,
    OPEN,
    CLOSE,
    CREATE,
    FIND,
    LIST,
    O_R, // Open and read
    PAINT_WINDOW,
    MOVE,
    BG_COLOR,
    FG_COLOR,
    DRAW
} operations_t;

#define O_RDONLY 0x01
#define O_WRONLY 0x02
#define O_RDWR 0x04
#define O_CREAT 0x08

#define COLOR_BLACK 0x000000
#define COLOR_WHITE 0xFFFFFF
#define COLOR_RED 0xFF0000
#define COLOR_GREEN 0x00FF00
#define COLOR_BLUE 0x0000FF
#define COLOR_YELLOW 0xFFFF00
#define COLOR_CYAN 0x00FFFF
#define COLOR_MAGENTA 0xFF00FF
#define COLOR_GRAY 0x808080
#define COLOR_ORANGE 0xFF8000
#define COLOR_QUARTZ 0xDDDDFF
#define COLOR_SKY_BLUE 0x87CEEB
#define COLOR_ICE_BLUE 0xF0F8FF
#define COLOR_POWDER_BLUE 0xB0E0E6

#endif