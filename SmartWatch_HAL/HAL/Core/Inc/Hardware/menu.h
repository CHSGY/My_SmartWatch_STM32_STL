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
#include "FreeRTOS.h"
#include "task.h"

/* ================================================================
 *  Task_UI 页面枚举（FreeRTOS 状态机）
 * ================================================================ */

/** @brief 页面标识符 */
typedef enum {
    PAGE_CLOCK = 0,       /* 首页时钟 */
    PAGE_MENU,            /* 菜单页面 */
    PAGE_SETTING,         /* 设置页面 */
    PAGE_STOPWATCH,       /* 秒表 */
    PAGE_FLASHLIGHT,      /* 手电筒 */
    PAGE_MPU6050,         /* 传感器数据 */
    PAGE_GAME_SELECT,     /* 游戏选择 */
    PAGE_DINO_GAME,       /* 恐龙游戏（运行中） */
    PAGE_EMOJI,           /* 表情动画 */
    PAGE_GRADIENTER,      /* 水平仪 */
    PAGE_SETTIME,         /* 设置时间 */
    PAGE_COUNT            /* 页面总数 */
} PageID_t;

/** @brief SetTime 子状态 */
typedef enum {
    SETTIME_MENU = 0,     /* 主选择菜单：返回/年/月/日/时/分/秒 */
    SETTIME_YEAR,
    SETTIME_MONTH,
    SETTIME_DAY,
    SETTIME_HOUR,
    SETTIME_MIN,
    SETTIME_SEC
} SetTimeState_t;

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
#define FRAME_PERIOD_MS         33      /* 帧率控制周期(ms)，约30FPS，降低OLED刷新功耗 */
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

/* ================================================================
 *  FreeRTOS Task_UI 全局变量与接口
 * ================================================================ */

/** @brief 当前显示页面（Task_UI 状态机核心） */
extern volatile PageID_t g_CurrentPage;

/** @brief Task_UI 任务句柄（供 Task_Input 发送按键通知） */
extern TaskHandle_t Task_UI_Handle;

/** @brief Task_Sensor 任务句柄（供 Task_UI 发送传感器启停通知） */
extern TaskHandle_t Task_Sensor_Handle;

/** @brief 传感器活跃标志（Task_UI 写，Task_Sensor 读，原子操作无需锁） */
extern volatile uint8_t g_SensorActive;

/** @brief Task_Sensor 通知命令值 */
#define SENSOR_CMD_START  1    /* 启动传感器采样 */
#define SENSOR_CMD_STOP   2    /* 停止传感器采样 */

/**
  * @brief Task_UI — 统一页面渲染任务
  * @note  优先级 2，栈 1280 bytes，独占 OLED I2C 总线
  *        通过 xTaskNotifyWait() 等待按键（33ms 超时 = 30FPS）
  */
void Task_UI(void *pvParameters);

/**
  * @brief Task_UI 每帧渲染入口（由 freertos.c 的 Task_UI 主循环调用）
  * @param key: 当前按键值（0=无按键/超时唤醒）
  */
void Task_UI_RenderFrame(uint8_t key);

/* ================================================================
 *  底层 UI 绘制函数（供 Task_UI 内 Render_* 调用）
 * ================================================================ */

void Battery_Show_UI(void);
void Show_Clock_UI(void);
void Show_Setting_UI(void);
void Menu_Animation(void);
void Show_StopClock_UI(void);
void StopClock_Tick(void);
void Show_flashlight_UI(void);
void MPU6050_Calculation_Euler_angles(void);
void Show_MPU6050_UI(void);
void Show_emoji_UI(void);
void Show_Gradienter_UI(void);

#endif
