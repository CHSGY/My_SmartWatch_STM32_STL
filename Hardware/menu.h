#ifndef __MENU_H
#define __MENU_H

/**
  * @brief 主菜单及功能模块
  * @details 包含首页时钟、设置页面、菜单页面及各功能实现
  */

/**
  * @brief 外设初始化
  */
void Peripheral_Init(void);

/**
  * @brief 电池电量显示UI
  */
void Battery_Show_UI(void);
void Battery_Func(void);

/**
  * @brief 首页时钟界面显示
  */
void Show_Clock_UI(void);

/**
  * @brief 首页时钟页面按键控制逻辑
  * @retval 返回值：1-进入菜单 2-进入设置
  */
uint8_t First_Page_Clock(void);

/**
  * @brief 显示设置页面UI
  */
void Show_Setting_UI(void);

/**
  * @brief 设置页面主函数
  * @retval 0-返回首页
  */
uint8_t SettingPage(void);

/**
  * @brief 菜单滑动动画
  */
void Menu_Animation(void);

/**
  * @brief 菜单页面主函数
  * @retval 0-返回首页
  */
uint8_t Menu_Page(void);

/**
  * @brief 显示秒表UI
  */
void Show_StopClock_UI(void);

/**
  * @brief 秒表计时滴答函数（中断调用）
  */
void StopClock_Tick(void);

/**
  * @brief 秒表功能主函数
  * @retval 0-返回
  */
int StopClock(void);

/**
  * @brief 显示手电筒UI
  */
void Show_flashlight_UI(void);

/**
  * @brief 手电筒功能主函数
  * @retval 0-返回
  */
int flashlight_Func(void);

/**
  * @brief MPU6050欧拉角解算（互补滤波）
  */
void MPU6050_Calculation_Euler_angles(void);

/**
  * @brief 显示MPU6050数据UI
  */
void Show_MPU6050_UI(void);

/**
  * @brief MPU6050功能主函数
  * @retval 0-返回
  */
int MPU6050_Main(void);

/**
  * @brief 游戏选择页面主函数
  * @retval 0-返回
  */
int Game(void);

/**
  * @brief 显示动态表情UI
  */
void Show_emoji_UI(void);

/**
  * @brief 动态表情功能主函数
  * @retval 0-返回
  */
int Emoji_Func(void);

/**
  * @brief 显示水平仪UI
  */
void Show_Gradienter_UI(void);

/**
  * @brief 水平仪功能主函数
  * @retval 0-返回
  */
uint8_t Gradienter_Func(void);

#endif
