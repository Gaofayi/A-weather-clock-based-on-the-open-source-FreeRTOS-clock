#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "rtc.h"
#include "wifi.h"
#include "ui.h"
#include "app.h"
#include "page.h"
#include "esp_at.h"
#include "task.h"


// ===== 自定义颜色 =====
#define COLOR_BG_PANEL      0x18C3   
#define COLOR_BG_TITLE      0x001F  
#define COLOR_TEXT_WHITE    0xFFFF
#define COLOR_TEXT_GRAY     0x8410   
#define COLOR_GREEN         0x07E0
#define COLOR_RED           0xF800
#define COLOR_YELLOW        0xFFE0

Page_t g_current_page = PAGE_MAIN;


void sysinfo_page_display(void)
{
    int y = 0;
    char line[32];
    
   
    ui_fill_color(0, 0, UI_WIDTH - 1, UI_HEIGHT - 1, 0x0000);
    
    
    ui_fill_color(0, 0, 239, 32, COLOR_BG_TITLE);
    ui_write_string(76, 6, "System Info", COLOR_TEXT_WHITE, COLOR_BG_TITLE, &font20_maple_bold);
    
    y = 40;
    
    
    ui_fill_color(8, y, 224, 44, COLOR_BG_PANEL);
    rtc_date_time_t now;
    rtc_get_time(&now);
    
    snprintf(line, sizeof(line), "%04u-%02u-%02u  %02u:%02u:%02u",
             now.year, now.month, now.day, now.hour, now.minute, now.second);
    ui_write_string(16, y + 12, line, COLOR_TEXT_WHITE, COLOR_BG_PANEL, &font20_maple_bold);
    y += 52;
    
    
    ui_fill_color(8, y, 224, 44, COLOR_BG_PANEL);
    esp_wifi_info_t wifi;
    uint16_t wifi_color = COLOR_TEXT_WHITE;
    if (esp_at_get_wifi_info(&wifi) && wifi.connected) {
       
        char ssid_short[9];
        strncpy(ssid_short, wifi.ssid, 8);
        ssid_short[8] = '\0';
        snprintf(line, sizeof(line), "WiFi: %s  %ddBm", ssid_short, wifi.rssi);
        wifi_color = COLOR_GREEN;
    } else {
        snprintf(line, sizeof(line), "WiFi: Disconnected");
        wifi_color = COLOR_RED;
    }
    ui_write_string(16, y + 12, line, wifi_color, COLOR_BG_PANEL, &font20_maple_bold);
    y += 52;
    
    
    ui_fill_color(8, y, 224, 44, COLOR_BG_PANEL);
    uint32_t uptime_sec = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
    uint32_t hours = uptime_sec / 3600;
    uint32_t minutes = (uptime_sec % 3600) / 60;
    uint32_t seconds = uptime_sec % 60;
    snprintf(line, sizeof(line), "Uptime: %02u:%02u:%02u", hours, minutes, seconds);
    ui_write_string(16, y + 12, line, COLOR_TEXT_WHITE, COLOR_BG_PANEL, &font20_maple_bold);
    y += 52;
    
    
    ui_fill_color(8, y, 224, 52, COLOR_BG_PANEL);
    if (g_last_temperature > -50.0f && g_last_temperature < 100.0f) {
        snprintf(line, sizeof(line), "In: %.1fC  %.1f%%", 
                 g_last_temperature, g_last_humidity);
    } else {
        snprintf(line, sizeof(line), "In: --C  --%%");
    }
    ui_write_string(16, y + 6, line, COLOR_TEXT_WHITE, COLOR_BG_PANEL, &font20_maple_bold);
    
    if (g_last_weather.temperature > -50.0f && g_last_weather.temperature < 80.0f) {
        snprintf(line, sizeof(line), "Out: %.1fC  %s", 
                 g_last_weather.temperature, g_last_weather.weather);
    } else {
        snprintf(line, sizeof(line), "Out: --C  waiting...");
    }
    ui_write_string(16, y + 30, line, COLOR_TEXT_WHITE, COLOR_BG_PANEL, &font20_maple_bold);
    y += 60;
    
    
    ui_fill_color(8, y, 224, 48, COLOR_BG_PANEL);
    const char *page_name = "MAIN";
    if (g_current_page == PAGE_SYSINFO) page_name = "SYSINFO";
    else if (g_current_page == PAGE_IMAGE) page_name = "IMAGE";
    snprintf(line, sizeof(line), "Page: %s", page_name);
    ui_write_string(16, y + 6, line, COLOR_YELLOW, COLOR_BG_PANEL, &font20_maple_bold);
    
    uint32_t stack_remain = (uint32_t)uxTaskGetStackHighWaterMark(NULL) * 4;
    snprintf(line, sizeof(line), "Stack: %u B", stack_remain);
    ui_write_string(16, y + 30, line, COLOR_TEXT_GRAY, COLOR_BG_PANEL, &font20_maple_bold);
    y += 56;
    
    
    ui_write_string(8, 304, "Weather Clock v1.0", COLOR_TEXT_GRAY, 0x0000, &font16_maple);
}
