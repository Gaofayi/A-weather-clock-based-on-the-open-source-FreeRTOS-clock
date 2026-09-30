#include "ui.h"
#include "image.h"
#include <stdio.h>


void image_page_display(void)
{
    
    ui_fill_color(0, 0, UI_WIDTH - 1, UI_HEIGHT - 1, 0x0000);
    
    
    ui_draw_image(0, 0, &img_part1);    
    ui_draw_image(0, 80, &img_part2);   
    ui_draw_image(0, 160, &img_part3);  
}
