/**
  * @file           : menu.c
  * @brief          : 主菜单及功能模块（HAL库版本 / FreeRTOS Task_UI 状态机）
  * @author         : CHSGY
  * @date           : 2026-06-07
  *
  * @note           : Phase 3 改造：9 个 while(1) 独占页面函数改为 Task_UI 状态机。
  *                   - Render_*() 函数：每帧绘制（由 Task_UI 调用，不包含 OLED_Clear/Update）
  *                   - UI_ProcessKey()：按键→状态转换（由 Task_UI 调用）
  *                   - Task_UI 主循环在 freertos.c 中
  */

#include "main.h"
#include "MyRTC.h"
#include "Hardware/AD.h"
#include "Hardware/OLED.h"
#include "Hardware/Key.h"
#include "Hardware/LED.h"
#include "delay.h"
#include "Hardware/MPU6050.h"
#include "Hardware/menu.h"
#include "Hardware/dino.h"
#include "Hardware/SetTime.h"
#include "power.h"
#include "timers.h"
#include <math.h>

/********************全局变量************************/

/* Task_UI 状态机全局变量 */
volatile PageID_t g_CurrentPage = PAGE_CLOCK;
TaskHandle_t Task_UI_Handle = NULL;

/*************************电池显示**************************/

/* 电池显示缓存，避免每帧重复ADC采样 */
static uint16_t cached_AD_Value = 0;
static uint8_t Battery_Refresh_Cnt = 0;

/*
* @brief 电池电量显示UI
* @note  优化：每50帧采样一次ADC（16次取均值），其余帧使用缓存值
*        原每帧3000次采样耗时约20ms，优化后约1ms
*/
void Battery_Show_UI(void)
{
	uint16_t AD_Value;
	int8_t Battery_Capacity = 0;

	Battery_Refresh_Cnt++;
	if (Battery_Refresh_Cnt < BATTERY_REFRESH_FRAMES) {
		/* 未到采样周期，使用缓存值直接显示 */
		AD_Value = cached_AD_Value;
	} else {
		/* 每50帧采样一次，取16次均值 */
		Battery_Refresh_Cnt = 0;
		uint32_t sum = 0;
		for (int i = 0; i < BATTERY_ADC_SAMPLES; i++) {
			sum += AD_GetValue();
		}
		cached_AD_Value = sum / BATTERY_ADC_SAMPLES;
		AD_Value = cached_AD_Value;
	}

	Battery_Capacity = (AD_Value - BATTERY_ADC_EMPTY) * 100 / (BATTERY_ADC_MAX - BATTERY_ADC_EMPTY);

	if(Battery_Capacity < 0)
	{
		Battery_Capacity = 0;
	}

	if(Battery_Capacity >= 100)
	{
		Battery_Capacity = 100;
	}

	OLED_ShowNum(82,4,Battery_Capacity,3,OLED_6X8);
	OLED_ShowChar(100,4,'%',OLED_6X8);

	if(Battery_Capacity >= 100)
	{
		OLED_ShowImage(BATTERY_ICON_X, BATTERY_ICON_Y, BATTERY_ICON_W, BATTERY_ICON_H, Battery);
	}
	else if(Battery_Capacity >= 10)
	{
		OLED_ShowImage(BATTERY_ICON_X, BATTERY_ICON_Y, BATTERY_ICON_W, BATTERY_ICON_H, Battery);
		OLED_ClearArea(BATTERY_BAR_X + Battery_Capacity/10, BATTERY_BAR_Y, BATTERY_BAR_W - Battery_Capacity/10, BATTERY_BAR_H);
		OLED_ClearArea(82,4,6,8);
	}
	else	//个位数时清空电量条
	{
		OLED_ShowImage(BATTERY_ICON_X, BATTERY_ICON_Y, BATTERY_ICON_W, BATTERY_ICON_H, Battery);
		OLED_ClearArea(BATTERY_BAR_X, BATTERY_BAR_Y, BATTERY_BAR_W, BATTERY_BAR_H);
		OLED_ClearArea(82,4,12,8);
	}
}


/*************************首页时钟 UI 绘制**************************/

void Show_Clock_UI(void)
{
	MyRTC_ReadTime();
	OLED_Printf(CLOCK_DATE_X, CLOCK_DATE_Y, OLED_6X8, "%d-%d-%d", MyRTC_Time[0], MyRTC_Time[1], MyRTC_Time[2]);
	OLED_Printf(CLOCK_TIME_X, CLOCK_TIME_Y, OLED_12X24, "%02d:%02d:%02d", MyRTC_Time[3], MyRTC_Time[4], MyRTC_Time[5]);
	OLED_ShowString(CLOCK_MENU_TEXT_X, CLOCK_MENU_TEXT_Y, "Menu", OLED_8X16);
	OLED_ShowString(CLOCK_SET_TEXT_X, CLOCK_SET_TEXT_Y, "Set", OLED_8X16);
	Battery_Show_UI();
}


