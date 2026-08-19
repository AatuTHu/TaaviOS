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
#define SYS_SBRK 45
#define SYS_IOCTL 54
#define SYS_IDLE 112
#define SYS_GETDENTS 141
#define SYS_YIELD 158
#define SYS_WI 250

typedef struct {
    char *buf;
    uint32_t *pixels;
    uint32_t buffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t scale;
    uint16_t struct_key;
    uint8_t opcode;
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
    RESIZE,
} operations_t;

#define O_RDONLY 0x01
#define O_WRONLY 0x02
#define O_RDWR 0x04
#define O_CREAT 0x08
#define O_APPEND 0x10

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
#define COLOR_GRIMACE 0x7F3AE8
#define COLOR_ICE_BLUE 0xF0F8FF
#define COLOR_POWDER_BLUE 0xB0E0E6
#define COLOR_DEEP_BLUE 0x131185
#define COLOR_DARK_GRAY 0x404040
#define COLOR_DARKER_GRAY 0x202020
#define COLOR_LIGHT_GRAY 0xC0C0C0
#define COLOR_SILVER 0xE0E0E0
#define COLOR_MAROON 0x800000
#define COLOR_DARK_RED 0x8B0000
#define COLOR_CRIMSON 0xDC143C
#define COLOR_SALMON 0xFA8072
#define COLOR_DARK_GREEN 0x006400
#define COLOR_FOREST_GREEN 0x228B22
#define COLOR_LIME 0x32CD32
#define COLOR_OLIVE 0x808000
#define COLOR_DARKER_GREEN 0x012401
#define COLOR_MINT 0x98FF98
#define COLOR_NAVY 0x000080
#define COLOR_TEAL 0x008080
#define COLOR_STEEL_BLUE 0x4682B4
#define COLOR_INDIGO 0x4B0082
#define COLOR_PURPLE 0x800080
#define COLOR_VIOLET 0xEE82EE
#define COLOR_LAVENDER 0xE6E6FA
#define COLOR_PLUM 0xDDA0DD
#define COLOR_PINK 0xFFC0CB
#define COLOR_HOT_PINK 0xFF69B4
#define COLOR_ROSE 0xFF007F
#define COLOR_GOLD 0xFFD700
#define COLOR_AMBER 0xFFBF00
#define COLOR_BROWN 0x8B4513
#define COLOR_TAN 0xD2B48C
#define COLOR_BEIGE 0xF5F5DC
#define COLOR_CORAL 0xFF7F50
#define COLOR_TURQUOISE 0x40E0D0
#define COLOR_AQUAMARINE 0x7FFFD4
#define COLOR_CHARTREUSE 0x7FFF00

#endif
