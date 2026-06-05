/**
  * @file           : delay.h
  * @brief          : 基于DWT周期计数器的微秒延时模块
  * @author         : CHSGY
  * @date           : 2026-06-05
  *
  * @note           : 使用Cortex-M3内核DWT CYCCNT实现周期级精确延时
  *                   与FreeRTOS兼容（不占用SysTick），用完即关，不增加静态功耗
  */

#ifndef __DELAY_H
#define __DELAY_H

#include "main.h"

void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

#endif
