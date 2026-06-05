/**
  * @file           : power.h
  * @brief          : 开关机电源控制模块（HAL库版本）
  * @author         : CHSGY
  * @date           : 2026-06-05
  *
  * @note           : 从标准库menu.c中的电源控制逻辑独立而来
  *
  *                   硬件说明：
  *                   - PB13: 控制MCU电源PMOS（高电平=系统运行，低电平=关机）
  *                   - PB12: 控制ADC/高压PMOS（低电平=ADC供电开启，高电平=ADC断开）
  *                   - PMOS特性：低电平导通，高电平截止
  *
  *                   GPIO引脚由CubeMX在MX_GPIO_Init()中配置为推挽输出
  *                   初始化默认状态：PB13=高（运行），PB12=低（ADC开启）
  */

#ifndef __POWER_H
#define __POWER_H

#include "main.h"

uint8_t POWER_IsRunning(void);
void POWER_Shutdown(void);

#endif