/*************************设置页面 UI 绘制**************************/

void Show_Setting_UI(void)
{
	OLED_ShowImage(0,0,16,16,GoBack);
	OLED_ShowString(0,16,"Set DateTime",OLED_8X16);
	OLED_ShowString(0,32,"Debug",OLED_8X16);
}


/********************************菜单页面 动画*******************************/

uint8_t MenuFlag = 2;
uint8_t Pre_item, Target_item;
uint8_t Pre_x;
uint8_t move_step = MENU_SLIDE_STEP;
uint8_t move_stateFlag = 1;
static uint8_t Direct_Flag = 2;     /* 图标移动方向：1=上一个 2=下一个 */

/*
*  菜单图标滑动动画（仅绘制，不做 Clear/Update — 由 Task_UI 统一处理）
*/
void Menu_Animation(void)
{
	/* 菜单向右滑动 */
	if(Pre_item < Target_item)
	{
		Pre_x -= move_step;
		if(Pre_x == 0)
		{
			Pre_item++;
			Pre_x = MENU_ICON_BASE_X;
			move_stateFlag = 0;
		}
	}

	/* 菜单向左滑动 */
	if(Pre_item > Target_item)
	{
		Pre_x += move_step;
		if(Pre_x == MENU_ICON_BASE_X * 2)
		{
			Pre_item--;
			Pre_x = MENU_ICON_BASE_X;
			move_stateFlag = 0;
		}
	}

	/* 绘制选择框 */
	OLED_ShowImage(MENU_FRAME_X, MENU_FRAME_Y, MENU_FRAME_W, MENU_FRAME_H, Frame);

	if(Pre_item >= 1)
	{
		OLED_ShowImage(Pre_x - MENU_ICON_SPACING, MENU_ICON_Y, MENU_ICON_SIZE, MENU_ICON_SIZE, Menu_Graph[Pre_item-1]);
	}
	if(Pre_item >= 2)
	{
		OLED_ShowImage(Pre_x - MENU_ICON_SPACING * 2, MENU_ICON_Y, MENU_ICON_SIZE, MENU_ICON_SIZE, Menu_Graph[Pre_item-2]);
	}

	OLED_ShowImage(Pre_x, MENU_ICON_Y, MENU_ICON_SIZE, MENU_ICON_SIZE, Menu_Graph[Pre_item]);
	OLED_ShowImage(Pre_x + MENU_ICON_SPACING, MENU_ICON_Y, MENU_ICON_SIZE, MENU_ICON_SIZE, Menu_Graph[Pre_item+1]);
	OLED_ShowImage(Pre_x + MENU_ICON_SPACING * 2, MENU_ICON_Y, MENU_ICON_SIZE, MENU_ICON_SIZE, Menu_Graph[Pre_item+2]);
}

/*
*  菜单进场动画（确认选择后播放，阻塞式6帧动画）
*/
void MenuToFunction_Animation(void)
{
	for(uint8_t i=0; i<=MENU_ENTER_FRAMES; i++)
	{
		OLED_ClearArea(0, MENU_ICON_Y, 128, 48);
		if(Pre_item >= 1)
		{
			OLED_ShowImage(Pre_x - MENU_ICON_SPACING, MENU_ICON_Y + i * MENU_ENTER_STEP, MENU_ICON_SIZE, MENU_ICON_SIZE, Menu_Graph[Pre_item-1]);
			OLED_ShowImage(Pre_x, MENU_ICON_Y + i * MENU_ENTER_STEP, MENU_ICON_SIZE, MENU_ICON_SIZE, Menu_Graph[Pre_item]);
			OLED_ShowImage(Pre_x + MENU_ICON_SPACING, MENU_ICON_Y + i * MENU_ENTER_STEP, MENU_ICON_SIZE, MENU_ICON_SIZE, Menu_Graph[Pre_item+1]);
		}
		OLED_UpdateArea(0, MENU_ICON_Y, 128, 48);
	}
}

/*
*	设置菜单选择（滑动动画入口）
*/
void Set_Selection(uint8_t move_flag, uint8_t pre_item, uint8_t target_item)
{
	if(move_flag == 1)
	{
		Pre_item = pre_item;
		Target_item = target_item;
	}
	Menu_Animation();
}


/********************************秒表************************************/

uint8_t StopClock_Flag = 1;
uint8_t hour,min,sec;
uint8_t start_timing_flag = 0;

void Show_StopClock_UI(void)
{
	OLED_ShowImage(0,0,16,16,GoBack);
	OLED_Printf(STOPCLK_TIME_X, STOPCLK_TIME_Y, OLED_8X16, "%02d:%02d:%02d", hour, min, sec);
	OLED_ShowString(STOPCLK_BTN_START_X, STOPCLK_BTN_Y, "Start", OLED_8X16);
	OLED_ShowString(STOPCLK_BTN_STOP_X, STOPCLK_BTN_Y, "Stop", OLED_8X16);
	OLED_ShowString(STOPCLK_BTN_CLEAR_X, STOPCLK_BTN_Y, "Clear", OLED_8X16);
}

