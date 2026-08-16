#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "workqueue.h"
#include "rtc.h"
#include "aht20.h"
#include "esp_at.h"
#include "weather.h"
#include "page.h"
#include "app.h"
#include "user_config.h"

#define MILLISECONDS(x) (x)
#define SECONDS(x)      MILLISECONDS((x) * 1000)
#define MINUTES(x)      SECONDS((x) * 60)
#define HOURS(x)        MINUTES((x) * 60)
#define DAYS(x)          HOURS((x) * 24)

#define TIME_SYNC_INTERVAL          HOURS(1)
#define WIFI_UPDATE_INTERVAL        SECONDS(5)
#define TIME_UPDATE_INTERVAL        SECONDS(1)
#define INNER_UPDATE_INTERVAL       SECONDS(3)
#define OUTDOOR_UPDATE_INTERVAL     MINUTES(1)

#define MLOOP_EVT_TIME_SYNC         (1 << 0)
#define MLOOP_EVT_WIFI_UPDATE       (1 << 1)
#define MLOOP_EVT_INNER_UPDATE      (1 << 2)
#define MLOOP_EVT_OUTDOOR_UPDATE    (1 << 3)
#define MLOOP_EVT_ALL               (MLOOP_EVT_TIME_SYNC | \
                                     MLOOP_EVT_WIFI_UPDATE | \
                                     MLOOP_EVT_INNER_UPDATE | \
                                     MLOOP_EVT_OUTDOOR_UPDATE)



extern void key_gpio_init(void);
extern uint8_t key_is_pressed(void);
extern void sysinfo_page_display(void);
extern void image_page_display(void);
// 定义变量（这里分配了实际的内存空间）
float g_last_temperature = 0.0f;
float g_last_humidity = 0.0f;
weather_info_t g_last_weather = { 0 };   // 初始化为全 0


// 按键扫描任务
static void key_scan_task(void *pvParameters)
{
    uint8_t last_key_state = 1;      // 上一次按键状态（1=松开，0=按下）
    uint8_t current_key_state;
    
    (void)pvParameters;  // 消除未使用参数警告
    
    while (1)
    {
        // 每 50ms 扫描一次
        vTaskDelay(pdMS_TO_TICKS(50));
        
        // 1. 读取当前按键状态（0=按下，1=松开）
        current_key_state = key_is_pressed() ? 0 : 1;
        
        // 2. 检测下降沿（按下事件）：上次=1，这次=0
        if (last_key_state == 1 && current_key_state == 0)
        {
            // 3. 消抖：再读一次确认
            vTaskDelay(pdMS_TO_TICKS(20));
            if (key_is_pressed())  // 确认确实按下了
            {
                // 4. 执行页面切换
                switch (g_current_page)
                {
                    case PAGE_MAIN:
                        g_current_page = PAGE_SYSINFO;
                        sysinfo_page_display();
                        break;
                        
                    case PAGE_SYSINFO:
                        g_current_page = PAGE_IMAGE;
                        image_page_display();
                        break;
                        
                    case PAGE_IMAGE:
                        g_current_page = PAGE_MAIN;
                        main_page_display();
                        break;
                        
                    default:
                        g_current_page = PAGE_MAIN;
                        main_page_display();
                        break;
                }
            }
        }
        
        // 5. 更新上一次状态
        last_key_state = current_key_state;
    }
}   //g

static TimerHandle_t time_sync_timer;
static TimerHandle_t wifi_update_timer;
static TimerHandle_t time_update_timer;
static TimerHandle_t inner_update_timer;
static TimerHandle_t outdoor_update_timer;

