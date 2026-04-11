#ifndef __KEY_H
#define __KEY_H

/**
  * @brief 按键模块
  * @details 支持3个按键：
  *          按键1 - PB1 (上/左选择)
  *          按键2 - PA6 (下/右选择)  
  *          按键3 - PA4 (确认/长按关机)
  */

/**
  * 函    数：按键初始化
  * 参    数：无
  * 返 回 值：无
  */
void Key_Init(void);

/**
  * 函    数：获取按键键码
  * 参    数：无
  * 返 回 值：按键键码，范围：0~4
  *           0: 无按键
  *           1: 按键1
  *           2: 按键2
  *           3: 按键3短按
  *           4: 按键3长按
  * @note  此函数会清除Key_Num，防止重复识别
  */
uint8_t Key_GetNum(void);

/**
  * 函    数：获取当前按键状态
  * 参    数：无
  * 返 回 值：按键状态，见Key_GetNum返回值说明
  */
uint8_t Key_GetState(void);

/**
  * 函    数：按键扫描滴答函数
  * 参    数：无
  * 返 回 值：无
  * @note  需要在定时器中断(1ms)中调用
  */
void KeyTick(void);

/**
  * 函    数：按键3按住时间计数
  * 参    数：无
  * 返 回 值：无
  * @note  需要在定时器中断(1ms)中调用
  */
void Key3_Tick(void);

#endif