void StopClock_Tick(void)
{
	static uint16_t Timer_count;
	Timer_count++;
	if(Timer_count >= STOPCLK_1S_TICKS)
	{
		Timer_count = 0;
		if(start_timing_flag == 1)
		{
			sec++;
			if(sec >= STOPCLK_SEC_MAX)
			{
				sec = 0;
				min++;
				if(min >= STOPCLK_MIN_MAX)
				{
					min = 0;
					hour++;
					if(hour >= STOPCLK_HOUR_MAX)
					{
						hour = 0;
					}
				}
			}
		}
	}
}

void ClkCount_Start(void)
{
	start_timing_flag = 1;
}

void ClkCount_Stop(void)
{
	start_timing_flag = 0;
}

void ClkCount_Clear(void)
{
	start_timing_flag = 0;
	hour = min = sec = 0;
}


/********************************手电筒************************************/

uint8_t flashlight_Flag = 1;

void Show_flashlight_UI(void)
{
	OLED_ShowImage(0,0,16,16,GoBack);
	OLED_ShowString(20,24,"OFF",OLED_8X16);
	OLED_ShowString(84,24,"ON",OLED_8X16);
}

void flashlight_ON(void)
{
	LED1_ON();
}

void flashlight_OFF(void)
{
	LED1_OFF();
}


/********************************MPU6050************************************/

float delta = MPU_SAMPLE_PERIOD_S;
float a = MPU_FILTER_ALPHA;
int16_t ax,ay,az;
int16_t gx,gy,gz;
float roll_a,pitch_a;
float roll_g,pitch_g,yaw_g;
volatile float g_Roll=0.0, g_Pitch=0.0, g_Yaw=0.0;
double Pi = 3.1415927;
TaskHandle_t Task_Sensor_Handle = NULL;
volatile uint8_t g_SensorActive = 0;

void MPU6050_Calculation_Euler_angles(void)
{
	MPU6050_GetData(&ax,&ay,&az,&gx,&gy,&gz);

	roll_g = g_Roll + (float)gx*delta;
	pitch_g = g_Pitch + (float)gy*delta;
	yaw_g = g_Yaw + (float)gz*delta;

	roll_a = atan2(ay,az)*180/Pi;
	pitch_a = atan2((-1)*ax,az)*180/Pi;

	taskENTER_CRITICAL();
	g_Roll =  a*roll_g + (1-a)*roll_a;
	g_Pitch = a*pitch_g +(1-a)*pitch_a;
	g_Yaw = a*yaw_g;
	taskEXIT_CRITICAL();
}

void Show_MPU6050_UI(void)
{
	OLED_ShowImage(0,0,16,16,GoBack);
	OLED_Printf(0,16,OLED_8X16,"Roll:  %.2f",g_Roll);
	OLED_Printf(0,32,OLED_8X16,"Pitch: %.2f",g_Pitch);
	OLED_Printf(0,48,OLED_8X16,"Yaw:   %.2f",g_Yaw);
}


/*********************************游戏选择页 UI*******************************/

void Show_Game_UI(void)
{
	OLED_ShowImage(0,0,16,16,GoBack);
	OLED_ShowString(0,16,"Google DinoGame",OLED_8X16);
}

uint8_t game_flag = 1;


/*********************************动态表情********************************************/

static uint8_t emoji_phase = 0;   /* 0=闭眼 1=睁眼 2=等待间隔 */
static uint8_t emoji_frame = 0;   /* 当前子帧计数器 */

/*
* @brief 绘制单帧表情（辅助函数）
*/
static void DrawEmojiFrame(int8_t eyebrow_offset, int8_t eye_ry)
{
	OLED_ShowImage(EMOJI_L_EYEBROW_X, EMOJI_EYEBROW_Y + eyebrow_offset, 16, 16, eyebrow[0]);
	OLED_ShowImage(EMOJI_R_EYEBROW_X, EMOJI_EYEBROW_Y + eyebrow_offset, 16, 16, eyebrow[1]);
	OLED_DrawEllipse(EMOJI_L_EYE_CX, EMOJI_EYE_CY, EMOJI_EYE_RX, eye_ry, 1);
	OLED_DrawEllipse(EMOJI_R_EYE_CX, EMOJI_EYE_CY, EMOJI_EYE_RX, eye_ry, 1);
	OLED_ShowImage(EMOJI_MOUTH_X, EMOJI_MOUTH_Y, EMOJI_MOUTH_W, EMOJI_MOUTH_H, mouth);
}

/* 保留原 Show_emoji_UI 供参考，但 Render_Emoji 已替代其功能 */
void Show_emoji_UI(void)
{
	/* 由 Render_Emoji 状态机替代，此函数保留以兼容旧接口 */
}


/*********************************水平仪********************************************/