static void time_sync(void)
{
	
    uint32_t restart_sync_delay = TIME_SYNC_INTERVAL;
    rtc_date_time_t rtc_date = { 0 };

    esp_date_time_t esp_date = { 0 };
    if (!esp_at_sntp_get_time(&esp_date))
    {
        printf("[SNTP] get time failed\n");
        restart_sync_delay = SECONDS(1);
        goto err;
    }
    
    if (esp_date.year < 2000)
    {
        printf("[SNTP] invalid date formate\n");
        restart_sync_delay = SECONDS(1);
        goto err;
    }
    
    printf("[SNTP] sync time: %04u-%02u-%02u %02u:%02u:%02u (%d)\n",
        esp_date.year, esp_date.month, esp_date.day,
        esp_date.hour, esp_date.minute, esp_date.second, esp_date.weekday);
    
    rtc_date.year = esp_date.year;
    rtc_date.month = esp_date.month;
    rtc_date.day = esp_date.day;
    rtc_date.hour = esp_date.hour;
    rtc_date.minute = esp_date.minute;
    rtc_date.second = esp_date.second;
    rtc_date.weekday = esp_date.weekday;
    rtc_set_time(&rtc_date);
    
err:
    xTimerChangePeriod(time_sync_timer, pdMS_TO_TICKS(restart_sync_delay), 0);
}

static void wifi_update(void)
{
	 if (g_current_page != PAGE_MAIN) {
        return; // 不是主界面就不刷新时间
    }
    static esp_wifi_info_t last_info = { 0 };

    esp_wifi_info_t info = { 0 };
    if (!esp_at_get_wifi_info(&info))
    {
        printf("[AT] wifi info get failed\n");
        return;
    }
    
    if (memcmp(&info, &last_info, sizeof(esp_wifi_info_t)) == 0)
    {
        return;
    }
    
    if (last_info.connected == info.connected)
    {
        return;
    }
    
    if (info.connected)
    {
        printf("[WIFI] connected to %s\n", info.ssid);
        printf("[WIFI] SSID: %s, BSSID: %s, Channel: %d, RSSI: %d\n",
                info.ssid, info.bssid, info.channel, info.rssi);
        main_page_redraw_wifi_ssid(info.ssid);
    }
    else
    {
        printf("[WIFI] disconnected from %s\n", last_info.ssid);
        main_page_redraw_wifi_ssid("wifi lost");
    }
    
    memcpy(&last_info, &info, sizeof(esp_wifi_info_t));
}

static void time_update(void)
{
	 if (g_current_page != PAGE_MAIN) {
        return; // 不是主界面就不刷新时间
    }
    static rtc_date_time_t last_date = { 0 };
    
    rtc_date_time_t date;
    rtc_get_time(&date);
    
    if (date.year < 2020)
    {
        return;
    }
    
    if (memcmp(&date, &last_date, sizeof(rtc_date_time_t)) == 0)
    {
        return;
    }
    
    memcpy(&last_date, &date, sizeof(rtc_date_time_t));
    main_page_redraw_time(&date);
    main_page_redraw_date(&date);
}

