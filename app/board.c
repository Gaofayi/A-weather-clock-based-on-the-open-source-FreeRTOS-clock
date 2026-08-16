#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"
#include "tim_delay.h"
#include "console.h"
#include "rtc.h"
#include "aht20.h"

void board_lowlevel_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);      //AHT20
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);	  //LCD屏幕
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);	  //定时器
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);	  //串口传输
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);	  //SPI传输
    PWR_BackupAccessCmd(ENABLE);
    RCC_LSEConfig(RCC_LSE_ON);
    while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
}
#define KEY_GPIO_PORT      GPIOB
#define KEY_GPIO_PIN       GPIO_Pin_0
#define KEY_GPIO_CLK       RCC_AHB1Periph_GPIOB

void key_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    
    
    RCC_AHB1PeriphClockCmd(KEY_GPIO_CLK, ENABLE);
    
    
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Pin = KEY_GPIO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;      
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz; 
    GPIO_Init(KEY_GPIO_PORT, &GPIO_InitStruct);
}


uint8_t key_is_pressed(void)
{
    return (GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY_GPIO_PIN) == Bit_RESET);
}


void board_init(void)
{
    tim_delay_init();
    console_init();
    printf("[SYS] Build Date: %s %s\n", __DATE__, __TIME__);
	
    key_gpio_init();
	
    rtc_init();
    aht20_init();
	printf("[AHT20] Init result: %d\n", aht20_init());
}

int fputc(int ch, FILE *f)
{
    USART_SendData(USART1, (uint8_t)ch);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    return ch;
}

void vAssertCalled(const char *file, int line)
{
    portDISABLE_INTERRUPTS();
    printf("Assert Called: %s(%d)\n", file, line);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("Stack Overflowed: %s\n", pcTaskName);
    configASSERT(0);
}

void vApplicationMallocFailedHook(void)
{
    printf("Malloc Failed\n");
    configASSERT(0);
}
