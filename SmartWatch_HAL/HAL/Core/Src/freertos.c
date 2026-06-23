/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Hardware/Key.h"
#include "power.h"
#include "Hardware/menu.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void Task_Input(void *pvParameters);
void Task_UI(void *pvParameters);
extern void Task_UI_RenderFrame(uint8_t key);
/* USER CODE END FunctionPrototypes */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* GetTimerTaskMemory prototype (linked to static allocation support) */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize );

/* Hook prototypes */
void vApplicationIdleHook(void);
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 2 */
void vApplicationIdleHook( void )
{
   /* Phase 3: 统一休眠点 — 替代原 9 个页面函数中各自的手动 __WFI()
    * 空闲任务每次迭代执行 WFI，MCU 进入低功耗等待中断唤醒。
    * 不能阻塞、不能调用 API。 */
   __WFI();
}
/* USER CODE END 2 */

/* USER CODE BEGIN 4 */
__weak void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
__weak void vApplicationMallocFailedHook(void)
{
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
}
/* USER CODE END 5 */

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/* USER CODE BEGIN GET_TIMER_TASK_MEMORY */
static StaticTask_t xTimerTaskTCBBuffer;
static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
  *ppxTimerTaskStackBuffer = &xTimerStack[0];
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
  /* place for user code */
}
/* USER CODE END GET_TIMER_TASK_MEMORY */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
  * @brief  Task_Input — 按键处理任务
  * @param  pvParameters: 未使用
  * @retval 无
  * @note   优先级 3（最高用户任务），栈 384 bytes
  *         阻塞等待 TIM2 ISR 的 Task Notification，
  *         处理全局按键（Key3 长按关机），
  *         其他按键转发给 Task_UI（Phase 3 实现）。
  */
void Task_Input(void *pvParameters)
{
  uint8_t key;
  (void)pvParameters;

  for (;;)
  {
    /* 阻塞等待 ISR 通知（无限等待，零 CPU 开销） */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    /* 读取并消费按键值 */
    key = Key_GetNum();

    if (key == 4)
    {
      /* Key3 长按 — 全局关机，不依赖当前页面 */
      if (POWER_IsRunning())
      {
        POWER_Shutdown();
      }
    }
    else if (key != 0)
    {
      /* 其他按键转发给 Task_UI（Phase 3 实现） */
      if (Task_UI_Handle != NULL)
      {
        xTaskNotify(Task_UI_Handle, key, eSetValueWithOverwrite);
      }
    }
  }
}

/**
  * @brief  Task_UI — 统一页面渲染任务
  * @param  pvParameters: 未使用
  * @retval 无
  * @note   优先级 2，栈 1280 bytes (320 words)
  *         独占 OLED I2C 总线，所有 11 个页面在此任务内渲染。
  *         xTaskNotifyWait(33ms) 同时实现帧率控制（30FPS）和按键事件驱动。
  *         实际渲染逻辑委托给 menu.c 的 Task_UI_RenderFrame()。
  */
void Task_UI(void *pvParameters)
{
  uint32_t key;
  (void)pvParameters;

  /* 初始页面已在 g_CurrentPage 中设为 PAGE_CLOCK */

  for (;;)
  {
    /* 等待按键通知（33ms 超时 = 30FPS 帧驱动，有按键时提前唤醒） */
    if (xTaskNotifyWait(0, 0xffffffffUL, &key, pdMS_TO_TICKS(FRAME_PERIOD_MS)) == pdTRUE)
    {
      /* 有按键：处理状态转换 + 渲染 */
      Task_UI_RenderFrame((uint8_t)key);
    }
    else
    {
      /* 超时（33ms 到期）：仅渲染（无按键） */
      Task_UI_RenderFrame(0);
    }
  }
}

/* USER CODE END Application */

