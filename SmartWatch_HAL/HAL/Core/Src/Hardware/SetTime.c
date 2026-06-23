/**
  * @file           : SetTime.c
  * @brief          : RTC时间设置功能（HAL库版本 / FreeRTOS状态机版）
  * @author         : CHSGY
  * @date           : 2026-06-08
  *
  * @note           : Phase 3 改造：Set_Year/Month/Day/Hour/Min/Sec + SetTime_mainprocess
  *                   的 while(1) 循环已迁移到 menu.c 的 Task_UI 状态机中。
  *                   此文件仅保留底层绘制函数和 ChangeRTC_Time()。
  */

#include "main.h"
#include "Hardware/OLED.h"
#include "Hardware/Key.h"
#include "MyRTC.h"
#include "Hardware/menu.h"
#include "Hardware/SetTime.h"


/***************************RTC时间设置功能***********************************/

/**
  * @brief 显示日期设置UI
  * @param  无
  * @retval 无
  */
void Show_SetDate_UI(void)
{
	OLED_ShowImage(0,0,16,16,GoBack);
	OLED_Printf(0,16,OLED_8X16,"%4d",MyRTC_Time[0]);		//显示年
	OLED_Printf(0,32,OLED_8X16,"%2d",MyRTC_Time[1]);		//显示月
	OLED_Printf(0,48,OLED_8X16,"%2d",MyRTC_Time[2]);      //显示日
}

/**
  * @brief 显示时间设置UI
  * @param  无
  * @retval 无
  */
void Show_SetTime_UI(void)
{
	OLED_Printf(0,0,OLED_8X16,"%2d",MyRTC_Time[3]);		//显示时
	OLED_Printf(0,16,OLED_8X16,"%2d",MyRTC_Time[4]);		//显示分
	OLED_Printf(0,32,OLED_8X16,"%2d",MyRTC_Time[5]);       //显示秒

}

/**
  * @brief 修改RTC时间值
  * @param  i    时间数组索引：0-年 1-月 2-日 3-时 4-分 5-秒
  * @param  flag 修改方向：0-减1 1-加1
  * @retval 无
  */
void ChangeRTC_Time(uint8_t i,uint8_t flag)
{
	if(flag == 0)
	{
		MyRTC_Time[i]--;
	}
	else //if(flag == 1)
	{
		MyRTC_Time[i]++;
	}
	MyRTC_SetTime(); 				//把更新后的时间值从结构体刷新到RTC硬件电路
}
