#include "ledger.h"
#include "kmalloc.h"
#include "klog.h"
#include "config.h"
#include "kstring.h"

/*
* Ledger protocol
* Design & Implementation: A.H, 2026
*/

static ledger_t *ledger_table[MAX_LEDGER_TASKS];
static uint8_t ledger_slot = 0;
static uint16_t alloc_slot = 0;

int ledger_register(uint32_t pid) {
    DEBUG("[LEDGER][REGISTER]: Registering new ledger entry\n");
    ledger_t *entry = (ledger_t *)kmalloc(sizeof(ledger_t));

    if(entry == NULL) {
        DEBUG("[LEDGER][REGISTER]: Kmalloc failed. Aborting\n");
        return STATUS_ERROR;
    }

    entry->task_base = LEDGER_BASE + (ledger_slot * LEDGER_FLOOR_SIZE);
    entry->pid       = pid;
    entry->active    = ACTIVE;
    entry->mem_cursor= 0;

    for(uint32_t i = 0; i < MAX_LEDGER_ALLOCS; i++) {
        memset(&entry->allocations[i], 0, sizeof(ledger_alloc_t));
    }

    ledger_table[ledger_slot] = entry;
    ledger_slot++;

    DEBUG("[LEDGER][REGISTER]: Registering complite\n");
    return STATUS_OK;
}

uint32_t ledger_alloc(uint32_t pid, uint32_t size) {
    DEBUG("[LEDGER][ALLOC]: Ledger starting to allocate for %d, size of :%d\n", pid, size);
    for(uint32_t i = 0; i < ledger_slot; i++) {
        if(ledger_table[i] != NULL && ledger_table[i]->pid == pid) {
            uint32_t paddr = (uint32_t)kmalloc(size);

            if(paddr == NULL) {
                DEBUG("[LEDGER][ALLOC]: Kmalloc failed. Aborting\n");
                return 0;
            }

            ledger_alloc_t *entry = ledger_table[i]->allocations;
            for(uint32_t j = 0; j < MAX_LEDGER_ALLOCS; j++) {
                if(entry[j].size == 0) {
                    DEBUG("[LEDGER][ALLOC]: Empty slot found, writing in to it\n");
                    entry[j].real_addr = paddr;
                    entry[j].size = size;
                    entry[j].protocol_addr = 
                        ledger_table[i]->task_base + ledger_table[i]->mem_cursor;
                        ledger_table[i]->mem_cursor += size;
                        return entry[j].protocol_addr;
                }
            }


        }
    }

    DEBUG("[LEDGER][ALLOC]: Kmalloc failed. Aborting\n");
    return 0;
}

uint32_t ledger_validate(uint32_t pid, uint32_t protocol_addr) {
    DEBUG("[LEDGER][VALIDATE]: Validating: 0x%x\n", protocol_addr);
    for(uint32_t i = 0; i < ledger_slot; i++) {
      if(ledger_table[i] != NULL && ledger_table[i]->pid == pid) {

        if(protocol_addr >= ledger_table[i]->task_base && protocol_addr < (ledger_table[i]->task_base + ledger_table[i]->mem_cursor)) {
         for(uint32_t j = 0; j < MAX_LEDGER_ALLOCS; j++) {
            ledger_t *entry = ledger_table[i];
            if(entry->allocations[j].protocol_addr == protocol_addr) {
              return entry->allocations[j].real_addr;
            }
          }
        }
      }
    }
    DEBUG("[LEDGER][VALIDATE]: Invalid protocol address \n");
    return 0;
}

int ledger_free(uint32_t pid, uint32_t protocol_addr) {

    DEBUG("[LEDGER][FREE]: Freeing protocol_address: 0x%x \n", protocol_addr);
    uint32_t paddr = ledger_validate(pid, protocol_addr);

    if(paddr != 0) {
       kfree(paddr);

       for(uint32_t i = 0; i < ledger_slot; i++) {
           if(ledger_table[i] != NULL && ledger_table[i]->pid == pid) {
                 for(uint32_t j = 0; j < MAX_LEDGER_ALLOCS; j++) {
                    ledger_t *entry = ledger_table[i];
                    if(entry->allocations[j].protocol_addr == protocol_addr) {
                        DEBUG("[LEDGER][FREE]: Coller allocation index found, freeing\n");
                        ledger_table[i]->mem_cursor -= entry->allocations[j].size;
                        memset(&entry->allocations[j], 0, sizeof(ledger_alloc_t));
                        DEBUG("[LEDGER][FREE]: Success\n");
                        return STATUS_OK;
                    }
                }
           }
       }
    } 

    DEBUG("[LEDGER][FREE]: Invalid protocol_address\n");
    return STATUS_ERROR;
}