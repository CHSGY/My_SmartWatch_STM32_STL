#ifndef __SETTIME_H
#define __SETTIME_H

/**
  * @brief 日期时间设置模块
  */

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

/**
  * @brief 设置年份
  * @retval 0-返回
  */
int Set_Year(void);

/**
  * @brief 设置月份
  * @retval 0-返回
  */
int Set_Month(void);

/**
  * @brief 设置日期
  * @retval 0-返回
  */
int Set_Day(void);

/**
  * @brief 设置小时
  * @retval 0-返回
  */
int Set_Hour(void);

/**
  * @brief 设置分钟
  * @retval 0-返回
  */
int Set_Min(void);

/**
  * @brief 设置秒钟
  * @retval 0-返回
  */
int Set_Sec(void);

/**
  * @brief 日期时间设置主流程
  * @retval 0-返回
  */
int SetTime_mainprocess(void);

#endif


