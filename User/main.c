#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "menu.h"
#include "Timer.h"
#include "Key.h"
#include "dino.h"

/**
  * 坐标轴定义：
  * 左上角为(0, 0)点
  * 横向向右为X轴，取值范围：0~127
  * 纵向向下为Y轴，取值范围：0~63
  * 
  *       0             X轴           127 
  *      .------------------------------->
  *    0 |
  *      |
  *      |
  *      |
  *  Y轴 |
  *      |
  *      |
  *      |
  *   63 |
  *      v
  * 
  */

extern uint8_t Clockmoveflag;
extern volatile uint8_t Key_Num;
extern uint8_t ClockUI_Move_Flag; 
extern uint8_t start_timing_flag;

uint8_t ClkUIPage_Flag = 1;
volatile uint8_t KeyTimeFlag;               /* 按键定时标志，每20ms触发一次按键状态检测 */
volatile uint8_t Pre_KeyState;              /* 前一次按键状态 */
volatile uint8_t Cur_KeyState;              /* 当前按键状态 */

/**
  * @brief  TIM2 中断服务函数
  * @note   每 1ms 进入一次，执行所有 tick 任务
  *         对于本项目，中断中的任务都是轻量级计数器操作，
  *         在中断中执行可确保实时性，避免主循环耗时操作影响
  */
void TIM2_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
	{
		Key3_Tick();      // 按键3长按检测
		KeyTick();        // 按键消抖扫描
		
		if(start_timing_flag == 1)
		{
			StopClock_Tick();  // 秒表计时
		}
		
		dino_tick();      // 游戏物理更新
		
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}


int main(void)
{
	OLED_Init();
	Peripheral_Init();
	Timer_Init();
	
	OLED_Clear();
	Show_Clock_UI();
	OLED_Update();
	
	while (1)
	{
		OLED_Clear();
		Battery_Show_UI();
		OLED_Update();
		ClockUI_Move_Flag = First_Page_Clock();
		if(ClockUI_Move_Flag == 1)        //[菜单] 选项被选中
		{
			Menu_Page();                      //进入菜单页面
		}
		else if(ClockUI_Move_Flag == 2)   //[设置] 选项被选中
		{
			SettingPage();                    //进入设置页面
		}
		else
		{
			; 
		}
	}
}