void Show_Gradienter_UI(void)
{
	int16_t x = GRADIENTER_CENTER_X - g_Roll;
	int16_t y = GRADIENTER_CENTER_Y + g_Pitch;
	OLED_DrawCircle(GRADIENTER_CENTER_X, GRADIENTER_CENTER_Y, GRADIENTER_OUTER_R, OLED_UNFILLED);
	int16_t dx = x - GRADIENTER_CENTER_X;
	int16_t dy = y - GRADIENTER_CENTER_Y;
	float distance = sqrtf(dx*dx + dy*dy);
	if(distance > GRADIENTER_BOUNDARY_R)
	{
		float scale = GRADIENTER_BOUNDARY_R / distance;
		x = GRADIENTER_CENTER_X + (dx * scale);
		y = GRADIENTER_CENTER_Y + (dy * scale);
	}
	OLED_DrawCircle(x, y, GRADIENTER_INNER_R, OLED_FILLED);
	/* OLED_Update 由 Task_UI 统一调用 */
}


/*************************SetTime 状态变量**************************/

static uint8_t Key_CursorFlag = 1;              /* 主菜单光标：1=返回 2=年 3=月 4=日 5=时 6=分 7=秒 */
static SetTimeState_t settime_state = SETTIME_MENU;


/*************************页面子状态变量**************************/

static uint8_t Clockmoveflag = 1;     /* 首页时钟光标：1=菜单 2=设置 */
static uint8_t SettingFlag = 1;       /* 设置页光标：1=返回 2=设置时间 */


/* ================================================================
 *   Render_* 函数 — 每帧绘制（由 Task_UI 调用）
 *   不包含 OLED_Clear() / OLED_Update()，由 Task_UI 统一处理
 * ================================================================ */

/*
* @brief 首页时钟渲染
*/
static void Render_Clock(void)
{
	Show_Clock_UI();
	switch(Clockmoveflag)
	{
		case 1: OLED_ReverseArea(0,48,32,16);   break;  /* [菜单] */
		case 2: OLED_ReverseArea(96,48,32,16);  break;  /* [设置] */
		default: break;
	}
}

/*
* @brief 设置页面渲染
*/
static void Render_Setting(void)
{
	Show_Setting_UI();
	switch(SettingFlag)
	{
		case 1: OLED_ReverseArea(0,0,16,16);    break;  /* [返回] */
		case 2: OLED_ReverseArea(0,16,96,16);   break;  /* [设置时间日期] */
		case 3: OLED_ReverseArea(0,32,40,16);   break;  /* [Debug] */
		default: break;
	}
}

/*
* @brief 菜单页面渲染
*/
static void Render_Menu(void)
{
	if(MenuFlag == 1)
	{
		/* 位置1是[返回]，无滑动动画 */
		OLED_ShowImage(MENU_FRAME_X, MENU_FRAME_Y, MENU_FRAME_W, MENU_FRAME_H, Frame);
		OLED_ShowImage(MENU_ICON_BASE_X, MENU_ICON_Y, MENU_ICON_SIZE, MENU_ICON_SIZE, Menu_Graph[0]);
	}
	else
	{
		if(move_stateFlag == 1)
		{
			if(Direct_Flag == 1)
			{
				Set_Selection(1, MenuFlag, MenuFlag-1);
			}
			else if(Direct_Flag == 2)
			{
				Set_Selection(1, MenuFlag-2, MenuFlag-1);
			}
			else
			{
				Menu_Animation();
			}
		}
		else
		{
			Menu_Animation();
		}
	}
}

/*
* @brief 秒表页面渲染
*/
static void Render_Stopwatch(void)
{
	Show_StopClock_UI();
	switch(StopClock_Flag)
	{
		case 1: OLED_ReverseArea(0, 0, 16, 16); break;
		case 2: OLED_ReverseArea(STOPCLK_BTN_START_X, STOPCLK_BTN_Y, STOPCLK_BTN_START_W, STOPCLK_BTN_H); break;
		case 3: OLED_ReverseArea(STOPCLK_BTN_STOP_X, STOPCLK_BTN_Y, STOPCLK_BTN_STOP_W, STOPCLK_BTN_H); break;
		case 4: OLED_ReverseArea(STOPCLK_BTN_CLEAR_X, STOPCLK_BTN_Y, STOPCLK_BTN_CLEAR_W, STOPCLK_BTN_H); break;
		default: break;
	}
}

/*
* @brief 手电筒页面渲染
*/
static void Render_Flashlight(void)
{
	Show_flashlight_UI();
	switch(flashlight_Flag)
	{
		case 1: OLED_ReverseArea(0,0,16,16);    break;
		case 2: OLED_ReverseArea(20,24,24,16);  break;
		case 3: OLED_ReverseArea(84,24,16,16);  break;
		default: break;
	}
}

/*
* @brief MPU6050 传感器页面渲染
*/
static void Render_MPU6050(void)
{
	Show_MPU6050_UI();
	OLED_ReverseArea(0,0,16,16);
}

