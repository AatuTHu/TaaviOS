#ifndef BLANKIE_H
#define BLANKIE_H

#include <stdint.h>

typedef struct {
    uint32_t pid;
    uint32_t entry_point;
    uint32_t stack_top;
} blankie_registry_t;

int blankie_register(uint32_t pid, uint32_t entry_point, uint32_t stack_top);
int blankie_activate(uint32_t pid);

#endif