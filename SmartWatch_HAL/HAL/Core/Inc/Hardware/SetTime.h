/**
  * @file           : SetTime.h
  * @brief          : 日期时间设置模块头文件（HAL库版本）
  * @author         : CHSGY
  * @date           : 2026-06-08
  *
  * @note           : 从标准库SetTime.h移植
  *                   头文件包含使用HAL版本路径
  */

#ifndef __SETTIME_H
#define __SETTIME_H

#include "main.h"

/**
  * @brief 显示日期设置UI
  */
void Show_SetDate_UI(void);

/**
  * @brief 显示时间设置UI
  */
void Show_SetTime_UI(void);

/**
  * @brief 修改RTC时间值
  * @param  i    时间数组索引：0-年 1-月 2-日 3-时 4-分 5-秒
  * @param  flag 修改方向：0-减1 1-加1
  */
void ChangeRTC_Time(uint8_t i,uint8_t flag);

#endif
