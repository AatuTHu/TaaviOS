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

    int header_reg_id = create_region(450, 20, 0, 0, COLOR_WHITE, COLOR_DARK_RED);

    if (header_reg_id == -1) {
        return 0;
    }

    const char *title = "Teditor -> the text editor";
    print_at(header_reg_id, ((WINDOW_WIDTH / 2) - (strlen(title) * FONT_WIDTH) / 2), 1, title);

    int main_req_id = create_region(450, 430, 0, 21, COLOR_WHITE, COLOR_DARK_GRAY);

    if (main_req_id == -1) {
        return 0;
    }

    char buf[BUF_SIZE];
    int pos = 0;
    char c;

    while (1) {

        create_button(100, 30, 175, 200, "Open file");
        create_button(100, 30, 175, 240, "Exit");

        while (1) {
            scan(&c);
        }
    }

    return 0;
}
