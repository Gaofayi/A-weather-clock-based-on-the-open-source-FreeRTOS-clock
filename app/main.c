#include "FreeRTOS.h"
#include "task.h"
#include "workqueue.h"
#include "app.h"
#include "ui.h"
#include "wifi.h"
#include "page.h"


extern void board_lowlevel_init(void);
extern void board_init(void);
extern void aht20_self_test(void);

static void main_init(void *param)
{
    board_init();    //初始化RTC、AHT20等
    ui_init();   //创建UI队列并往UI队列里写数据，根据不同数据决定画字符串还是图像还是填充颜色（优先级8）
    
	aht20_self_test();
	
    welcome_page_display();   //界面UI的设计都是可以用Figma软件定位每个像素的具体位置的
    
    wifi_init();
    wifi_page_display();
    wifi_wait_connect();
    
    main_page_display();
    app_init();
    
    vTaskDelete(NULL);
}

int main(void)
{
    board_lowlevel_init();    //时钟、外设等初始化,以及一些钩子函数
    workqueue_init();    //创建工作队列（打工人任务）
    
    xTaskCreate(main_init, "init", 1024, NULL, 9, NULL);  //第一次执行所有初始化后自杀
    
    vTaskStartScheduler();  //启动调度器，让FreeRTOS接管
    
    while (1)
    {
        ; // code should not run here
    }
}
