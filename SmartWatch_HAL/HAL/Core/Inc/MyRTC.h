/**
  * @file           : MyRTC.h
  * @brief          : RTC驱动（HAL库版本）
  * @author         : CHSGY
  * @date           : 2026-06-06
  *
  * @note           : 从标准库MyRTC移植
  *                   使用HAL库RTC API，保持MyRTC_Time[]接口兼容
  */

#ifndef __MYRTC_H
#define __MYRTC_H

#include "main.h"

extern int16_t MyRTC_Time[];

void MyRTC_Init(void);
void MyRTC_SetTime(void);
void MyRTC_ReadTime(void);

#endif