#ifndef LEDGER_H
#define LEDGER_H

#include <stdint.h>
#include "config.h"

#define POLICE_BASE       0xD0000000
#define POLICE_FLOOR_SIZE 0x40000
#define MAX_POLICE_TASKS  15

typedef struct {
    uint32_t protocol_addr;
    uint32_t real_addr;
    uint32_t size;
} police_alloc_t;

typedef struct {
    uint32_t pid;
    uint32_t task_base;
    uint32_t mem_cursor;
    uint8_t  active;
    police_alloc_t allocations[MAX_TASKS];
} police_ledger_t;

int police_register(uint32_t pid);
uint32_t police_alloc(uint32_t pid, uint32_t size);
uint32_t police_validate(uint32_t pid, uint32_t protocol_addr);
int police_free(uint32_t pid, uint32_t protocol_addr);

#endif