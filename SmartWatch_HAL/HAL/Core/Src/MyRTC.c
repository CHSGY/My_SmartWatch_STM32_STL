/**
  * @file           : MyRTC.c
  * @brief          : RTC驱动（HAL库版本）
  * @author         : CHSGY
  * @date           : 2026-06-06
  *
  * @note           : 从标准库MyRTC移植
  *                   标准库使用秒计数器 + mktime/localtime
  *                   HAL库使用RTC_TimeTypeDef + RTC_DateTypeDef结构体
  *                   
  *                   备份寄存器用于判断是否首次配置
  *                   首次配置时设置默认时间，否则保留RTC硬件时间
  */

#include "MyRTC.h"

extern RTC_HandleTypeDef hrtc;

int16_t MyRTC_Time[] = {2025, 9, 10, 23, 10, 55};

/**
  * @brief  RTC初始化
  * @note   检查备份寄存器判断是否首次配置
  *         首次配置时设置默认时间
  */
void MyRTC_Init(void)
{
	if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != 0xA5A5)
	{
		MyRTC_SetTime();
		HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0xA5A5);
	}
}

/**
  * @brief  设置RTC时间
  * @note   将MyRTC_Time数组的时间写入RTC硬件
  */
void MyRTC_SetTime(void)
{
	RTC_TimeTypeDef sTime = {0};
	RTC_DateTypeDef sDate = {0};
	
	sTime.Hours = MyRTC_Time[3];
	sTime.Minutes = MyRTC_Time[4];
	sTime.Seconds = MyRTC_Time[5];
	
	sDate.Year = MyRTC_Time[0] - 2000;
	sDate.Month = MyRTC_Time[1];
	sDate.Date = MyRTC_Time[2];
	
	HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}

/**
  * @brief  读取RTC时间
  * @note   将RTC硬件时间读取到MyRTC_Time数组
  */
void MyRTC_ReadTime(void)
{
	RTC_TimeTypeDef sTime = {0};
	RTC_DateTypeDef sDate = {0};
	
	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
	
	MyRTC_Time[0] = sDate.Year + 2000;
	MyRTC_Time[1] = sDate.Month;
	MyRTC_Time[2] = sDate.Date;
	MyRTC_Time[3] = sTime.Hours;
	MyRTC_Time[4] = sTime.Minutes;
	MyRTC_Time[5] = sTime.Seconds;
}