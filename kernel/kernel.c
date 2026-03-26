#include "vga.h"
#include "serial.h"
#include "klog.h"

void init_drivers() {
    vga_init();
    serial_init();
}

void kernel_main(uint32_t *mboot_info) {
    init_drivers();
    klog(0,"for vga and serial\n");
    klog(1,"Only for serial");
    klog(2,"Only for vga\n");
}