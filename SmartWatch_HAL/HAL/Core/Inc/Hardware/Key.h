/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : Key.h
  * @brief          : 按键驱动头文件
  ******************************************************************************
  * @attention
  *
  * 支持3个按键：
  * 按键1 - PB1 (上/左选择)
  * 按键2 - PA6 (下/右选择)
  * 按键3 - PA4 (确认/长按关机)
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __KEY_H
#define __KEY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  按键初始化
  * @param  无
  * @retval 无
  * @note   GPIO时钟和引脚配置由CubeMX的MX_GPIO_Init()完成
  *         此函数仅初始化按键模块的内部变量
  */
void Key_Init(void);

/**
  * @brief  获取按键键码
  * @param  无
  * @retval 按键键码，范围：0~4
  *         0: 无按键
  *         1: 按键1
  *         2: 按键2
  *         3: 按键3短按
  *         4: 按键3长按
  * @note   此函数会清除Key_Num，防止重复识别
  */
uint8_t Key_GetNum(void);

/**
  * @brief  获取当前按键状态
  * @param  无
  * @retval 按键状态，见Key_GetNum返回值说明
  */
uint8_t Key_GetState(void);

/**
  * @brief  按键扫描滴答函数
  * @param  无
  * @retval 无
  * @note   需要在定时器中断(1ms)中调用
  */
void KeyTick(void);

/**
  * @brief  按键3按住时间计数
  * @param  无
  * @retval 无
  * @note   需要在定时器中断(1ms)中调用
  */
void Key3_Tick(void);

/**
  * @brief  检查是否有待处理的按键
  * @param  无
  * @retval 0: 无按键, 1: 有按键
  * @note   仅查询不消费，供 ISR 判断是否需要通知 Task_Input
  */
uint8_t Key_HasPending(void);

/* 按键处理任务句柄，由 main.c 创建任务时赋值，ISR 用于通知 */
extern TaskHandle_t Task_Input_Handle;

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __KEY_H */
