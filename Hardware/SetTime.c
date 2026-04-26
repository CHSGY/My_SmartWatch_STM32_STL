#include "stm32f10x.h"                  // Device header
#include "OLED.h"
#include "Key.h"
#include "MyRTC.h"


/***************************日期时间设置功能***********************************/

/**
  * @brief 显示日期设置UI
  * @param  无
  * @retval 无
  */
void Show_SetDate_UI(void)
{
	OLED_ShowImage(0,0,16,16,GoBack);
	OLED_Printf(0,16,OLED_8X16,"年:%4d",MyRTC_Time[0]);		//显示年
	OLED_Printf(0,32,OLED_8X16,"月:%2d",MyRTC_Time[1]);		//显示月
	OLED_Printf(0,48,OLED_8X16,"日:%2d",MyRTC_Time[2]);      //显示日
}

/**
  * @brief 显示时间设置UI
  * @param  无
  * @retval 无
  */
void Show_SetTime_UI(void)
{
	OLED_Printf(0,0,OLED_8X16,"时:%2d",MyRTC_Time[3]);		//显示时
	OLED_Printf(0,16,OLED_8X16,"分:%2d",MyRTC_Time[4]);		//显示分
	OLED_Printf(0,32,OLED_8X16,"秒:%2d",MyRTC_Time[5]);       //显示秒

}

/**
  * @brief 修改RTC时间值
  * @param  i    时间数组索引：0-年 1-月 2-日 3-时 4-分 5-秒
  * @param  flag 修改方向：0-减1 1-加1
  * @retval 无
  */
void ChangeRTC_Time(uint8_t i,uint8_t flag)
{
	if(flag == 0)
	{
		MyRTC_Time[i]--;
	}
	else //if(flag == 1)
	{
		MyRTC_Time[i]++;
	}
	MyRTC_SetTime(); 				//把更新后的时间值从结构体数组中刷新到RTC秒定时器硬件电路中
}

static uint8_t KeyNum;             /* 临时按键键码变量 */

/**
  * @brief 设置年份
  * @retval 0-返回
  * @note  Key1增加年份 Key2减少年份 Key3确认并返回
  */
int Set_Year(void)
{
	while(1)
	{
		KeyNum = Key_GetNum();
		if(KeyNum == 1) //Key1：add Year 
		{
			ChangeRTC_Time(0,1);
			KeyNum = 0;
		}

		else if(KeyNum == 2) //Key2: minus Year
		{
			ChangeRTC_Time(0,0);
			KeyNum = 0;
		}

		else if(KeyNum == 3) //Key3: save and quit
		{
			return 0;
		}

		Show_SetDate_UI();
		OLED_ReverseArea(24,16,32,16);
		OLED_Update();
	}
}

/**
  * @brief 设置月份
  * @retval 0-返回
  * @note  Key1增加月份 Key2减少月份 Key3确认并返回
  */
int Set_Month(void)
{
	while(1)
	{
		KeyNum = Key_GetNum();
		if(KeyNum == 1)         //Key1：add Month 
		{
			ChangeRTC_Time(1,1);
			if(MyRTC_Time[1] > 12)
			{
				MyRTC_Time[1] = 1;
				MyRTC_SetTime();
			}
		}

		else if(KeyNum == 2) //Key2: minus Month
		{
			ChangeRTC_Time(1,0);
			if(MyRTC_Time[1] <= 0)
			{
				MyRTC_Time[1] = 12;
				MyRTC_SetTime();
			}
		}

		else if(KeyNum == 3) //Key3: save and quit
		{
			return 0;
		}

		Show_SetDate_UI();
		OLED_ReverseArea(24,32,16,16);
		OLED_Update();

	}
}

/**
  * @brief 设置日期
  * @retval 0-返回
  * @note  Key1增加日期 Key2减少日期 Key3确认并返回
  */
int Set_Day(void)
{
	while(1)
	{
		KeyNum = Key_GetNum();
		if(KeyNum == 1)         //Key1：add Day
		{
			ChangeRTC_Time(2,1);
			if(MyRTC_Time[2] >= 32)
			{
				MyRTC_Time[2] = 1;
				MyRTC_SetTime();
			}
		}

		else if(KeyNum == 2)   //Key2: minus Day
		{
			ChangeRTC_Time(2,0);
			if(MyRTC_Time[2] <= 0)
			{
				MyRTC_Time[2] = 31;
				MyRTC_SetTime();
			}
		}

		else if(KeyNum == 3) //Key3: save and quit
		{
			return 0;
		}

		Show_SetDate_UI();
		OLED_ReverseArea(24,48,16,16);
		OLED_Update();
	}
}

/**
  * @brief 设置小时
  * @retval 0-返回
  * @note  Key1增加小时 Key2减少小时 Key3确认并返回
  */
int Set_Hour(void)
{
	while(1)
	{
		KeyNum = Key_GetNum();
		if(KeyNum == 1)     //Key1：add Hour 
		{
            ChangeRTC_Time(3,1);
            if(MyRTC_Time[3] >= 25)
            {
                MyRTC_Time[3] = 0;
				MyRTC_SetTime();
            }
		}
		else if(KeyNum == 2)    //Key2: minus Hour
		{
			ChangeRTC_Time(3,0);
			if(MyRTC_Time[3] < 0)
			{
				MyRTC_Time[3] = 24;
				MyRTC_SetTime();
			}
		}
		else if(KeyNum == 3) //Key3: save and quit
		{
			return 0;
		}
		
		/*无按键操作时，显示时间设置页面并高亮对应设置项*/
		Show_SetTime_UI();
		OLED_ReverseArea(24,0,16,16);
		OLED_Update();

    }

}

