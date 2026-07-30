#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>

#define MAX_SYSCALLS 256

#define SYS_EXIT 1
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_EXEC 11
#define SYS_CHDIR 12
#define SYS_GETPID 20
#define SYS_KILL 37
#define SYS_MKDIR 39
#define SYS_IDLE 112
#define SYS_GETDENTS 141
#define SYS_YIELD 158
#define SYS_WI 250

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    uint32_t scale;
    uint32_t *pixels;
} gui_params_pack;

typedef enum {
    WRITE,
    WRITE_AT,
    READ,
    UPDATE,
    DELETE,
    FREE,
    OPEN,
    CLOSE,
    CREATE,
    FIND,
    LIST,
    PAINT_WINDOW,
    MOVE,
    BG_COLOR,
    FG_COLOR,
    DRAW,
    SET_OPERATOR,
    CH_ACT_W,
    SCROLL_DOWN,
} operations_t;

#define O_RDONLY 0x01
#define O_WRONLY 0x02
#define O_RDWR 0x04
#define O_CREAT 0x08

#define TRANSPARENT 0x000000
#define COLOR_BLACK 0x010101
#define COLOR_MAGENTA 0xFF00FF
#define COLOR_WHITE 0xFFFFFF
#define COLOR_RED 0xFF0000
#define COLOR_GREEN 0x00FF00
#define COLOR_BLUE 0x0000FF
#define COLOR_YELLOW 0xFFFF00
#define COLOR_CYAN 0x00FFFF
#define COLOR_GRAY 0x808080
#define COLOR_ORANGE 0xFF8000
#define COLOR_QUARTZ 0xDDDDFF
#define COLOR_SKY_BLUE 0x87CEEB
#define COLOR_ICE_BLUE 0xF0F8FF
#define COLOR_POWDER_BLUE 0xB0E0E6

#endif