static void inner_update(void)
{
	
	 if (g_current_page != PAGE_MAIN) {
        return; // 不是主界面就不刷新时间
    }
    static float last_temperature, last_humidity;
    
    /*if (!aht20_start_measurement())
    {
        printf("[AHT20] start measurement failed\n");
        return;
    } */
	
    // 带重试的测量触发（最多尝试 200ms）
bool measured = false;
for (int retry = 0; retry < 20; retry++) {
    if (aht20_start_measurement()) {
        measured = true;
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}
if (!measured) {
    printf("[AHT20] start measurement failed after retries\n");
    return;
}



    if (!aht20_wait_for_measurement())
    {
        printf("[AHT20] wait for measurement failed\n");
        return;
    }
    
    float temperature = 0.0f, humidity = 0.0f;
    
    if (!aht20_read_measurement(&temperature, &humidity))
    {
        printf("[AHT20] read measurement failed\n");
        return;
    }
    
	// ========== ?? 就在这里插入（加在这里最好） ?? ==========
    // 应用软件偏移补偿（解决 PCB 热干扰导致的偏高问题）
    temperature += AHT20_TEMP_OFFSET;   // 假如宏是 -4.5f，这里就是减 4.5 度
    
    // 保险限幅（防止补偿后溢出显示异常）
    if (temperature < -40.0f) temperature = -40.0f;
    if (temperature > 85.0f) temperature = 85.0f;
    // ========== ?? 插入结束 ?? ==========
	
    if (temperature == last_temperature && humidity == last_humidity)
    {
        return;
    }
    
    last_temperature = temperature;
    last_humidity = humidity;
    
    printf("[AHT20] Temperature: %.1f, Humidity: %.1f\n", temperature, humidity);
	  g_last_temperature = temperature;
    g_last_humidity = humidity;
    main_page_redraw_inner_temperature(temperature);
    main_page_redraw_inner_humidity(humidity);
}

static void outdoor_update(void)
{
	
	 if (g_current_page != PAGE_MAIN) {
        return; // 不是主界面就不刷新时间
    }
    static weather_info_t last_weather = { 0 };
    
    weather_info_t weather = { 0 };
    const char *weather_url = API_KEY;
    const char *weather_http_response = esp_at_http_get(weather_url);
    if (weather_http_response == NULL)
    {
        printf("[WEATHER] http error\n");
        return;
    }
    
    if (!parse_seniverse_response(weather_http_response, &weather))
    {
        printf("[WEATHER] parse failed\n");
        return;
    }
    
    if (memcmp(&last_weather, &weather, sizeof(weather_info_t)) == 0)
    {
        return;
    }
    
    memcpy(&last_weather, &weather, sizeof(weather_info_t));
    printf("[WEATHER] %s, %s, %.1f\n", weather.city, weather.weather, weather.temperature);
      memcpy(&g_last_weather, &weather, sizeof(weather_info_t));
    main_page_redraw_outdoor_temperature(weather.temperature);
    main_page_redraw_outdoor_weather_icon(weather.weather_code);
}

typedef void (*app_job_t)(void);   //定义app_job_t类型的函数

static void app_work(void *param)
{
    app_job_t job = (app_job_t)param;
    job(); //执行该函数
}

static void work_timer_cb(TimerHandle_t timer)  //FreeRTOS要求回调是形式上void（TimerHandle_t）
{
    app_job_t job = (app_job_t)pvTimerGetTimerID(timer);
    workqueue_run(app_work, job);
}       //定时器到时间后执行此回调，此回调只是把要执行的耗时函数丢到工作队列里去，低优先级任务后台执行

static void app_timer_cb(TimerHandle_t timer)
{
    app_job_t job = (app_job_t)pvTimerGetTimerID(timer);
    job();
}

void app_init(void)
{
    time_update_timer = xTimerCreate("time update", pdMS_TO_TICKS(TIME_UPDATE_INTERVAL), pdTRUE, time_update, app_timer_cb);
	//SNTP网络对时
    time_sync_timer = xTimerCreate("time sync", pdMS_TO_TICKS(200), pdFALSE, time_sync, work_timer_cb);
	//读取RTC，刷新时间、显示日期
    wifi_update_timer = xTimerCreate("wifi update", pdMS_TO_TICKS(WIFI_UPDATE_INTERVAL), pdTRUE, wifi_update, work_timer_cb);
	//检查WIFI连接状态
    inner_update_timer = xTimerCreate("inner upadte", pdMS_TO_TICKS(INNER_UPDATE_INTERVAL), pdTRUE, inner_update, work_timer_cb);
	//更新室内环境
    outdoor_update_timer = xTimerCreate("outdoor update", pdMS_TO_TICKS(OUTDOOR_UPDATE_INTERVAL), pdTRUE, outdoor_update, work_timer_cb);
	//更新室外环境
    workqueue_run(app_work, time_sync);
    workqueue_run(app_work, wifi_update);
    workqueue_run(app_work, inner_update);
    workqueue_run(app_work, outdoor_update);
    
    xTimerStart(time_update_timer, 0);
    xTimerStart(time_sync_timer, 0);
    xTimerStart(wifi_update_timer, 0);
    xTimerStart(inner_update_timer, 0);
    xTimerStart(outdoor_update_timer, 0);
	
	xTaskCreate(key_scan_task, "key_scan", 512, NULL, 3, NULL);
}
