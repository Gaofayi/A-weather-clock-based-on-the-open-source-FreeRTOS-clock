#ifndef __APP_H__
#define __APP_H__

#define APP_VERSION "v1.0"

void app_init(void);
#include "weather.h"   

// 定义全局缓存变量（告诉编译器“这些变量在别的文件里”）
extern float g_last_temperature;
extern float g_last_humidity;
extern weather_info_t g_last_weather;   

// 页面状态枚举
typedef enum {
    PAGE_MAIN,
    PAGE_SYSINFO,
    PAGE_IMAGE
} Page_t;

extern Page_t g_current_page;   


void app_init(void);
void sysinfo_page_display(void);
void image_page_display(void);

#endif /* __APP_H__ */
