#include "font.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "ui.h"
#include <stdint.h>

#define BUF_SIZE 128
#define WINDOW_WIDTH 450
#define WINDOW_HEIGHT 450

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

    int main_req_id = create_container(450, 430, 0, 21, COLOR_WHITE, COLOR_DARK_GRAY);

    if (main_req_id == -1) {
        return 0;
    }

    int b_open = create_button(100, 30, 175, 200, "Open file");
    int b_exit = create_button(100, 30, 175, 240, "Exit");

    char buf[BUF_SIZE];
    int pos = 0;
    char c;

    while (1) {

        show(header_reg_id);
        show(main_req_id);
        show(b_open);
        show(b_exit);

        while (1) {
            scan(&c);
        }
    }

    return 0;
}