/*
* @brief 游戏选择页面渲染
*/
static void Render_GameSelect(void)
{
	Show_Game_UI();
	switch(game_flag)
	{
		case 1: OLED_ReverseArea(0,0,16,16);     break;
		case 2: OLED_ReverseArea(0,16,120,16);   break;
		default: break;
	}
}

/*
* @brief 恐龙游戏页面渲染（每帧调用 dino.c 的单帧函数）
*/
static void Render_DinoGame(void)
{
	if(Dino_RenderFrame())
	{
		/* 游戏结束，返回游戏选择页 */
		g_CurrentPage = PAGE_GAME_SELECT;
	}
}

/*
* @brief 表情动画页面渲染（状态机推进）
*/
static void Render_Emoji(void)
{
	int8_t i;
	switch(emoji_phase)
	{
		case 0: /* 闭眼阶段：每33ms推进1帧 */
			i = emoji_frame;
			DrawEmojiFrame(i, EMOJI_EYE_RY_MAX - i);
			if(++emoji_frame > EMOJI_BLINK_FRAMES)
			{
				emoji_frame = 0;
				emoji_phase = 1;
			}
			break;
		case 1: /* 睁眼阶段 */
			i = EMOJI_BLINK_FRAMES - emoji_frame;
			DrawEmojiFrame(i, EMOJI_EYE_RY_MAX - EMOJI_BLINK_FRAMES + emoji_frame);
			if(++emoji_frame > EMOJI_BLINK_FRAMES)
			{
				emoji_frame = 0;
				emoji_phase = 2;
			}
			break;
		case 2: /* 等待间隔 ~500ms/33ms ≈ 15帧 */
			DrawEmojiFrame(0, EMOJI_EYE_RY_MAX);
			if(++emoji_frame >= 15)
			{
				emoji_frame = 0;
				emoji_phase = 0;
			}
			break;
	}
}

/*
* @brief 水平仪页面渲染
*/
static void Render_Gradienter(void)
{
	Show_Gradienter_UI();
}

/*
* @brief 设置时间页面渲染
*/
static void Render_SetTime(void)
{
	switch(settime_state)
	{
		case SETTIME_MENU:
			/* SETTIME_MENU: 双列布局同时显示全部6个字段 + 返回图标 */
			OLED_ShowImage(0, 0, 16, 16, GoBack);

			/* 左列：日期 (y=16起，避开返回图标) */
			OLED_Printf(0, 16, OLED_8X16, "Y:%4d", MyRTC_Time[0]);
			OLED_Printf(0, 32, OLED_8X16, "M:%2d", MyRTC_Time[1]);
			OLED_Printf(0, 48, OLED_8X16, "D:%2d", MyRTC_Time[2]);

			/* 右列：时间 */
			OLED_Printf(64, 16, OLED_8X16, "H:%2d", MyRTC_Time[3]);
			OLED_Printf(64, 32, OLED_8X16, "M:%2d", MyRTC_Time[4]);
			OLED_Printf(64, 48, OLED_8X16, "S:%2d", MyRTC_Time[5]);

			switch(Key_CursorFlag)
			{
				case 1: OLED_ReverseArea(0, 0, 16, 16);      break; /* 返回 */
				case 2: OLED_ReverseArea(16, 16, 32, 16);    break; /* Year */
				case 3: OLED_ReverseArea(16, 32, 16, 16);    break; /* Mon */
				case 4: OLED_ReverseArea(16, 48, 16, 16);    break; /* Day */
				case 5: OLED_ReverseArea(80, 16, 16, 16);    break; /* Hour */
				case 6: OLED_ReverseArea(80, 32, 16, 16);    break; /* Min */
				case 7: OLED_ReverseArea(80, 48, 16, 16);    break; /* Sec */
			}
			break;

		case SETTIME_YEAR:  Show_SetDate_UI(); OLED_ReverseArea(40,16,32,16);  break;
		case SETTIME_MONTH: Show_SetDate_UI(); OLED_ReverseArea(32,32,16,16);  break;
		case SETTIME_DAY:   Show_SetDate_UI(); OLED_ReverseArea(32,48,16,16);  break;
		case SETTIME_HOUR:  Show_SetTime_UI(); OLED_ReverseArea(40,0,16,16);   break;
		case SETTIME_MIN:   Show_SetTime_UI(); OLED_ReverseArea(32,16,16,16);  break;
		case SETTIME_SEC:   Show_SetTime_UI(); OLED_ReverseArea(32,32,16,16);  break;
	}
}


