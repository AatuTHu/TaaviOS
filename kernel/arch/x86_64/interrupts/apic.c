#include "apic.h"
#include "acpi.h"
#include "config.h"
#include "io.h"
#include "klog.h"
#include "paging.h"
#include <stdint.h>

static volatile uint64_t ticks;
static volatile uint32_t *lapic_base;
static uint32_t lapic_ticks_per_ms;

static struct {
    volatile uint8_t *base;
    uint32_t pins;
} ioapic_rt[ACPI_MAX_IOAPICS];

static inline uint32_t ioapic_read(int idx, uint32_t reg) {
    volatile uint8_t *base                       = ioapic_rt[idx].base;
    *(volatile uint32_t *)(base + IOAPIC_REGSEL) = reg;
    return *(volatile uint32_t *)(base + IOAPIC_WIN);
}

static inline void ioapic_write(int idx, uint32_t reg, uint32_t val) {
    volatile uint8_t *base                       = ioapic_rt[idx].base;
    *(volatile uint32_t *)(base + IOAPIC_REGSEL) = reg;
    *(volatile uint32_t *)(base + IOAPIC_WIN)    = val;
}

// read and ret val from reg
static inline uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t *)((volatile uint8_t *)lapic_base + reg);
}

// write value to reg
static inline void lapic_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)((volatile uint8_t *)lapic_base + reg) = val;
}

void lapic_eoi(void) {
    lapic_write(LAPIC_REG_EOI, 0);
}

uint32_t lapic_id(void) {
    return (lapic_read(LAPIC_REG_ID) >> 24);
}

void lapic_tick_ticks() {
    ticks++;
}

uint64_t lapic_get_ticks() {
    return ticks;
}

/**
 * lapic_timer_calibrate - Used to determine the LAPIC tick length.
 *
 * Description:
 * First, we read System Control Port B (0x61) of the PIT, mute the speaker
 * by clearing bit 1, open the second gate by setting bit 0, and write the changes back.
 *
 * Then, we initialize the PIT channel 2 to tick every 50ms and prepare the LAPIC timer
 * (divider set to 16, masked to prevent interruptions, and initial count set to the 32-bit max).
 *
 * Next, we enter a polling loop on port 0x61 (bit 5) to wait for the PIT timeout event.
 * A safety iteration limit of 1,000,000 cycles prevents infinite hanging on faulty hardware.
 *
 * Finally, we read the current LAPIC timer count before stopping the timer, calculate the elapsed
 * ticks from the max value, and derive the ticks per millisecond (`lapic_ticks_per_ms`).
 *
 * Return: Calculated ticks per millisecond, or 0 on timeout/failure.
 */
static int lapic_timer_calibrate(void) {
    DEBUG_KERNEL("[LAPIC][TIMER_CALIBRATE]: Calibrating the lapic timer\n");
    uint8_t sys_contr_p_b = inb(0x61);
    sys_contr_p_b &= ~2;
    sys_contr_p_b |= 1;
    outb(0x61, sys_contr_p_b);

    outb(0x43, 0xB0);
    outb(0x42, 59659 & 0xFF);
    outb(0x42, 59659 >> 8);

    lapic_write(LAPIC_REG_TIMER_DIV, LAPIC_TIMER_DIV_16);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_LVT_MASKED | LAPIC_TIMER_VECTOR);
    lapic_write(LAPIC_REG_TIMER_INIT, 0xFFFFFFFF);

    uint64_t break_loop = 0;
    while (!(inb(0x61) & (1 << 5))) {
        if (++break_loop > 2000000) {
            ERROR("[LAPIC][TIMER_CALIBRATE]: Polling timeout, aborting\n");
            return 0;
        }
    }

    uint32_t current = lapic_read(LAPIC_REG_TIMER_CUR);
    lapic_write(LAPIC_REG_TIMER_INIT, 0);
    uint32_t elapsed   = 0xFFFFFFFF - current;

    lapic_ticks_per_ms = elapsed / 50;
    DEBUG_KERNEL("[LAPIC][TIMER_CALIBRATE]: LAPIC calibrated. LAPIC ticks per ms: %d\n", elapsed, lapic_ticks_per_ms);

    return elapsed == 0 ? 0 : lapic_ticks_per_ms;
}

/**
 * lapic_timer_start_periodic - Starts the Local APIC timer in periodic mode.
 * @ms: The interval in milliseconds between timer ticks.
 *
 * Description:
 * Configures and starts the Local APIC timer to generate interrupts
 * periodically at the specified millisecond rate.
 *
 * Return: STATUS_OK on success, or STATUS_ERROR if the timer is uncalibrated.
 */
