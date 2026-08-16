#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"
#include "tim_delay.h"
#include <stdio.h>  

static bool aht20_write(uint8_t data[], uint32_t length);
static bool aht20_read(uint8_t data[], uint32_t length);
static bool aht20_is_ready(void);
static bool aht20_read_status(uint8_t *status);

/*bool aht20_init(void)
{
    I2C_InitTypeDef I2C_InitStruct;
    I2C_StructInit(&I2C_InitStruct);
    I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStruct.I2C_ClockSpeed = 100ul * 1000ul;
    I2C_InitStruct.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStruct.I2C_OwnAddress1 = 0x00;
    I2C_Init(I2C2, &I2C_InitStruct);

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
	//GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStruct.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_Init(GPIOB, &GPIO_InitStruct);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_I2C2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_I2C2);
    
    vTaskDelay(pdMS_TO_TICKS(40));
    if (aht20_is_ready())
        return true;
    
    if (!aht20_write((uint8_t[]){0xBE, 0x08, 0x00}, 3))
        return false;
    
    for (uint32_t t = 0; t < 20; t ++)
    {
        vTaskDelay(pdMS_TO_TICKS(5));
        if (aht20_is_ready())
            return true;
    }
    
    return false;
}

*/

bool aht20_init(void)
{
    I2C_InitTypeDef I2C_InitStruct;
    I2C_StructInit(&I2C_InitStruct);
    I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStruct.I2C_ClockSpeed = 100ul * 1000ul;
    I2C_InitStruct.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStruct.I2C_OwnAddress1 = 0x00;
    I2C_Init(I2C2, &I2C_InitStruct);

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStruct.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_Init(GPIOB, &GPIO_InitStruct);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_I2C2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_I2C2);
    
    vTaskDelay(pdMS_TO_TICKS(40));
    
    // ★★★ 尝试发送初始化命令，最多重试 3 次 ★★★
    uint8_t status;
    for (int retry = 0; retry < 3; retry++) {
        if (!aht20_write((uint8_t[]){0xBE, 0x08, 0x00}, 3)) {
            printf("[AHT20] Init command write failed (retry %d)\n", retry);
            continue;
        }
        
        // ★★★ 等待校准完成，最多 300ms（原先是 100ms） ★★★
        for (uint32_t t = 0; t < 60; t++) {
            vTaskDelay(pdMS_TO_TICKS(5));
            if (!aht20_read_status(&status))
                continue;
            // 检查 ready（Bit3）和 cal（Bit0）是否都为 1
            if ((status & 0x08) && (status & 0x01)) {
                printf("[AHT20] Init+Calibration done, status=0x%02X\n", status);
                return true;
            }
        }
        printf("[AHT20] Calibration timeout (retry %d), status=0x%02X\n", retry, status);
    }
    
    printf("[AHT20] Init failed after 3 retries, status=0x%02X\n", status);
    return false;
}

#define I2C_CHECK_EVENT(EVENT, TIMEOUT) \
    do { \
        uint32_t timeout = TIMEOUT; \
        while (!I2C_CheckEvent(I2C2, EVENT) && timeout > 0) { \
            tim_delay_us(10); \
            timeout -= 10; \
        } \
        if (timeout <= 0) \
            return false; \
    } while (0)

static bool aht20_write(uint8_t data[], uint32_t length)
{
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    I2C_GenerateSTART(I2C2, ENABLE);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_MODE_SELECT, 1000);
    I2C_Send7bitAddress(I2C2, 0x70, I2C_Direction_Transmitter);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, 1000);
    for (uint32_t i = 0; i < length; i++)
    {
        I2C_SendData(I2C2, data[i]);
        I2C_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_TRANSMITTING, 1000);
    }
    I2C_GenerateSTOP(I2C2, ENABLE);
    
    return true;
}


static bool aht20_read(uint8_t data[], uint32_t length)
{
    // ★★★ 先使能 ACK（默认回复 ACK） ★★★
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    
    I2C_GenerateSTART(I2C2, ENABLE);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_MODE_SELECT, 1000);
    I2C_Send7bitAddress(I2C2, 0x70, I2C_Direction_Receiver);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, 1000);
    
    for (uint32_t i = 0; i < length; i++)
    {
        // ★★★ 关键修复：在接收最后一个字节之前，先关闭 ACK ★★★
        if (i == length - 1) {
            I2C_AcknowledgeConfig(I2C2, DISABLE);
        }
        
        I2C_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_RECEIVED, 1000);
        data[i] = I2C_ReceiveData(I2C2);
    }
    
    I2C_GenerateSTOP(I2C2, ENABLE);
    
    // ★★★ 重新使能 ACK，为下一次通信准备 ★★★
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    
    return true;
}

