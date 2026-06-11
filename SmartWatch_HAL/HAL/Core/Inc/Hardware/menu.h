/**
  * @file           : menu.h
  * @brief          : 主菜单及功能模块头文件（HAL库版本）
  * @author         : CHSGY
  * @date           : 2026-06-07
  *
  * @note           : 从标准库menu.h移植
  *                   所有宏定义保持不变，仅修改了头文件包含
  *                   电源控制由独立power模块管理，不再内嵌于menu
  */

#ifndef __MENU_H
#define __MENU_H

#include "main.h"

/* ================================================================
 *  菜单/UI/秒表/电池/表情/水平仪参数宏定义
 * ================================================================ */

/* ---- 首页时钟 ---- */
#define CLOCK_DATE_X            0       /* 日期显示 X */
#define CLOCK_DATE_Y            0       /* 日期显示 Y */
#define CLOCK_TIME_X            16      /* 时间显示 X */
#define CLOCK_TIME_Y            16      /* 时间显示 Y */
#define CLOCK_MENU_TEXT_X       0       /* "菜单"文字 X */
#define CLOCK_MENU_TEXT_Y       48      /* "菜单"文字 Y */
#define CLOCK_SET_TEXT_X        96      /* "设置"文字 X */
#define CLOCK_SET_TEXT_Y        48      /* "设置"文字 Y */

/* ---- 菜单动画 ---- */
#define MENU_CURSOR_MIN         1       /* 菜单光标最小值 */
#define MENU_CURSOR_MAX         7       /* 菜单光标最大值（7个菜单项） */
#define MENU_FRAME_X            42      /* 选择框 X */
#define MENU_FRAME_Y            10      /* 选择框 Y */
#define MENU_FRAME_W            44      /* 选择框宽度 */
#define MENU_FRAME_H            44      /* 选择框高度 */
#define MENU_ICON_BASE_X        48      /* 图标基准 X 位置 */
#define MENU_ICON_Y             16      /* 图标 Y 起始位置 */
#define MENU_ICON_SIZE          32      /* 菜单图标尺寸(32x32) */
#define MENU_ICON_SPACING       48      /* 图标间距(px) */
#define MENU_SLIDE_STEP         8       /* 菜单滑动步长(px/帧) */
#define MENU_ENTER_FRAMES       6       /* 进场动画帧数 */
#define MENU_ENTER_STEP         8       /* 进场动画步距(px/帧) */

/* ---- 秒表 ---- */
#define STOPCLK_1S_TICKS        1000    /* 1秒 = 1000 个 tick */
#define STOPCLK_SEC_MAX         60      /* 秒进位阈值 */
#define STOPCLK_MIN_MAX         60      /* 分进位阈值 */
#define STOPCLK_HOUR_MAX        99      /* 小时最大值（2位显示限制） */
#define STOPCLK_TIME_X          32      /* 秒表时间显示 X */
#define STOPCLK_TIME_Y          20      /* 秒表时间显示 Y */
#define STOPCLK_BTN_START_X     8       /* "开始"按钮 X */
#define STOPCLK_BTN_STOP_X      48      /* "停止"按钮 X */
#define STOPCLK_BTN_CLEAR_X     88      /* "清除"按钮 X */
#define STOPCLK_BTN_Y           44      /* 按钮统一 Y 坐标 */
#define STOPCLK_BTN_W           32      /* 按钮宽度 */
#define STOPCLK_BTN_H           16      /* 按钮高度 */

/* ---- 电池 ---- */
#define BATTERY_REFRESH_FRAMES  50      /* 电池刷新周期(帧) */
#define BATTERY_ADC_SAMPLES     16      /* ADC 过采样次数 */
#define BATTERY_ADC_MAX         4092    /* 12-bit ADC 满量程值 */
#define BATTERY_ADC_EMPTY       3276    /* 电池空(2.64V)对应 ADC 值 */
#define BATTERY_ICON_X          110     /* 电池图标 X */
#define BATTERY_ICON_Y          0       /* 电池图标 Y */
#define BATTERY_ICON_W          16      /* 电池图标宽度 */
#define BATTERY_ICON_H          16      /* 电池图标高度 */
#define BATTERY_BAR_X           113     /* 电量条 X 起始 */
#define BATTERY_BAR_Y           5       /* 电量条 Y 起始 */
#define BATTERY_BAR_W           10      /* 电量条最大宽度 */
#define BATTERY_BAR_H           6       /* 电量条高度 */

/* ---- MPU6050 / 水平仪 ---- */
#define MPU_SAMPLE_PERIOD_S     0.005f  /* 采样周期(秒) */
#define MPU_FILTER_ALPHA        0.9f    /* 互补滤波系数（陀螺仪权重） */
#define MPU_SAMPLE_DELAY_MS     5       /* 采样延时(ms)，与 SAMPLE_PERIOD 对应 */
#define GRADIENTER_CENTER_X     64      /* 水平仪外圆圆心 X */
#define GRADIENTER_CENTER_Y     32      /* 水平仪外圆圆心 Y */
#define GRADIENTER_OUTER_R      30      /* 水平仪外圆半径 */
#define GRADIENTER_BOUNDARY_R   26.0f   /* 小圆运动边界半径 */
#define GRADIENTER_INNER_R      4       /* 水平仪小圆半径 */

/* ---- 表情动画 ---- */
#define EMOJI_BLINK_FRAMES      3       /* 眨眼动画帧数（0~3 共4帧） */
#define EMOJI_L_EYEBROW_X       30      /* 左眉 X */
#define EMOJI_R_EYEBROW_X       82      /* 右眉 X */
#define EMOJI_EYEBROW_Y         10      /* 眉毛 Y 起始 */
#define EMOJI_L_EYE_CX          40      /* 左眼圆心 X */
#define EMOJI_R_EYE_CX          88      /* 右眼圆心 X */
#define EMOJI_EYE_CY            32      /* 眼睛圆心 Y */
#define EMOJI_EYE_RX            6       /* 眼睛椭圆 X 半径 */
#define EMOJI_EYE_RY_MAX        6       /* 眼睛椭圆 Y 最大半径（睁眼） */
#define EMOJI_MOUTH_X           54      /* 嘴巴 X */
#define EMOJI_MOUTH_Y           40      /* 嘴巴 Y */
#define EMOJI_MOUTH_W           20      /* 嘴巴宽度 */
#define EMOJI_MOUTH_H           20      /* 嘴巴高度 */
#define EMOJI_BLINK_DELAY_MS    100     /* 眨眼帧延时(ms) */
#define EMOJI_BLINK_GAP_MS      500     /* 眨眼间隔延时(ms) */

/**
  * @brief 电池电量显示UI
  */
void Battery_Show_UI(void);

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

/**
  * @brief SetTime主流程
  * @retval 0-返回
  */
int SetTime_mainprocess(void);

#endif
