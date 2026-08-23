#include "keyboard.h"
#include "config.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "sched.h"
#include "shared.h"
#include <stdbool.h>
/**
 * Keyboard driver & routing
 * Design & Implementation
 * @author: A.H, 2026
 */

static keyboard_buffer_t kbd_buf_instance;
static keyboard_buffer_t *keyboard_buffer = &kbd_buf_instance;
static int shift_pressed                  = 0;
static int caps_lock_pressed              = 0;
static int ctrl_pressed                   = 0;
static int waiting_queue_count            = 0;
static int operator_pid                   = -1;
static int waiting_queue[MAX_TASKS];
static int extended_prefix = 0;

int keyboard_get_foreground_pid() {
    return keyboard_buffer->foreground_pid;
}

static void keyboard_clear_buffer() {
    keyboard_buffer->read = keyboard_buffer->write;
}

int keyboard_replace_cur_foreground_pid(uint32_t new_pid) {
    DEBUG("[KEYBOARD][RCFP]: Replacing current foreground pid with %d\n", new_pid);
    if (new_pid >= MAX_TASKS) {
        return STATUS_ERROR;
    }

    keyboard_clear_buffer();
    keyboard_buffer->foreground_pid = new_pid;
    return STATUS_OK;
}

void keyboard_set_foreground_pid(int pid) {
    if (pid == -1) {
        if (waiting_queue_count > 0 && operator_pid == -1) {
            pid = waiting_queue[0];
            for (int i = 0; i < waiting_queue_count - 1; i++) {
                waiting_queue[i] = waiting_queue[i + 1];
            }
            waiting_queue_count--;
        } else if (operator_pid != -1) {
            pid = operator_pid;
        }
    }

    keyboard_replace_cur_foreground_pid(pid);
}

static void keyboard_add_to_waiting_queue(int pid) {
    if (pid == -1)
        return;
    for (int i = 0; i < waiting_queue_count; i++) {
        if (waiting_queue[i] == pid)
            return; // already queued
    }
    waiting_queue[waiting_queue_count++] = pid;
}

static void keyboard_irq_handler(void) {
    uint8_t scancode = inb(0x60);
    keyboard_handler(scancode);
}

static void keyboard_write_to_buffer(char c) {
    if ((keyboard_buffer->write - keyboard_buffer->read) ==
        KEYBOARD_BUFFER_SIZE) {
        return;
    }
    keyboard_buffer->buf[keyboard_buffer->write % KEYBOARD_BUFFER_SIZE] = c;
    keyboard_buffer->write++;
}

int keyboard_read_from_buffer(char *out, uint32_t pid) {

    if (keyboard_get_foreground_pid() == -1) {
        keyboard_set_foreground_pid(pid);
    }

    if (keyboard_get_foreground_pid() != (int)pid) {
        keyboard_add_to_waiting_queue(pid);
        return 0;
    }

    if (keyboard_buffer->read == keyboard_buffer->write) {
        return 0;
    }

    *out = keyboard_buffer->buf[keyboard_buffer->read % KEYBOARD_BUFFER_SIZE];
    keyboard_buffer->read++;

    return 1;
}

int keyboard_set_operator_pid(uint32_t pid) {
    if (pid >= MAX_TASKS) {
        return STATUS_ERROR;
    }

    if (operator_pid == -1) {
        DEBUG("[KEYBOARD][RCFP]: Set %d as operator pid\n", pid);
        operator_pid = pid;
        return STATUS_OK;
    }

    return STATUS_ERROR;
}

void keyboard_handler(uint8_t scancode) {

    if ((scancode == 0x2A) || (scancode == 0x36)) {
        shift_pressed = 1;
        return;
    }
    if ((scancode == 0xAA) || (scancode == 0xB6)) {
        shift_pressed = 0;
        return;
    }

    if (scancode == 0xE0) {
        extended_prefix = 1;
        return;
    }

    if (scancode == 0x1D) {
        ctrl_pressed = 1;
        return;
    }

    if (scancode == 0x9D) {
        ctrl_pressed = 0;
        return;
    }

    if (scancode == 0x3A) {
        caps_lock_pressed = 1 - caps_lock_pressed;
        return;
    }

    if (extended_prefix) {
        extended_prefix          = 0;
        bool did_press_arrow_key = false;
        switch (scancode) {
        case 0x48:
            keyboard_write_to_buffer(KEY_UP);
            did_press_arrow_key = true;
            break;
        case 0x50:
            keyboard_write_to_buffer(KEY_DOWN);
            did_press_arrow_key = true;
            break;
        case 0x4B:
            keyboard_write_to_buffer(KEY_LEFT);
            did_press_arrow_key = true;
            break;
        case 0x4D:
            keyboard_write_to_buffer(KEY_RIGHT);
            did_press_arrow_key = true;
            break;
        default:
            break;
        }
        if (did_press_arrow_key) {
            scheduler_wake_task(keyboard_buffer->foreground_pid);
        }
        return;
    }

    if (ctrl_pressed && scancode == 0x18) {
        if (operator_pid != -1)
            keyboard_replace_cur_foreground_pid(operator_pid);
        return;
    }

    if ((scancode < sizeof(scancode_to_ascii) && !(scancode & 0x80))) {
        char c = shift_pressed ? scancode_to_ascii_shifted[scancode]
                               : scancode_to_ascii[scancode];

        if (caps_lock_pressed) {
            c = scancode_to_ascii_caps_lock[scancode];
        }

        if (caps_lock_pressed && shift_pressed) {
            c = scancode_to_ascii[scancode];
        }

        if (c != 0) {
            // DEBUG("[KEYBOARD][HANDLER]: fired!\n");
            keyboard_write_to_buffer(c);
            scheduler_wake_task(keyboard_buffer->foreground_pid);
        }
    }
}

void keyboard_init() {
    keyboard_buffer->read           = 0;
    keyboard_buffer->write          = 0;
    keyboard_buffer->foreground_pid = -1;
    irq_register_handler(1, keyboard_irq_handler);
}
