/**
  * @file           : AD.c
  * @brief          : ADC电池电压读取驱动（HAL库版本）
  * @author         : CHSGY
  * @date           : 2026-06-05
  *
  * @note           : 从标准库移植，ADC1通道2(PA2)
  *                   CubeMX的MX_ADC1_Init()已完成外设配置
  *                   此文件仅提供AD_GetValue()读取函数
  */

#include "Hardware/AD.h"

extern ADC_HandleTypeDef hadc1;		/* CubeMX生成的ADC1句柄，在main.c中定义 */

/**
  * @brief  获取AD转换的值
  * @retval AD转换的值，范围：0~4095
  */
uint16_t AD_GetValue(void)
{
	HAL_ADC_Start(&hadc1);							/* 启动ADC转换 */
	HAL_ADC_PollForConversion(&hadc1, 100);			/* 等待转换结束，超时100ms */
	return (uint16_t)HAL_ADC_GetValue(&hadc1);		/* 读取转换结果 */
}
