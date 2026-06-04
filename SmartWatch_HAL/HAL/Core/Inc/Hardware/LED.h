/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : LED.h
  * @brief          : LED驱动头文件
  ******************************************************************************
  * @attention
  *
  * LED硬件连接：
  *   LED1 (Flashlight) - PA0
  *   LED2 - PB12
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __LED_H
#define __LED_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  LED1开启（PA0低电平）
  */
void LED1_ON(void);

/**
  * @brief  LED1关闭（PA0高电平）
  */
void LED1_OFF(void);

/**
  * @brief  LED1状态翻转
  */
void LED1_Turn(void);

/**
  * @brief  LED2开启（PB12低电平）
  */
void LED2_ON(void);

/**
  * @brief  LED2关闭（PB12高电平）
  */
void LED2_OFF(void);

/**
  * @brief  LED2状态翻转
  */
void LED2_Turn(void);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H */
