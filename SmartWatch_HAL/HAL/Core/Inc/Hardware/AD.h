/**
  * @file           : AD.h
  * @brief          : ADC电池电压读取驱动（HAL库版本）
  * @author         : CHSGY
  * @date           : 2026-06-05
  *
  * @note           : ADC1通道2(PA2)，CubeMX已完成外设配置和GPIO配置
  *                   无需AD_Init()，直接调用AD_GetValue()即可
  */

#ifndef __AD_H
#define __AD_H

#include "main.h"

uint16_t AD_GetValue(void);

#endif
