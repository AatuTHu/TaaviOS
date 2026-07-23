#include "hail_mary.h"
#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include <stdint.h>

/*
 * Hail_mary protocol
 * Design & Implementation: A.H, 2026
 */

hail_mary_t *gosling_table[CLERK_COUNT];

void register_hail_mary_function(uint32_t pid, void (*cb)(void)) {
    for (uint8_t i = 0; i < CLERK_COUNT; i++) {
        if (gosling_table[i] == NULL) {
            hail_mary_t *entry = (hail_mary_t *)kmalloc(sizeof(hail_mary_t));
            if (entry == NULL) {
                ERROR("[HAIL_MARY][REGISTER] Unable to allocate space for hail_mary function\n");
                return;
            }
            entry->pid       = pid;
            entry->cb        = cb;
            gosling_table[i] = entry;
        }
    }
}
void activate_hail_mary(uint32_t pid) {
    for (int i = 0; i < CLERK_COUNT; i++) {
        if (gosling_table[i] != NULL && gosling_table[i]->pid == pid) {
            DEBUG("[HAILMARY][ACTIVATE]: activating hailmary!\n");
            gosling_table[i]->cb();
            return;
        }
    }
}
