#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "workqueue.h"

typedef struct
{
    work_t work;  //函数指针变量work
    void *param;    //函数参数
} work_message_t;

static QueueHandle_t work_msg_queue;   

static void work_func(void *param)
{
    work_message_t msg;      //msg里有函数指针变量和一个void指针

    
    while (1)
    {
        xQueueReceive(work_msg_queue, &msg, portMAX_DELAY);  //等待接收消息，没有就阻塞
        msg.work(msg.param);  //这里其实也是回调的写法只是这个回调在一个结构体里（具体函数+参数）
    }
}

void workqueue_init(void)
{
    work_msg_queue = xQueueCreate(16, sizeof(work_message_t));    //创建工作队列
    configASSERT(work_msg_queue);  //断言
    xTaskCreate(work_func, "workqueue", 1024, NULL, 5, NULL);   //创建打工人任务（最关键的地方）
}

void workqueue_run(work_t work, void *param)   //表示要丢进来work_t这个模板的函数，还有具体参数
{
    configASSERT(work_msg_queue);
    work_message_t msg = { work, param };  //局部变量来存其他人丢进来的要执行的函数
    xQueueSend(work_msg_queue, &msg, portMAX_DELAY);   //把具体要执行的函数丢到工作队列里去，等待work_func来执行
}
