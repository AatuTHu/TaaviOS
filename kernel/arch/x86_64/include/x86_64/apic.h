#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h>

#define IA32_APIC_BASE_MSR 0x1B
#define APIC_BASE_ENABLE (1ULL << 11)
#define APIC_BASE_BSP (1ULL << 8)
#define APIC_BASE_ADDR_MASK 0xFFFFFFFFFFFFF000ULL

#define LAPIC_REG_ID 0x020
#define LAPIC_REG_VERSION 0x030
#define LAPIC_REG_TPR 0x080
#define LAPIC_REG_EOI 0x0B0
#define LAPIC_REG_SPURIOUS 0x0F0
#define LAPIC_REG_LVT_TIMER 0x320
#define LAPIC_REG_TIMER_INIT 0x380
#define LAPIC_REG_TIMER_CUR 0x390
#define LAPIC_REG_TIMER_DIV 0x3E0

#define LAPIC_SPURIOUS_ENABLE (1u << 8)
#define LAPIC_SPURIOUS_VECTOR 0xFF

#define LAPIC_LVT_MASKED (1u << 16)
#define LAPIC_LVT_PERIODIC (1u << 17)
#define LAPIC_TIMER_DIV_16 0x3

#define LAPIC_TIMER_VECTOR 0x40

#define PIT_FREQ_HZ 1193182
#define PIT_CMD 0x43
#define PIT_CH2_DATA 0x42
#define PIT_GATE_PORT 0x61

#define IOAPIC_REGSEL 0x00
#define IOAPIC_WIN 0x10

#define IOAPIC_REG_ID 0x00
#define IOAPIC_REG_VER 0x01
#define IOAPIC_REG_REDIR(n) (0x10 + 2 * (n))

#define IOAPIC_REDIR_POL_LOW (1u << 13)
#define IOAPIC_REDIR_LEVEL (1u << 15)
#define IOAPIC_REDIR_MASKED (1u << 16)

#define IOAPIC_MADT_POL_MASK 0x3
#define IOAPIC_MADT_POL_HIGH 0x1
#define IOAPIC_MADT_POL_LOW 0x3
#define IOAPIC_MADT_TRIG_SHIFT 2
#define IOAPIC_MADT_TRIG_MASK 0x3
#define IOAPIC_MADT_TRIG_EDGE 0x1
#define IOAPIC_MADT_TRIG_LEVEL 0x3

#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

#define PIC_ICW1_INIT 0x10
#define PIC_ICW1_ICW4 0x01
#define PIC_ICW4_8086 0x01

#define PIC1_VECTOR_BASE 0x20
#define PIC2_VECTOR_BASE 0x28

int ioapic_route_gsi(uint32_t gsi, uint8_t vector,
                     uint8_t dest_apic_id, uint16_t madt_flags);
int ioapic_route_isa(uint8_t irq, uint8_t vector, uint8_t dest_apic_id);
int ioapic_mask_gsi(uint32_t gsi, int masked);

void lapic_tick_ticks();
void lapic_eoi(void);
uint32_t lapic_id(void);
uint64_t lapic_get_ticks();
int lapic_timer_start_periodic(uint32_t ms);
int apic_init();
#endif
