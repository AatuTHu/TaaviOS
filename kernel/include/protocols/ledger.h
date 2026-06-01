#ifndef LEDGER_H
#define LEDGER_H

#include <stdint.h>
#include "config.h"

#define LEDGER_BASE       0xD0000000
#define LEDGER_FLOOR_SIZE 0x40000
#define MAX_LEDGER_TASKS  15
#define MAX_LEDGER_ALLOCS 256
#define ACTIVE            1
#define INACTIVE          0

typedef struct {
    uint32_t protocol_addr;
    uint32_t real_addr;
    uint32_t size;
} ledger_alloc_t;

typedef struct {
    uint32_t pid;
    uint32_t task_base;
    uint32_t mem_cursor;
    uint8_t  active;
    ledger_alloc_t allocations[MAX_LEDGER_ALLOCS];
} ledger_t;

int ledger_register(uint32_t pid);
uint32_t ledger_alloc(uint32_t pid, uint32_t size);
uint32_t ledger_validate(uint32_t pid, uint32_t protocol_addr);
int ledger_free(uint32_t pid, uint32_t protocol_addr);

#endif