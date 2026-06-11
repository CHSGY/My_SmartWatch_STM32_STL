/**
  * @file           : power.c
  * @brief          : 开关机电源控制模块（HAL库版本）
  * @author         : CHSGY
  * @date           : 2026-06-05
  *
  * @note           : 从标准库menu.c中的电源控制逻辑独立而来
  *                   电源控制引脚：
  *                   - PB13 (POWER_CONTROL_Pin): MCU电源PMOS控制
  *                   - PB12 (ADC_CONTROL_Pin):   ADC/高压PMOS控制
  */

#include "power.h"

/**
  * @brief  获取当前电源状态
  * @retval 1=系统运行中，0=已关机
  */
uint8_t POWER_IsRunning(void)
{
	return (HAL_GPIO_ReadPin(POWER_CONTROL_GPIO_Port, POWER_CONTROL_Pin) == GPIO_PIN_SET);
}

/**
  * @brief  执行关机
  * @retval 无
  * @note   关闭ADC/高压PMOS，关闭MCU电源PMOS
  *         调用后MCU将断电停止运行
  */
void POWER_Shutdown(void)
{
	HAL_GPIO_WritePin(ADC_CONTROL_GPIO_Port, ADC_CONTROL_Pin, GPIO_PIN_SET);		/* PB12高=PMOS截止，断开ADC/高压 */
	HAL_GPIO_WritePin(POWER_CONTROL_GPIO_Port, POWER_CONTROL_Pin, GPIO_PIN_RESET);	/* PB13低=MCU关机 */
}

void POWER_Boot(void)
{
  HAL_GPIO_WritePin(ADC_CONTROL_GPIO_Port, ADC_CONTROL_Pin, GPIO_PIN_RESET);		/* PB12低=PMOS导通，开启ADC/高压 */
  HAL_GPIO_WritePin(POWER_CONTROL_GPIO_Port, POWER_CONTROL_Pin, GPIO_PIN_SET);	/* PB13高=MCU运行 */
}