int lapic_timer_start_periodic(uint32_t ms) {
    DEBUG_KERNEL("[LAPIC][TIMER]: Starting periodic timer with %dms\n", ms);
    if (lapic_ticks_per_ms == 0) {
        return STATUS_ERROR;
    }

    lapic_write(LAPIC_REG_TIMER_DIV, LAPIC_TIMER_DIV_16);

    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_LVT_PERIODIC | LAPIC_TIMER_VECTOR);

    int initial_count = lapic_ticks_per_ms * ms;
    lapic_write(LAPIC_REG_TIMER_INIT, initial_count);
    DEBUG_KERNEL("[LAPIC][TIMER]: Timer started\n");
    return STATUS_OK;
}

/**
 * PIC_REMAP_AND_MASK - Used to silence older PIC.
 *
 * Description:
 * This function moves the older pics interrupts vectors to safer zones
 * and then disables the pic. First we send to pic1/2 cmd port info that tells to expect
 * initialization params. Then we send to pic1/2 the addr from where the interrupt vectors
 * start (0x20/0x28). After that we connect master and salve to each other. Then
 * we set them to the ancient 8086 arch. And finally we mask the ports. After that
 * we check if the all went as planned and then return status according to check
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
static int pic_remap_and_mask(void) {

    outb(PIC1_CMD, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, PIC1_VECTOR_BASE);
    io_wait();
    outb(PIC2_DATA, PIC2_VECTOR_BASE);
    io_wait();

    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    outb(PIC1_DATA, PIC_ICW4_8086);
    io_wait();
    outb(PIC2_DATA, PIC_ICW4_8086);
    io_wait();

    outb(PIC1_DATA, 0xFF);
    io_wait();
    outb(PIC2_DATA, 0xFF);
    io_wait();

    if (inb(PIC1_DATA) == 0xFF && inb(PIC2_DATA) == 0xFF) {
        return STATUS_OK;
    } else {
        return STATUS_ERROR;
    }
}

/**
 * ioapic_find - Find the owner of a given GSI.
 * @gsi: global system interrupt number to locate.
 *
 * Description:
 * This function searches through all registered I/O APIC entries to find the one
 * that owns the specified GSI route based on its base address and pin count.
 *
 * Return: index of the I/O APIC on success, or -1 if not found.
 */