/*
* @brief Debug 信息显示
* @note  使用 uxTaskGetStackHighWaterMark 读取各任务栈剩余水位(words)，
*        xPortGetFreeHeapSize 读取堆空闲量。供 Phase 5 栈调优使用。
*/
static void Show_Debug_UI(void)
{
	UBaseType_t inp_hwm, ui_hwm, sen_hwm, idle_hwm, tmr_hwm;
	size_t free_heap;

	inp_hwm = uxTaskGetStackHighWaterMark(Task_Input_Handle);
	ui_hwm  = uxTaskGetStackHighWaterMark(Task_UI_Handle);
	sen_hwm = uxTaskGetStackHighWaterMark(Task_Sensor_Handle);
	idle_hwm = uxTaskGetStackHighWaterMark(xTaskGetIdleTaskHandle());
	tmr_hwm = uxTaskGetStackHighWaterMark(xTimerGetTimerDaemonTaskHandle());

	free_heap = xPortGetFreeHeapSize();

	/* 渲染 GoBack 图标 */
	OLED_ShowImage(0, 0, 16, 16, GoBack);

	/* 按行显示各任务栈水位 + 堆空闲量 (6x8 字体) */
	OLED_Printf(20, 16, OLED_6X8, "Inp:%3u/ 96w", inp_hwm);
	OLED_Printf(20, 24, OLED_6X8, "UI:%4u/320w", ui_hwm);
	OLED_Printf(20, 32, OLED_6X8, "Sen:%3u/128w", sen_hwm);
	OLED_Printf(20, 40, OLED_6X8, "Idle:%3u/128w", idle_hwm);
	OLED_Printf(20, 48, OLED_6X8, "Tmr:%4u/256w", tmr_hwm);
	OLED_Printf(20, 56, OLED_6X8, "Heap:%5u/10240B", free_heap);
}

/*
* @brief Debug 页面渲染
*/
static void Render_Debug(void)
{
	Show_Debug_UI();
	OLED_ReverseArea(0, 0, 16, 16);  /* 反显 GoBack 图标表示选择中 */
}


/* ================================================================
 *   UI_ProcessKey — 按键→状态转换（由 Task_UI 调用）
 * ================================================================ */

