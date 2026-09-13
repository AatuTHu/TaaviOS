#include "font.h"
#include "interfaces/interfaces.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "ui.h"
#include <stdint.h>

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

    show(header_reg_id);
    display_main_view();

    return 0;
}
