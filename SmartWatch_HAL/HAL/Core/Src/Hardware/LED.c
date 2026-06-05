/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : LED.c
  * @brief          : LED驱动源文件
  ******************************************************************************
  * @attention
  *
  * LED硬件连接：
  *   LED1 (Flashlight) - PA0
  *
  * 功能说明：
  *   - GPIO时钟和引脚配置由CubeMX的MX_GPIO_Init()完成
  *   - 提供LED1手电筒的开关和翻转控制
  *   - PB12/PB13为电源控制引脚，由power模块管理
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "Hardware/LED.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PT */

/* USER CODE END PT */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  LED1开启
  * @param  无
  * @retval 无
  */
void LED1_ON(void)
{
  /* USER CODE BEGIN LED1_ON */
  HAL_GPIO_WritePin(LED1_Flashlight_GPIO_Port, LED1_Flashlight_Pin, GPIO_PIN_RESET);
  /* USER CODE END LED1_ON */
}

/**
  * @brief  LED1关闭
  * @param  无
  * @retval 无
  */
void LED1_OFF(void)
{
  /* USER CODE BEGIN LED1_OFF */
  HAL_GPIO_WritePin(LED1_Flashlight_GPIO_Port, LED1_Flashlight_Pin, GPIO_PIN_SET);
  /* USER CODE END LED1_OFF */
}

/**
  * @brief  LED1状态翻转
  * @param  无
  * @retval 无
  */
void LED1_Turn(void)
{
  /* USER CODE BEGIN LED1_Turn */
  if (HAL_GPIO_ReadPin(LED1_Flashlight_GPIO_Port, LED1_Flashlight_Pin) == GPIO_PIN_RESET)
  {
    HAL_GPIO_WritePin(LED1_Flashlight_GPIO_Port, LED1_Flashlight_Pin, GPIO_PIN_SET);
  }
  else
  {
    HAL_GPIO_WritePin(LED1_Flashlight_GPIO_Port, LED1_Flashlight_Pin, GPIO_PIN_RESET);
  }
  /* USER CODE END LED1_Turn */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