static void UI_ProcessKey(uint8_t key)
{
	switch(g_CurrentPage)
	{
		case PAGE_CLOCK:
			if(key == 1)
			{
				if(--Clockmoveflag == 0) Clockmoveflag = 2;
			}
			else if(key == 2)
			{
				if(++Clockmoveflag == 3) Clockmoveflag = 1;
			}
			else if(key == 3)
			{
				if(Clockmoveflag == 1)
				{
					g_CurrentPage = PAGE_MENU;
					MenuFlag = 2;
					move_stateFlag = 1;
					Direct_Flag = 2;
					Pre_item = 1;
					Pre_x = MENU_ICON_BASE_X;
				}
				else
				{
					g_CurrentPage = PAGE_SETTING;
					SettingFlag = 1;
				}
			}
			break;

		case PAGE_SETTING:
			if(key == 1)
			{
				if(--SettingFlag == 0) SettingFlag = 3;
			}
			else if(key == 2)
			{
				if(++SettingFlag == 4) SettingFlag = 1;
			}
			else if(key == 3)
			{
				if(SettingFlag == 1)
				{
					g_CurrentPage = PAGE_CLOCK;
					Clockmoveflag = 1;
				}
				else if(SettingFlag == 2)
				{
					/* 进入设置时间状态机 */
					g_CurrentPage = PAGE_SETTIME;
					Key_CursorFlag = 1;
					settime_state = SETTIME_MENU;
				}
				else
				{
					/* 进入 Debug 页面 */
					g_CurrentPage = PAGE_DEBUG;
				}
			}
			break;

		case PAGE_MENU:
			if(key == 1)
			{
				Direct_Flag = 1;
				move_stateFlag = 1;
				if(--MenuFlag == 0) MenuFlag = MENU_CURSOR_MAX;
			}
			else if(key == 2)
			{
				Direct_Flag = 2;
				move_stateFlag = 1;
				if(++MenuFlag == MENU_CURSOR_MAX + 1) MenuFlag = MENU_CURSOR_MIN;
			}
			else if(key == 3)
			{
				Direct_Flag = 0;
				/* 播放进场动画后跳转 */
				MenuToFunction_Animation();
				switch(MenuFlag)
				{
					case 1: /* 返回 */
						g_CurrentPage = PAGE_CLOCK;
						Clockmoveflag = 1;
						break;
					case 2: /* 秒表 */
						g_CurrentPage = PAGE_STOPWATCH;
						StopClock_Flag = 1;
						break;
					case 3: /* 手电筒 */
						g_CurrentPage = PAGE_FLASHLIGHT;
						flashlight_Flag = 1;
						flashlight_OFF();
						break;
					case 4: /* MPU6050 */
						g_CurrentPage = PAGE_MPU6050;
						g_SensorActive = 1;
						if (Task_Sensor_Handle != NULL)
							xTaskNotify(Task_Sensor_Handle, SENSOR_CMD_START, eSetValueWithOverwrite);
						break;
					case 5: /* 游戏 */
						g_CurrentPage = PAGE_GAME_SELECT;
						game_flag = 1;
						break;
					case 6: /* 表情动画 */
						g_CurrentPage = PAGE_EMOJI;
						emoji_phase = 0;
						emoji_frame = 0;
						break;
					case 7: /* 水平仪 */
						g_CurrentPage = PAGE_GRADIENTER;
						g_SensorActive = 1;
						if (Task_Sensor_Handle != NULL)
							xTaskNotify(Task_Sensor_Handle, SENSOR_CMD_START, eSetValueWithOverwrite);
						break;
				}
			}
			break;

		case PAGE_STOPWATCH:
			if(key == 1)
			{
				if(--StopClock_Flag == 0) StopClock_Flag = 4;
			}
			else if(key == 2)
			{
				if(++StopClock_Flag == 5) StopClock_Flag = 1;
			}
			else if(key == 3)
			{
				switch(StopClock_Flag)
				{
					case 1: /* 返回 */
						g_CurrentPage = PAGE_MENU;
						MenuFlag = 2;
						move_stateFlag = 1;
						Direct_Flag = 2;
						Pre_item = 1;
						Pre_x = MENU_ICON_BASE_X;
						break;
					case 2: ClkCount_Start();  break; /* 开始 */
					case 3: ClkCount_Stop();   break; /* 停止 */
					case 4: ClkCount_Clear();  break; /* 清除 */
				}
			}
			break;

		case PAGE_FLASHLIGHT:
			if(key == 1)
			{
				if(--flashlight_Flag == 0) flashlight_Flag = 3;
			}
			else if(key == 2)
			{
				if(++flashlight_Flag == 4) flashlight_Flag = 1;
			}
			else if(key == 3)
			{
				switch(flashlight_Flag)
				{
					case 1: /* 返回 */
						flashlight_OFF();
						g_CurrentPage = PAGE_MENU;
						MenuFlag = 3;
						move_stateFlag = 1;
						Direct_Flag = 2;
						Pre_item = 2;
						Pre_x = MENU_ICON_BASE_X;
						break;
					case 2: flashlight_OFF(); break; /* OFF */
					case 3: flashlight_ON();  break; /* ON */
				}
			}
			break;

		case PAGE_MPU6050:
			if(key == 3)
			{
				g_SensorActive = 0;
				if (Task_Sensor_Handle != NULL)
					xTaskNotify(Task_Sensor_Handle, SENSOR_CMD_STOP, eSetValueWithOverwrite);
				g_CurrentPage = PAGE_MENU;
				MenuFlag = 4;
				move_stateFlag = 1;
				Direct_Flag = 2;
				Pre_item = 3;
				Pre_x = MENU_ICON_BASE_X; 
			}
			break;

		case PAGE_GAME_SELECT:
			if(key == 1)
			{
				if(--game_flag == 0) game_flag = 2;
			}
			else if(key == 2)
			{
				if(++game_flag == 3) game_flag = 1;
			}
			else if(key == 3)
			{
				if(game_flag == 1)
				{
					/* 返回菜单 */
					g_CurrentPage = PAGE_MENU;
					MenuFlag = 5;
					move_stateFlag = 1;
					Direct_Flag = 2;
					Pre_item = 4;
					Pre_x = MENU_ICON_BASE_X;
				}
				else
				{
					/* 进入恐龙游戏 */
					Game_Init();
					g_CurrentPage = PAGE_DINO_GAME;
				}
			}
			break;

		case PAGE_DINO_GAME:
			if(key == 1)
			{
				/* Key1: 恐龙跳跃 */
				Dino_JumpRequest = 1;
			}
			else if(key == 3)
			{
				/* Key3: 退出游戏 */
				g_CurrentPage = PAGE_GAME_SELECT;
				game_flag = 2;
			}
			break;

		case PAGE_EMOJI:
			if(key == 3)
			{
				g_CurrentPage = PAGE_MENU;
				MenuFlag = 6;
				move_stateFlag = 1;
				Direct_Flag = 2;
				Pre_item = 5;
				Pre_x = MENU_ICON_BASE_X;
			}
			break;

		case PAGE_GRADIENTER:
			if(key == 3)
			{
				g_SensorActive = 0;
				if (Task_Sensor_Handle != NULL)
					xTaskNotify(Task_Sensor_Handle, SENSOR_CMD_STOP, eSetValueWithOverwrite);
				g_CurrentPage = PAGE_MENU;
				MenuFlag = 7;
				move_stateFlag = 1;
				Direct_Flag = 2;
				Pre_item = 6;
				Pre_x = MENU_ICON_BASE_X;
			}
			break;

		case PAGE_SETTIME:
			if(settime_state == SETTIME_MENU)
			{
				/* 主菜单：移动光标 */
				if(key == 1)
				{
					if(--Key_CursorFlag == 0) Key_CursorFlag = 7;
				}
				else if(key == 2)
				{
					if(++Key_CursorFlag > 7) Key_CursorFlag = 1;
				}
				else if(key == 3)
				{
					switch(Key_CursorFlag)
					{
						case 1: /* 返回设置页 */
							g_CurrentPage = PAGE_SETTING;
							SettingFlag = 2;
							settime_state = SETTIME_MENU;
							Key_CursorFlag = 1;
							break;
						case 2: settime_state = SETTIME_YEAR;  break;
						case 3: settime_state = SETTIME_MONTH; break;
						case 4: settime_state = SETTIME_DAY;   break;
						case 5: settime_state = SETTIME_HOUR;  break;
						case 6: settime_state = SETTIME_MIN;   break;
						case 7: settime_state = SETTIME_SEC;   break;
					}
				}
			}
			else
			{
				/* 子状态：修改具体值 */
				int idx = (int)settime_state - 1; /* SETTIME_YEAR=1 -> idx=0 */
				if(key == 1)
				{
					/* Key1: +1 */
					ChangeRTC_Time(idx, 1);
					/* 边界回绕检查 */
					switch(settime_state)
					{
						case SETTIME_MONTH:
							if(MyRTC_Time[1] > 12) { MyRTC_Time[1] = 1; MyRTC_SetTime(); }
							break;
						case SETTIME_DAY:
							if(MyRTC_Time[2] >= 32) { MyRTC_Time[2] = 1; MyRTC_SetTime(); }
							break;
						case SETTIME_HOUR:
							if(MyRTC_Time[3] >= 24) { MyRTC_Time[3] = 0; MyRTC_SetTime(); }
							break;
						case SETTIME_MIN:
							if(MyRTC_Time[4] >= 60) { MyRTC_Time[4] = 0; MyRTC_SetTime(); }
							break;
						case SETTIME_SEC:
							if(MyRTC_Time[5] >= 60) { MyRTC_Time[5] = 0; MyRTC_SetTime(); }
							break;
						default: break;
					}
				}
				else if(key == 2)
				{
					/* Key2: -1 */
					ChangeRTC_Time(idx, 0);
					/* 边界回绕检查 */
					switch(settime_state)
					{
						case SETTIME_MONTH:
							if(MyRTC_Time[1] <= 0) { MyRTC_Time[1] = 12; MyRTC_SetTime(); }
							break;
						case SETTIME_DAY:
							if(MyRTC_Time[2] <= 0) { MyRTC_Time[2] = 31; MyRTC_SetTime(); }
							break;
						case SETTIME_HOUR:
							if(MyRTC_Time[3] < 0) { MyRTC_Time[3] = 23; MyRTC_SetTime(); }
							break;
						case SETTIME_MIN:
							if(MyRTC_Time[4] < 0) { MyRTC_Time[4] = 59; MyRTC_SetTime(); }
							break;
						case SETTIME_SEC:
							if(MyRTC_Time[5] < 0) { MyRTC_Time[5] = 59; MyRTC_SetTime(); }
							break;
						default: break;
					}
				}
				else if(key == 3)
				{
					/* Key3: 确认返回主菜单 */
					settime_state = SETTIME_MENU;
				}
			}
			break;

		case PAGE_DEBUG:
			if(key == 3)
			{
				g_CurrentPage = PAGE_SETTING;
				SettingFlag = 3;  /* 返回后保持 Debug 选项选中 */
			}
			break;

		default:
			break;
	}
}