/**
  * @brief 设置分钟
  * @retval 0-返回
  * @note  Key1增加分钟 Key2减少分钟 Key3确认并返回
  */
int Set_Min(void)
{
	while(1)
	{
		KeyNum = Key_GetNum();
		if(KeyNum == 1)     //Key1：add Min 
		{
            ChangeRTC_Time(5,1);
            if(MyRTC_Time[5] >= 60)
            {
                MyRTC_Time[5] = 0;
				MyRTC_SetTime();
            }
		}
		
		else if(KeyNum == 2)    //Key2: minus Min
		{
			ChangeRTC_Time(5,0);
			if(MyRTC_Time[5] < 0)
			{
				MyRTC_Time[5] = 59;
				MyRTC_SetTime();
			}
		}
		
		else if(KeyNum == 3) //Key3: save and quit
		{
			return 0;
		}

		Show_SetTime_UI();
		OLED_ReverseArea(24,32,16,16);
		OLED_Update();

	}
}

/**
  * @brief 设置秒钟
  * @retval 0-返回
  * @note  Key1增加秒钟 Key2减少秒钟 Key3确认并返回
  */
int Set_Sec(void)
{
	while(1)
	{
		KeyNum = Key_GetNum();
		if(KeyNum == 1)         //Key1：add Sec 
		{
            ChangeRTC_Time(5,1);
            if(MyRTC_Time[5] >= 60)
            {
                MyRTC_Time[5] = 0;
				MyRTC_SetTime();
            }
		}
		
		else if(KeyNum == 2)        //Key2: minus Sec
		{
			ChangeRTC_Time(5,0);
			if(MyRTC_Time[5] < 0)
			{
				MyRTC_Time[5] = 59;
				MyRTC_SetTime();
			}
		}
		
		else if(KeyNum == 3) //Key3: save and quit
		{
			return 0;
		}

		Show_SetTime_UI();
		OLED_ReverseArea(24,32,16,16);
		OLED_Update();

    }
}


uint8_t Key_CursorFlag = 1;         /* 日期时间设置页面光标位置：1-返回 2-年 3-月 4-日 5-时 6-分 7-秒 */

/**
  * @brief 日期时间设置主流程
  * @retval 0-返回
  * @note  负责日期时间设置的页面跳转和功能选择
  */
int SetTime_mainprocess(void)
{
	while(1)
	{
		KeyNum = Key_GetNum();
		uint8_t Key_FuncFlag = 0;
		if(KeyNum == 1)       //光标递增
		{
			Key_CursorFlag++;
			if(Key_CursorFlag > 7)
			{
				Key_CursorFlag = 1;
			}
		}
		else if(KeyNum == 2)  //光标递减
		{
			Key_CursorFlag--;
			if(Key_CursorFlag <= 0)
			{
				Key_CursorFlag = 7;
			}
		}
		else if(KeyNum == 3)  //确定
		{
			//OLED_Clear();
			//OLED_Update();
			Key_FuncFlag = Key_CursorFlag;
		}
		
		/*******按键时间日期调整功能触发*******/
		if(Key_FuncFlag == 1)
		{
			Key_FuncFlag = 0;
			return 0;
		}
		else if(Key_FuncFlag == 2){Set_Year();}
		else if(Key_FuncFlag == 3){Set_Month();}
		else if(Key_FuncFlag == 4){Set_Day();}
		else if(Key_FuncFlag == 5){Set_Hour();}
		else if(Key_FuncFlag == 6){Set_Min();}
		else if(Key_FuncFlag == 7){Set_Sec();}
		else{;}
		
		/***********按键选择光标位置*********/
		switch(Key_CursorFlag)
		{
			case 1:                     //[返回]
				OLED_Clear();
				Show_SetDate_UI();
				OLED_ReverseArea(0,0,16,16);
				OLED_Update();
			    break;
            case 2:                     //年
                OLED_Clear();
				Show_SetDate_UI();
				OLED_ReverseArea(24,16,32,16);
				OLED_Update();
			    break;
            case 3:                     //月
                OLED_Clear();
				Show_SetDate_UI();
				OLED_ReverseArea(24,32,16,16);
				OLED_Update();
			    break;
            case 4:                     //日
                OLED_Clear();
				Show_SetDate_UI();
				OLED_ReverseArea(24,48,16,16);
				OLED_Update();
			    break;
            case 5:                     //时
                OLED_Clear();
				Show_SetTime_UI();
				OLED_ReverseArea(24,0,16,16);
				OLED_Update();
			    break;
            case 6:                     //分
                OLED_Clear();
				Show_SetTime_UI();
				OLED_ReverseArea(24,16,16,16);
				OLED_Update();
			    break;
            case 7:                     //秒
                OLED_Clear();
				Show_SetTime_UI();
				OLED_ReverseArea(24,32,16,16);
				OLED_Update(); 
			    break;
            default:
                break;
		}
	}
}

