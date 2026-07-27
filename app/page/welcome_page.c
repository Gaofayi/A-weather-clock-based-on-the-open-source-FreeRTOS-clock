#include "ui.h"

void welcome_page_display(void)
{
    const uint16_t color_bg = mkcolor(0, 0, 0);
    ui_fill_color(0, 0, UI_WIDTH - 1, UI_HEIGHT - 1, color_bg);
    ui_draw_image(30, 10, &img_meihua);
    ui_write_string(40, 205, "\xB4\xA8\xC9\xCF\xB8\xB0\xD2\xB0", mkcolor(237, 128, 147), color_bg, &font32_maple_bold);
    ui_write_string(56, 233, "\xCC\xEC\xC6\xF8\xCA\xB1\xD6\xD3", mkcolor(86, 165, 255), color_bg, &font32_maple_bold);
    ui_write_string(60, 285, "loading...", mkcolor(255, 255, 255), color_bg, &font24_maple_bold);
}