static int ioapic_find(uint32_t gsi) {
    for (uint32_t i = 0; i < acpi_info.ioapic_count; i++) {
        uint32_t gsi_base  = acpi_info.ioapics[i].gsi_base;
        uint32_t pin_count = ioapic_rt[i].pins;

        if (gsi >= gsi_base && gsi < gsi_base + pin_count) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * ioapic_route_gsi - used to route interrupt to right core.
 * @gsi: global system interrupt number to route.
 * @vector: interrupt vector number.
 * @dest_apic_id: target apic id.
 * @madt_flags: polarity and trigger mode flags from MADT.
 *
 * Description:
 * First we determine the apic who owns the given gsi. Once the owner is found we can calculate its
 * pin by subtracting from the given gsi the target saved gsi_base. After that we can read the lower
 * register of the pin.
 *
 * Then we move on to create the lower part of the interrupt. By storing the vector into a 4-byte
 * variable we get more space to set the polarity and trigger mode should the madt flags
 * variable demand so. After that we create the high part for the interrupt by shifting dest_apic_id
 * left 24 bits. Finally, we write to the register region first the high part then the low part.
 *
 * Context: Called during device and interrupt initialization to map GSIs to specific CPU cores
 * with correct hardware polarity and trigger modes before enabling interrupts.
 * Return: STATUS_OK || STATUS_ERROR.
 */
int ioapic_route_gsi(uint32_t gsi, uint8_t vector, uint8_t dest_apic_id, uint16_t madt_flags) {
    int idx = ioapic_find(gsi);
    if (idx < 0) {
        ERROR("[IOAPIC][ROUTE_GSI]: GSI not registered to any device\n");
        return STATUS_ERROR;
    }

    uint32_t pin = gsi - acpi_info.ioapics[idx].gsi_base;
    uint32_t reg = IOAPIC_REG_REDIR(pin);

    uint32_t low = vector;
    if ((madt_flags & IOAPIC_MADT_POL_MASK) == IOAPIC_MADT_POL_LOW) {
        low |= IOAPIC_REDIR_POL_LOW;
    }

    if (((madt_flags >> IOAPIC_MADT_TRIG_SHIFT) & IOAPIC_MADT_TRIG_MASK) == IOAPIC_MADT_TRIG_LEVEL) {
        low |= IOAPIC_REDIR_LEVEL;
    }

    uint32_t high = (uint32_t)dest_apic_id << 24;

    ioapic_write(idx, reg + 1, high);
    ioapic_write(idx, reg, low);

    DEBUG_KERNEL("[IOAPIC][ROUTE_GSI]: GSI: %d, vector %d, flags %d\n", gsi, vector, madt_flags);
    return STATUS_OK;
}

/**
 * ioapic_route_isa - Used to route incoming interrupts to right target.
 * @irq: legacy IRQ number.
 * @vector: hold the interrupt vector number.
 * @dest_apic_id: target apics id.
 *
 * Description:
 * This function tries to find from the saved acpi_info.isos the source irq that matches the given
 * irq. If found we can take the gsi and flags of it and pass to gsi router function the information.
 * However should there be no saved iso entry from the interrupt we pass given params with flags 0 to
 * gsi router as a general interrupt.
 *
 * Return: result of ioapic_route_gsi function (status_ok or status_error).
 */
int ioapic_route_isa(uint8_t irq, uint8_t vector, uint8_t dest_apic_id) {
    for (uint32_t i = 0; i < acpi_info.iso_count; i++) {
        if (acpi_info.isos[i].source_irq == irq) {
            uint32_t gsi   = acpi_info.isos[i].gsi;
            uint16_t flags = acpi_info.isos[i].flags;

            return ioapic_route_gsi(gsi, vector, dest_apic_id, flags);
        }
    }
    return ioapic_route_gsi(irq, vector, dest_apic_id, 0);
}

/**
 * ioapic_mask_gsi - Used to silence/listen a single global system interrupt (gsi) pin.
 * @gsi: target interrupt.
 * @masked: effect.
 *
 * Description:
 * First this function tries to figure out what ioapic is the target by giving gsi param to
 * ioapic_find function. If it doesnt return and error it calculates the pin by subtracting the
 * target ioapic gsi base from the given gsi. Then with this pin it can calculate the region and
 * read the low part of it by using ioapic_read. Then if op is 1(true) we silence the target.
 * If the op is 0(false) we start listening the target.
 *
 * Context: So that we can silence or listen on the flight without having to reconfigure the whole
 * routing conf.
 * Return: STATUS_OK || STATUS_ERROR.
 */
int ioapic_mask_gsi(uint32_t gsi, int op) {
    int idx = ioapic_find(gsi);
    if (idx < 0) {
        ERROR("[IOAPIC][MASK_GSI]: GSI not registered to any device\n");
        return STATUS_ERROR;
    }

    uint32_t pin = gsi - acpi_info.ioapics[idx].gsi_base;
    uint32_t reg = IOAPIC_REG_REDIR(pin);
    uint32_t low = ioapic_read(idx, reg);

    if (op) {
        low |= IOAPIC_REDIR_MASKED;
    } else {
        low &= ~IOAPIC_REDIR_MASKED;
    }

    ioapic_write(idx, reg, low);
    return STATUS_OK;
}

/**
 * ioapic_init - Used to initialize ioapic controller.
 *
 * Description:
 * This function maps all the found apics to kernel page dir and silences all of them.
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
static int ioapic_init(void) {
    DEBUG_KERNEL("[IOAPIC][INIT]: Mapping & masking all the found ioapics\n");
    for (uint32_t i = 0; i < acpi_info.ioapic_count; i++) {
        uint64_t vaddr = acpi_info.ioapics[i].phys + KMMIO_REGION_BASE;
        if (paging_map(&kernel_page_dir, vaddr, acpi_info.ioapics[i].phys,
                       PAGE_RW | PAGE_PWT | PAGE_PCD | PAGE_NX) == STATUS_ERROR) {
            ERROR("[IOAPIC][INIT]: Failed to map ioapic entry %d to kernel directory\n", i);
            return STATUS_ERROR;
        }
        ioapic_rt[i].base = (uint8_t *)vaddr;

        uint32_t ver      = ioapic_read(i, IOAPIC_REG_VER);
        ioapic_rt[i].pins = ((ver >> 16) & 0xFF) + 1;

        for (uint32_t pin = 0; pin < ioapic_rt[i].pins; pin++) {
            uint32_t reg = IOAPIC_REG_REDIR(pin);
            ioapic_write(i, reg + 1, 0);
            ioapic_write(i, reg, IOAPIC_REDIR_MASKED | 0xFF);
        }
    }

    DEBUG_KERNEL("[IOAPIC][INIT]: Checking that masking was succesfull\n");
    for (uint32_t i = 0; i < acpi_info.ioapic_count; i++) {
        uint32_t pin1_val = ioapic_read(i, IOAPIC_REG_REDIR(1));
        DEBUG_KERNEL("[IOAPIC][INIT]: Pin 1 readback (proof of masking) for IOAPIC %d: 0x%x\n", i, pin1_val);

        if (!(pin1_val & IOAPIC_REDIR_MASKED)) {
            ERROR("[IOAPIC][INIT]: Did not succeed to silence the ioapic %d\n", i);
            return STATUS_ERROR;
        }
    }

    DEBUG_KERNEL("[IOAPIC][INIT]: IOAPIC controller initialized successfully\n");
    return STATUS_OK;
}

/**
 * lapic_init - used to initialize local advanced programmable interrupt controller.
 * @lapic: physical address of local apic.
 *
 * Description:
 * First we read the model specific register of IA32_APIC_BASE. Then we check that bit 11 (software
 * enable/disable) is on, if not then set it on and write it back. Then it compares the register to
 * the lapic_phys and warns if not.
 *
 * Next step is to map the lapic to kernel page dir. As it is not virtual address and our 2mb pages
 * do not allow us to map few singe 4KiB pages we don't add to it the hhdm value. We instead map it
 * to a special regions chosen by me (see config.h between kstack and heap).
 *
 * Finally we set the all outside interrupts on and set spurious vectors on
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
static int lapic_init(uint64_t lapic_phys) {
    uint64_t ia32_bmsr = rdmsr(IA32_APIC_BASE_MSR);

    if (!(ia32_bmsr & (1ULL << 11))) {
        DEBUG_KERNEL("[LAPIC][INIT]: LAPIC not enabled, Enabling it\n");
        ia32_bmsr |= (1ULL << 11);
        wrmsr(IA32_APIC_BASE_MSR, ia32_bmsr);
    }

    if ((ia32_bmsr & ~0xFFFULL) != lapic_phys) {
        DEBUG_KERNEL("[LAPIC][INIT]: MSR differs compared to physical lapic address\n");
    }

    uint64_t lapic_vaddr = lapic_phys + KMMIO_REGION_BASE;

    if (paging_map(&kernel_page_dir, lapic_vaddr, lapic_phys, PAGE_RW | PAGE_PWT | PAGE_PCD | PAGE_NX) == STATUS_ERROR) {
        ERROR("[LAPIC][INIT]: Failed to map lapic to kernel directory\n");
        return STATUS_ERROR;
    }

    lapic_base = (uint32_t *)lapic_vaddr;
    lapic_write(LAPIC_REG_TPR, 0);
    lapic_write(LAPIC_REG_SPURIOUS, LAPIC_SPURIOUS_ENABLE | LAPIC_SPURIOUS_VECTOR);

    uint32_t ver_reg = lapic_read(LAPIC_REG_VERSION);
    DEBUG_KERNEL("[LAPIC][INIT]: Lapic id: %d, version: 0x%x (ver: %d, max lvt: %d). LAPIC ENABLED\n",
                 lapic_id(), ver_reg, ver_reg & 0xFF, (ver_reg >> 16) & 0xFF);
    return STATUS_OK;
}

/**
 * apic_init - use to initialize lapic and ioapic.
 *
 * Description:
 * This function calls the chain of functions used to get ticks ticking.
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
int apic_init() {
    if (pic_remap_and_mask() == STATUS_ERROR) {
        ERROR("[APIC][INIT]: Failed to remap and mask PIC\n");
        return STATUS_ERROR;
    }

    if (lapic_init(acpi_info.lapic_phys) == STATUS_ERROR) {
        ERROR("[APIC][INIT]: Failed to initialize lapic\n");
        return STATUS_ERROR;
    }

    if (lapic_timer_calibrate() == 0) {
        ERROR("[APIC][INIT]: Failed to calibrate timer\n");
        return STATUS_ERROR;
    }

    if (ioapic_init() == STATUS_ERROR) {
        ERROR("[APIC][INIT]: Failed to initialize ioapic\n");
        return STATUS_ERROR;
    }

    return STATUS_OK;
}
