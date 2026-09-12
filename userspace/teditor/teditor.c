#include "font.h"
#include "op_sy.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "ui.h"
#include <stdint.h>

#define BUF_SIZE 128
#define WINDOW_WIDTH 450
#define WINDOW_HEIGHT 450
#define BUFFER_SIZE 512

static int curret_selected_id = -1;
static int main_req_id        = -1;

void on_press_dirent() {
}

void on_open_click() {

    dirent_info_t dirents[20];

    int dirents_added = list_dirents(dirents, 20);

    if (dirents_added <= 0) {
        return;
    }
    refresh_region(main_req_id);

    for (int i = 0; i < dirents_added; i++) {
        char number[10];
        itoa(i, number);
        print_to_region(main_req_id, number);
        print_to_region(main_req_id, " ");
        print_to_region(main_req_id, dirents[i].name);
        print_to_region(main_req_id, "\n");
    }

    char c;
    print_to_region(main_req_id, "Select -> \n");
    while (1) {
        scan(&c);

        switch (c) {
        case KEY_UP:
            break;
        case KEY_DOWN:
            break;
        case '\n':
            button_press(curret_selected_id);
            break;
        }
    }
}

void on_exit_click() {
    terminate_program();
}

int main(void) {

    if (resize_viewport(WINDOW_WIDTH, WINDOW_HEIGHT) == STATUS_ERROR) {
        print("Could not resize the viewport\n");
    }

    const char *title = "Teditor -> the text editor";
    int header_reg_id = create_label(450, 20, 0, 1, ((WINDOW_WIDTH / 2) - (strlen(title) * FONT_WIDTH) / 2),
                                     0, COLOR_WHITE, COLOR_DARK_RED, title);

    if (header_reg_id == -1) {
        return 0;
    }

    main_req_id = create_container(450, 430, 0, 21, COLOR_WHITE, COLOR_DARK_GRAY);

    if (main_req_id == -1) {
        return 0;
    }

    int b_open = create_button(100, 30, 175, 200, "Open file", on_open_click);
    int b_exit = create_button(100, 30, 175, 240, "Exit", on_exit_click);

    char c;

    show(header_reg_id);
    show(main_req_id);
    show(b_open);
    show(b_exit);

    while (1) {
        reset_region(main_req_id);
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

    return 0;
}