/* ================================================================
 *   Task_UI — 统一页面渲染任务
 *   在 freertos.c 中实现主循环，调用此处的 Render + ProcessKey
 * ================================================================ */

/**
  * @brief  Task_UI 每帧渲染入口（由 freertos.c 的 Task_UI 主循环调用）
  * @param  key: 当前按键值（0=无按键/超时唤醒）
  * @retval 无
  */
void Task_UI_RenderFrame(uint8_t key)
{
	/* 有按键时处理状态转换 */
	if(key != 0)
	{
		UI_ProcessKey(key);
	}

	/* 渲染当前页面 */
	OLED_Clear();
	switch(g_CurrentPage)
	{
		case PAGE_CLOCK:       Render_Clock();       break;
		case PAGE_MENU:        Render_Menu();        break;
		case PAGE_SETTING:     Render_Setting();     break;
		case PAGE_STOPWATCH:   Render_Stopwatch();   break;
		case PAGE_FLASHLIGHT:  Render_Flashlight();  break;
		case PAGE_MPU6050:     Render_MPU6050();     break;
		case PAGE_GAME_SELECT: Render_GameSelect();  break;
		case PAGE_DINO_GAME:   Render_DinoGame();    break;
		case PAGE_EMOJI:       Render_Emoji();       break;
		case PAGE_GRADIENTER:  Render_Gradienter();  break;
		case PAGE_SETTIME:     Render_SetTime();     break;
		case PAGE_DEBUG:       Render_Debug();       break;
		default: break;
	}
	/* 挂起调度器 → 防止 I2C 位带传输被 Task_Input(prio3) 抢占导致帧时间抖动 */
	vTaskSuspendAll();
	OLED_Update();                              /* ~12ms 连续 I2C 传输，无任务切换 */
	xTaskResumeAll();                           /* 恢复调度器，执行待处理的 PendSV 切换 */
}
