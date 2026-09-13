#include "interfaces.h"
#include "font.h"
#include "log.h"
#include "malloc.h"
#include "op_sy.h"
#include "readline.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "ui.h"
#include <stdbool.h>
#include <stdint.h>

#define FILE_BUFFER_SIZE 1024

static int curret_selected_id = -1;
static int main_req_id        = -1;
static int b_open             = -1;
static int b_exit             = -1;
static bool has_initialized   = false;

void display_file(int fd) {
    char *file = (char *)malloc(FILE_BUFFER_SIZE);

    if (file == NULL) {
        return;
    }

    int bytes_read = read(fd, file, FILE_BUFFER_SIZE);

    if (bytes_read == STATUS_ERROR) {
        return;
    }

    reset_region(main_req_id);
    print_to_region(main_req_id, file);

    char c;
    while (1) {
        scan(&c);
    }
}

static void init_widgets() {
    main_req_id = create_container(450, 430, 0, 21, COLOR_WHITE, COLOR_DARK_GRAY);

    b_open      = create_button(100, 30, 175, 200, "Open file", display_open_project);
    b_exit      = create_button(100, 30, 175, 240, "Exit", terminate_program);

    if (b_open == STATUS_ERROR || b_exit == STATUS_ERROR || main_req_id == STATUS_ERROR) {
        terminate_program();
    }
}

static int handle_answer(const char *answer) {

    int answer_len = strlen(answer);

    for (int i = 0; i < answer_len; i++) {
        if (answer[i] == 'b') {
            return STATUS_ERROR;
        }
    }

    int converted_answer = atoi(answer);

    return converted_answer;
}

void display_open_project() {

    dirent_info_t dirents[20];

    int dirents_added = list_dirents(dirents, 20);

    if (dirents_added <= 0) {
        return;
    }
    reset_region(main_req_id);
    int advance_x = 175;
    int advance_y = 200;
    for (int i = 0; i < dirents_added; i++) {
        char number[10];
        itoa(i, number);
        print_at(main_req_id, advance_x, advance_y, number);
        advance_x += strlen(number) * FONT_WIDTH;
        print_at(main_req_id, advance_x, advance_y, " ");
        advance_x += FONT_WIDTH;
        print_at(main_req_id, advance_x, advance_y, dirents[i].name);
        advance_x = 175;
        advance_y += 18;
    }

    print_at(main_req_id, advance_x, advance_y, "B to go back\n");
    advance_y += 18;

    char buf[3];
    const char *readline_start_msg = "Select -> ";
    advance_x -= strlen(readline_start_msg) * FONT_WIDTH;
    print_at(main_req_id, advance_x, advance_y, readline_start_msg);
    advance_x += strlen(readline_start_msg) * FONT_WIDTH;
    readline_at(main_req_id, buf, 3, advance_x, advance_y);

    int selected_option = handle_answer(buf);

    if (dirents[selected_option].type == DIRECTORY) {
        change_directory(dirents[selected_option].name, NULL);
        display_open_project(); // <-- RECURSION;
        return;
    }

    int fd = open(dirents[selected_option].name, O_RDWR);

    if (fd == STATUS_ERROR) {
        LOG("Invalid file\n");
        return;
    }
    display_file(fd);
}

void display_main_view() {

    if (!has_initialized) {
        init_widgets();
    }

    char c;

    while (1) {
        reset_region(main_req_id);
        show(main_req_id);
        show(b_open);
        show(b_exit);
        scan(&c);

        switch (c) {
        case KEY_UP:
            set_region_border_color(b_exit, COLOR_DARK_GRAY);
            set_region_border_color(b_open, COLOR_WHITE);
            curret_selected_id = b_open;
            break;
        case KEY_DOWN:
            set_region_border_color(b_open, COLOR_DARK_GRAY);
            set_region_border_color(b_exit, COLOR_WHITE);
            curret_selected_id = b_exit;
            break;
        case '\n':
            button_press(curret_selected_id);
            break;
        }
    }
}
