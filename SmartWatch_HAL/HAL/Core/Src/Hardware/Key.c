/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : Key.c
  * @brief          : 按键驱动源文件
  ******************************************************************************
  * @attention
  *
  * 按键硬件连接：
  *   按键1 - PB1 (上/左选择)
  *   按键2 - PA6 (下/右选择)
  *   按键3 - PA4 (确认/长按关机)
  *
  * 功能说明：
  *   - 支持按键消抖（20ms）
  *   - 支持按键3长按检测（>=1000ms）
  *   - 需要在1ms定时器中断中调用KeyTick()和Key3_Tick()
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "Hardware/Key.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PT */

/* USER CODE END PT */

/* Private define ------------------------------------------------------------*/

/** @brief 按键消抖周期(ms) */
#define KEY_DEBOUNCE_MS         20

/** @brief 长按判定阈值(ms) */
#define KEY_LONG_PRESS_MS       1000

/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/** @brief 按键键码值，由KeyTick()更新，Key_GetNum()读取后清零 */
static volatile uint8_t Key_Num = 0;

/** @brief 按键定时计数器，每1ms递增 */
static volatile uint8_t KeyTimeFlag = 0;

/** @brief 前一次按键状态 */
static volatile uint8_t Pre_KeyState = 0;

/** @brief 当前按键状态 */
static volatile uint8_t Cur_KeyState = 0;

/** @brief 按键3按住时间计数，单位ms */
static uint16_t press_time = 0;

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
  * @brief  按键初始化
  * @param  无
  * @retval 无
  * @note   GPIO时钟和引脚配置由CubeMX的MX_GPIO_Init()完成
  *         此函数仅初始化按键模块的内部变量
  */
void Key_Init(void)
{
  /* USER CODE BEGIN Key_Init */
  Key_Num = 0;
  KeyTimeFlag = 0;
  Pre_KeyState = 0;
  Cur_KeyState = 0;
  press_time = 0;
  /* USER CODE END Key_Init */
}

/**
  * @brief  获取按键键码
  * @param  无
  * @retval 按键键码，范围：0~4
  *         0: 无按键
  *         1: 按键1
  *         2: 按键2
  *         3: 按键3短按
  *         4: 按键3长按
  * @note   此函数会清除Key_Num，防止重复识别
  */
uint8_t Key_GetNum(void)
{
  /* USER CODE BEGIN Key_GetNum */
  uint8_t Temp;
  if(Key_Num)
  {
    Temp = Key_Num;
    Key_Num = 0;  /* 清空按键值，防止重复识别 */
    return Temp;
  }
  else
  {
    return 0;
  }
  /* USER CODE END Key_GetNum */
}

/**
  * @brief  获取当前按键状态
  * @param  无
  * @retval 按键状态：
  *           0: 无按键按下
  *           1: 按键1按下 (PB1)
  *           2: 按键2按下 (PA6)
  *           3: 按键3短按 (PA4, 按住时间<1000ms)
  *           4: 按键3长按 (PA4, 按住时间>=1000ms)
  */
uint8_t Key_GetState(void)
{
  /* USER CODE BEGIN Key_GetState */
  if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
  {
    return 1;
  }
  else if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET)
  {
    return 2;
  }
  else if(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_RESET && press_time < KEY_LONG_PRESS_MS)
  {
    return 3;
  }
  else if(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_RESET && press_time >= KEY_LONG_PRESS_MS)
  {
    return 4;
  }
  else
  {
    return 0;
  }
  /* USER CODE END Key_GetState */
}

/**
  * @brief  按键扫描滴答函数
  * @param  无
  * @retval 无
  * @note   此函数在1ms定时器中断中调用，每20ms检测一次按键状态
  *         用于检测按键的按下并松开事件，检测到后更新Key_Num值
  *         外部可通过Key_GetNum()获取按键值
  */
void KeyTick(void)
{
  /* USER CODE BEGIN KeyTick */
  KeyTimeFlag++;
  if(KeyTimeFlag >= KEY_DEBOUNCE_MS)                 /* 20ms触发一次判断按键状态 */
  {
    Pre_KeyState = Cur_KeyState;             /* 赋值给前回状态 */
    Cur_KeyState = Key_GetState();           /* 获取当前按键状态并赋值给当前状态 */
    if(Pre_KeyState != 0 && Cur_KeyState == 0) /* 表示某个按键被按下并松开 */
    {
      Key_Num = Pre_KeyState;                /* 前回状态表示按键值 */
    }
    KeyTimeFlag = 0;
  }
  /* USER CODE END KeyTick */
}

/**
  * @brief  按键3按住时间计数
  * @param  无
  * @retval 无
  * @note   此函数在1ms定时器中断中调用，用于检测按键3是否长按
  *         当按键3按住时，press_time累加；松开时清零
  *         用于区分按键3的短按和长按状态
  */
void Key3_Tick(void)
{
  /* USER CODE BEGIN Key3_Tick */
  if(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_RESET)
  {
    press_time++;
  }
  
  if(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_SET)
  {
    press_time = 0;
  }
  /* USER CODE END Key3_Tick */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
