#ifndef HAIL_MARY_H
#define HAIL_MARY_H

#include "config.h"
#include <stdint.h>

typedef struct {
    uint32_t pid;
    void (*cb)(void);
} hail_mary_t;

void register_hail_mary_function(uint32_t pid, void (*cb)(void));
void activate_hail_mary(uint32_t pid);

#endif