static bool aht20_read_status(uint8_t *status)
{
    uint8_t cmd = 0x71;
    if (!aht20_write(&cmd, 1))
        return false;
    if (!aht20_read(status, 1))
        return false;
    
    return true;
}

static bool aht20_is_busy(void)
{
    uint8_t status;
    if (!aht20_read_status(&status))
        return false;
    return (status & 0x80) != 0;
}

 static bool aht20_is_ready(void)
{
    uint8_t status;
    if (!aht20_read_status(&status))
        return false;
    return (status & 0x08) != 0;
} 

bool aht20_start_measurement(void)
{
    return aht20_write((uint8_t[]){0xAC, 0x33, 0x00}, 3);
}

bool aht20_wait_for_measurement(void)
{
    for (uint32_t t = 0; t < 20; t++)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        if (!aht20_is_busy())
        {
            return true;
        }
    }
    return false;
}

bool aht20_read_measurement(float *temperature, float *humidity)
{
    uint8_t data[6];
    if (!aht20_read(data, 6))
        return false;
    
    uint32_t raw_humidity = ((uint32_t)data[1] << 12) | 
                            ((uint32_t)data[2] << 4) | 
                            ((uint32_t)(data[3] &0xF0) >> 4);
    uint32_t raw_temperature = ((uint32_t)(data[3] & 0x0F) << 16) | 
                               ((uint32_t)data[4] << 8) | 
                               ((uint32_t)data[5]);
    
    *humidity = (float)raw_humidity * 100.0f / (float)0x100000;
    *temperature = (float)raw_temperature * 200.0f / (float)0x100000 - 50.0f;
    
    return true;
}








void aht20_i2c_scan(void)
{
    printf("\r\n===== I2C Scan Start =====\r\n");
    printf("Scanning I2C2 (PB10/PB11)...\r\n");
    
    uint8_t found_count = 0;
    for (uint8_t addr_7bit = 1; addr_7bit < 127; addr_7bit++)
    {
        // 7位地址左移1位，变成8位写地址
        uint8_t dev_addr = addr_7bit << 1;
        
        // 产生 START 信号
        I2C_GenerateSTART(I2C2, ENABLE);
        uint32_t timeout = 1000;
        while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT) && timeout > 0) {
            tim_delay_us(10);
            timeout--;
        }
        if (timeout == 0) {
            printf("[ERROR] I2C bus stuck, check wiring!\r\n");
            return;
        }
        
        // 发送 7 位地址 + 写位
        I2C_Send7bitAddress(I2C2, dev_addr, I2C_Direction_Transmitter);
        
        // 等待地址被应答（如果传感器存在，会拉低 SDA 应答）
        timeout = 1000;
        while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) && timeout > 0) {
            tim_delay_us(10);
            timeout--;
        }
        
        if (timeout > 0) {
            // 有设备应答
            printf("[I2C] Found device at 7-bit address: 0x%02X (8-bit: 0x%02X)\r\n", 
                   addr_7bit, dev_addr);
            found_count++;
        }
        
        // 发送 STOP 信号
        I2C_GenerateSTOP(I2C2, ENABLE);
        // 加一点延时，避免总线冲突
        tim_delay_us(100);
    }
    
    if (found_count == 0) {
        printf("[I2C] No devices found on I2C2!\r\n");
        printf("   Please check: 1) wiring (SDA=PB11, SCL=PB10, GND)\r\n");
        printf("                 2) sensor power (3.3V)\r\n");
        printf("                 3) pull-up resistors (4.7k on SDA/SCL)\r\n");
    } else {
        printf("[I2C] Total devices found: %d\r\n", found_count);
        printf("   AHT20 should be at 7-bit address 0x38 (8-bit 0x70)\r\n");
        printf("   If 0x38 is NOT in the list, check wiring/power.\r\n");
    }
    printf("===== I2C Scan End =====\r\n\r\n");
}

// ============================================================
// 2. 读取 AHT20 状态寄存器（0x71）
//    返回 8 位状态值，直接打印
// ============================================================
bool aht20_print_status(void)
{
    uint8_t status = 0;
    uint8_t cmd = 0x71;
    
    printf("[AHT20] Reading status register...\r\n");
    
    // 发送命令 0x71（读取状态）
    if (!aht20_write(&cmd, 1)) {
        printf("[AHT20] Failed to send status command (0x71)\r\n");
        return false;
    }
    
    // 读取 1 个字节
    if (!aht20_read(&status, 1)) {
        printf("[AHT20] Failed to read status byte\r\n");
        return false;
    }
    
    printf("[AHT20] Status = 0x%02X\r\n", status);
    printf("   Bit 7 (busy)  : %s\r\n", (status & 0x80) ? "YES (sensor busy)" : "NO (sensor idle)");
    printf("   Bit 3 (ready) : %s\r\n", (status & 0x08) ? "YES (initialized)" : "NO (not initialized yet)");
    printf("   Bit 0 (cal)   : %s\r\n", (status & 0x01) ? "YES (calibrated)" : "NO (not calibrated)");
    
    if ((status & 0x08) == 0) {
        printf("[WARN] Sensor not initialized! Need to send init command (0xBE).\r\n");
    }
    if ((status & 0x01) == 0) {
        printf("[WARN] Sensor not calibrated! Measurement may be inaccurate.\r\n");
    }
    
    return true;
}

// ============================================================
// 3. 完整测试流程：扫描 → 读状态 → 触发测量 → 读取数据
//    在 main 里调用 `aht20_self_test()` 即可
// ============================================================
void aht20_self_test(void)
{
    printf("\r\n========== AHT20 Self Test Start ==========\r\n\r\n");
    
	// ★★★ 强制重新初始化 ★★★
    printf("[AHT20] Forcing re-init...\n");
    if (aht20_init()) {
        printf("[AHT20] Re-init success!\n");
    } else {
        printf("[AHT20] Re-init failed!\n");
    }
	
    // ----- 第一步：I2C 扫描 -----
    aht20_i2c_scan();
    
    // ----- 第二步：读状态寄存器（看传感器是否初始化）-----
    if (!aht20_print_status()) {
        printf("[ERROR] Cannot read AHT20 status. Check wiring/power.\r\n");
        return;
    }
    
    // ----- 第三步：如果未初始化，发送初始化命令 -----
    uint8_t status;
    aht20_read_status(&status);
    if ((status & 0x08) == 0) {
        printf("[AHT20] Sending init command (0xBE, 0x08, 0x00)...\r\n");
        if (!aht20_write((uint8_t[]){0xBE, 0x08, 0x00}, 3)) {
            printf("[ERROR] Init command failed!\r\n");
            return;
        }
        // 等待初始化完成（最多100ms）
        for (int i = 0; i < 20; i++) {
            vTaskDelay(pdMS_TO_TICKS(5));
            aht20_read_status(&status);
            if (status & 0x08) {
                printf("[AHT20] Init successful after %d ms\r\n", i * 5);
                break;
            }
        }
    }
    
    // ----- 第四步：触发一次测量 -----
    printf("[AHT20] Triggering measurement (0xAC, 0x33, 0x00)...\r\n");
    if (!aht20_write((uint8_t[]){0xAC, 0x33, 0x00}, 3)) {
        printf("[ERROR] Measurement trigger failed!\r\n");
        return;
    }
    
    // 等待测量完成（最多200ms）
    printf("[AHT20] Waiting for measurement...\r\n");
    bool busy = true;
    for (int i = 0; i < 40; i++) {
        vTaskDelay(pdMS_TO_TICKS(5));
        aht20_read_status(&status);
        if ((status & 0x80) == 0) {
            busy = false;
            printf("[AHT20] Measurement ready after %d ms\r\n", i * 5);
            break;
        }
    }
    if (busy) {
        printf("[ERROR] Measurement timeout (still busy after 200ms)\r\n");
        return;
    }
    
    // ----- 第五步：读取 6 字节数据 -----
    uint8_t data[6];
    printf("[AHT20] Reading 6 bytes of data...\r\n");
    if (!aht20_read(data, 6)) {
        printf("[ERROR] Read data failed!\r\n");
        return;
    }
    
    printf("[AHT20] Raw data: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\r\n",
           data[0], data[1], data[2], data[3], data[4], data[5]);
    
    // ----- 第六步：解析温湿度 -----
    uint32_t raw_humidity = ((uint32_t)data[1] << 12) | 
                            ((uint32_t)data[2] << 4) | 
                            ((uint32_t)(data[3] & 0xF0) >> 4);
    uint32_t raw_temperature = ((uint32_t)(data[3] & 0x0F) << 16) | 
                               ((uint32_t)data[4] << 8) | 
                               ((uint32_t)data[5]);
    
    float humidity = (float)raw_humidity * 100.0f / (float)0x100000;
    float temperature = (float)raw_temperature * 200.0f / (float)0x100000 - 50.0f;
    
    printf("[AHT20] Raw humidity   : 0x%06X (%d)\r\n", raw_humidity, raw_humidity);
    printf("[AHT20] Raw temperature: 0x%06X (%d)\r\n", raw_temperature, raw_temperature);
    printf("[AHT20] *** Temperature: %.2f °C\r\n", temperature);
    printf("[AHT20] *** Humidity   : %.2f %%RH\r\n", humidity);
    
    printf("\r\n========== AHT20 Self Test End ==========\r\n");
}
