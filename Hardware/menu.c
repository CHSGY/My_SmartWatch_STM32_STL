#include "stm32f10x.h"                  // Device header
#include "MyRTC.h"
#include "AD.h"
#include "OLED.h"
#include "key.h"
#include "SetTime.h"
#include "menu.h"
#include "LED.h"
#include "Delay.h"
#include "MPU6050.h"
#include "dino.h"
#include <math.h>

/*********************函数声明************************/
// void StopClock_Tick(void);
// int StopClock(void);

//外设初始化
void Peripheral_Init(void)
{
	MyRTC_Init(); 		//实时时钟初始化
	Key_Init();			//按键初始化
	LED_Init();			//LED初始化
	AD_Init();			//ADC初始化
	MPU6050_Init();
	
}


/*************************首页时钟界面**********************************/
/*
* @brief 电池电量显示UI
*/
void Battery_Show_UI(void)
{
	uint16_t AD_Value;
	//float VBat;
	int8_t Battery_Capacity = 0;
	int i=0;
	int sum=0;

	//均值滤波，取3000次采样平均值
	for(i=0;i<3000;i++)
	{
		AD_Value = AD_GetValue();
		sum += AD_Value;
	}
	AD_Value = sum/3000;
	//OLED_ShowNum(70,8,AD_Value,4,OLED_6X8);

	//VBat = (float)AD_Value/4095 * 3.3;
	//OLED_ShowFloatNum(80,0,VBat,1,1,OLED_6X8);
	Battery_Capacity = (AD_Value - ((2.64/3.3)*4092))*100 / (4092-3276);

	OLED_ShowNum(82,4,Battery_Capacity,3,OLED_6X8);
	OLED_ShowChar(100,4,'%',OLED_6X8);

	if(Battery_Capacity < 0)
	{
		Battery_Capacity = 0;
	}
	
	if(Battery_Capacity >= 100)
	{
		Battery_Capacity = 100;
		OLED_ShowNum(82,4,Battery_Capacity,3,OLED_6X8);
		OLED_ShowImage(110,0,16,16,Battery);
	}

	else if(Battery_Capacity >= 10 && Battery_Capacity < 100)
	{
		OLED_ShowImage(110,0,16,16,Battery);
		OLED_ClearArea(113+Battery_Capacity/10,5,10-Battery_Capacity/10,6);
		OLED_ClearArea(82,4,6,8);
	}
	else	//个位数字电量显示
	{
		OLED_ShowImage(110,0,16,16,Battery);
		OLED_ClearArea(113,5,10,6);	//电池电量UI清除
		OLED_ClearArea(82,4,12,8);			//百位十位电量显示清除
	}	
}


//首页用户界面
void Show_Clock_UI(void)
{
	MyRTC_ReadTime();	//读取tm日期时间结构体时钟数据
	OLED_Printf(0,0,OLED_6X8,"%d-%d-%d",MyRTC_Time[0],MyRTC_Time[1],MyRTC_Time[2]);					//显示年-月-日
	OLED_Printf(16,16,OLED_12X24,"%02d:%02d:%02d",MyRTC_Time[3],MyRTC_Time[4],MyRTC_Time[5]);		//显示时:分:秒
	OLED_ShowString(0,48,"菜单",OLED_8X16);
	OLED_ShowString(96,48,"设置",OLED_8X16);
	Battery_Show_UI();
}

uint8_t Clockmoveflag = 1;	/* 首页时钟页面光标位置，1:菜单选项 2:设置选项 */
static uint8_t KeyNum;            /* 临时按键键码变量 */

//按键控制时钟页面光标移动逻辑实现
uint8_t First_Page_Clock(void)
{
	while(1) 					//Get key's number continuely
	{
		KeyNum = Key_GetNum(); 	//Get Key Number
		if(KeyNum == 1) 		//Key1: last item
		{
			//press key1 to move last one 
			Clockmoveflag --;
			if(Clockmoveflag <= 0) 
			{
				Clockmoveflag = 2; //cursor move to second item [设置]
			}
		}
		//when press key2 move to next item, cursor move to first item
		else if(KeyNum == 2) 	//Key2: next item
		{
			Clockmoveflag ++;
			if(Clockmoveflag >= 3)
			{
				Clockmoveflag = 1; //cursor move to first item [菜单]
			}
		}
		else if(KeyNum == 3) 	//Key3: short press to [确认]
		{
			OLED_Clear();  			//Clear Screen
			OLED_Update(); 			//ReFlash Screen
			return Clockmoveflag; //return position of cursor
			
		}
		else if(KeyNum == 4) 	//Long Press Key3 
		{
			//PMOS低电平导通，高电平截止
			if (GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_13) == 1)		//获取输出寄存器的状态，如果当前引脚输出低电平
			{
				GPIO_ResetBits(GPIOB,GPIO_Pin_13); //MCU Shut down
				GPIO_SetBits(GPIOB,GPIO_Pin_12); //High voltage  PMOS stop, ADC stop 
			}
			else													//否则，即当前引脚输出高电平
			{
				GPIO_ResetBits(GPIOB, GPIO_Pin_12);
				GPIO_SetBits(GPIOB, GPIO_Pin_13);			
			}
		}
		
		/******Key Function Control******/
		switch(Clockmoveflag)
		{
			case 1: //[菜单]
				Show_Clock_UI();
				OLED_ReverseArea(0,48,32,16);
				OLED_Update();
				break;
			case 2: //[设置]
				Show_Clock_UI();
				OLED_ReverseArea(96,48,32,16);
				OLED_Update();
				break;
			default:
				break;
		}
	}
}



/*************************设置页面************************************/
uint8_t ClockUI_Move_Flag;         /* 设置页面光标位置标志 */

/**
  * @brief 显示设置页面UI
  * @param  无
  * @retval 无
  */
void Show_Setting_UI(void)
{
	OLED_ShowImage(0,0,16,16,GoBack);				//显示返回图标
	OLED_ShowString(0,16,"日期时间设置",OLED_8X16);	//显示[日期时间设置]
}

uint8_t SettingFlag = 1; 		//按键光标初始位置在[返回]图标处

uint8_t SettingPage(void)
{
	uint8_t Temp_SettingFlag = 0;		//返回设置页面光标位置
	while(1) //Get key's number continuely
	{
		KeyNum = Key_GetNum(); //Get Key Number
		if(KeyNum == 1) //Key1: last item
		{
			//press key1 to move last one 
			SettingFlag--;
			if(SettingFlag <= 0) 
			{
				SettingFlag = 2; //cursor move to second item [日期时间设置]
			}
		}
		//when press key2 move to next item, cursor move to first item
		else if(KeyNum == 2) //Key2: next item
		{
			SettingFlag ++;
			if(SettingFlag >= 3)
			{
				SettingFlag = 1; //cursor move to first item [返回]
			}
		}
		else if(KeyNum == 3) //Key3: press to confirmation
		{
			OLED_Clear();  //Clear Screen
			OLED_Update(); //ReFlash Screen
			Temp_SettingFlag = SettingFlag;
		}
		
		if(Temp_SettingFlag == 1)				//光标位置：【返回】
		{
			Temp_SettingFlag = 0;
			OLED_Clear();
			return 0;
		}
		else if(Temp_SettingFlag == 2)			//光标位置：【日期时间设置】
		{
			Temp_SettingFlag = 0;
			SetTime_mainprocess();				//调用[设置日期时间]函数	
			OLED_Clear();
			
		}
		
		/******Background_Color of Function selection******/
		switch(SettingFlag)
		{
			case 1: //[返回]
				Show_Setting_UI();
				OLED_ReverseArea(0,0,16,16);
				OLED_Update();
				break;
			case 2: //[日期时间设置]
				Show_Setting_UI();
				OLED_ReverseArea(0,16,96,16);
				OLED_Update();
				break;
		}
	}
}



/********************************菜单页面************************************/

uint8_t MenuFlag = 2; 		//菜单图标位置标志位

uint8_t Pre_item;			//当前项
uint8_t Target_item;		//目标项目
uint8_t Pre_x;				//上一次x的坐标
uint8_t move_step = 4;		//图标移动步长
uint8_t move_stateFlag = 1;		//1:开始移动，0:停止移动

/*
*  菜单滑动动画显示函数
*/
void Menu_Animation(void)
{
	OLED_Clear();
	OLED_ShowImage(42,10,44,44,Frame);			//菜单选择框
	//OLED_ShowImage(48,16,32,32,Menu_Graph);		//菜单图标

	//菜单整体左移
	if(Pre_item < Target_item)
	{
		Pre_x -= move_step;
		if(Pre_x == 0)				//前回图标移动至x=0处
		{
			Pre_item++;				//菜单移动到下一项
			Pre_x = 48;				//前回坐标更新为x=48
			move_stateFlag = 0;		//停止移动
		}
	}

	//菜单整体右移
	if(Pre_item > Target_item)
	{
		Pre_x += move_step;
		if(Pre_x == 96)			//前回图标移动至x=96处
		{
			Pre_item--;			//菜单移动到上一项
			Pre_x = 48;			//前回坐标更新为x=48
			move_stateFlag = 0;	//停止移动
		}
	}

	if(Pre_item >= 1)			//第一项及以后的菜单选项
	{
		OLED_ShowImage(Pre_x-48,16,32,32,Menu_Graph[Pre_item-1]);	//显示前一个菜单图标
	}
	if(Pre_item >= 2)			//第二项及以后的菜单选项
	{
		OLED_ShowImage(Pre_x-96,16,32,32,Menu_Graph[Pre_item-2]);	//显示上上个图标
	}

	/*保证图标滑动的连续性*/
	OLED_ShowImage(Pre_x,16,32,32,Menu_Graph[Pre_item]);		//显示选中的菜单图标
	OLED_ShowImage(Pre_x+48,16,32,32,Menu_Graph[Pre_item+1]);	//显示选中图标后第一个图标
	OLED_ShowImage(Pre_x+96,16,32,32,Menu_Graph[Pre_item+2]);	//显示选中图标后第二个图标

	OLED_Update();
}

void MenuToFunction_Animation(void)
{
	for(uint8_t i=0; i<=6; i++)
	{
		OLED_Clear();				//清屏
		if(Pre_item >= 1)			//当前选项及以后的菜单选项时
		{
			/*保证图标滑动的连续性*/
			OLED_ShowImage(Pre_x-48,16+i*8,32,32,Menu_Graph[Pre_item-1]);	//
			OLED_ShowImage(Pre_x,16+i*8,32,32,Menu_Graph[Pre_item]);		//显示选中的菜单图标
			OLED_ShowImage(Pre_x+48,16+i*8,32,32,Menu_Graph[Pre_item+1]);	//显示选中图标后第一个图标

		}
		
		// OLED_ShowImage(Pre_x+96,16,32,32,Menu_Graph[Pre_item+2]);	//显示选中图标后第二个图标

		OLED_Update();
	}


}

/*
*	设置菜单选项
*/
void Set_Selection(uint8_t move_flag, uint8_t pre_item, uint8_t target_item)
{
	if(move_flag == 1)
	{
		Pre_item = pre_item;
		Target_item = target_item;
		//Menu_Animation();
	}
	Menu_Animation();
}


/*
*  菜单页面图标按键控制逻辑实现
*/
uint8_t Menu_Page(void)
{
	uint8_t Temp_MenuFlag = 0;			//返回设置页面光标位置
	uint8_t Direct_Flag = 2;			//图标移动方向 1：上一项 2：下一项
	move_stateFlag = 1;
	
	while(1) //Get key's number continuely
	{
		KeyNum = Key_GetNum(); //Get Key Number
		if(KeyNum == 1) //Key1: last item
		{
			//press key1 to move last one 
			Direct_Flag = 1;		//上一项
			move_stateFlag = 1;		//开始移动
			MenuFlag--;				//菜单项向右滚动
			if(MenuFlag <= 0) 
			{
				MenuFlag = 7; 		//cursor move to [水平仪]
			}
		}
		//when press key2 move to next item, cursor move to first item
		else if(KeyNum == 2) //Key2: next item
		{
			Direct_Flag = 2;		//下一项
			move_stateFlag = 1;		//开始移动
			MenuFlag ++;			//菜单项向左滚动
			if(MenuFlag >= 8)
			{
				MenuFlag = 1; 		//cursor move to [返回]
			}
		}
		else if(KeyNum == 3) 		//Key3: press to confirmation
		{
			Direct_Flag = 0;
			OLED_Clear();  			//Clear Screen
			OLED_Update(); 			//ReFlash Screen

			Temp_MenuFlag = MenuFlag;
		}
		
		/*菜单光标位置与对应功能跳转*/
		if(Temp_MenuFlag == 1)				//光标位置1：【返回】
		{
			Temp_MenuFlag = 0;
			MenuToFunction_Animation();
			OLED_Clear();
			return 0;
		}
		else if(Temp_MenuFlag == 2)			//光标位置2：【秒表】
		{
			Temp_MenuFlag = 0;
			MenuToFunction_Animation();
			StopClock();					//调用[秒表]函数	
			OLED_Clear();
		}
		else if(Temp_MenuFlag == 3)			//光标位置3：【手电筒】
		{
			Temp_MenuFlag = 0;
			MenuToFunction_Animation();
			flashlight_Func();
			OLED_Clear();
		}
		else if(Temp_MenuFlag == 4)			//光标位置4：【MPU6050】
		{
			Temp_MenuFlag = 0;
			MenuToFunction_Animation();
			MPU6050_Main();
			OLED_Clear();
		}
		else if(Temp_MenuFlag == 5)			//光标位置5：【游戏】
		{
			Temp_MenuFlag = 0;
			MenuToFunction_Animation();
			Game();
			OLED_Clear();
		}
		else if(Temp_MenuFlag == 6)			//光标位置6：【动态表情】
		{
			Temp_MenuFlag = 0;
			MenuToFunction_Animation();
			Emoji_Func();
			OLED_Clear();
		}
		else if(Temp_MenuFlag == 7)			//光标位置7：【水平仪】
		{
			Temp_MenuFlag = 0;
			MenuToFunction_Animation();
			Gradienter_Func();
			OLED_Clear();
		}
		else
		{
			;
		}
		
		Menu_Animation();


		if(MenuFlag==1)								//菜单位置：【返回】
		{
			
			if(Direct_Flag == 1)					//上一项
			{
				Set_Selection(move_stateFlag,1,0);	
			}
			else if(Direct_Flag == 2)				//下一项
			{
				Set_Selection(move_stateFlag,0,0);
			} 
		}
		else
		{
			if(Direct_Flag == 1){Set_Selection(move_stateFlag,MenuFlag,MenuFlag-1);}
			else if(Direct_Flag == 2){Set_Selection(move_stateFlag,MenuFlag-2,MenuFlag-1);}
		}
		//Menu_Animation();
		
	}
}


/********************************秒表************************************/
uint8_t StopClock_Flag = 1;        /* 秒表页面光标位置：1-返回 2-开始 3-停止 4-清除 */
uint8_t hour,min,sec;              /* 秒表计时值：时、分、秒 */
uint8_t start_timing_flag = 0;     /* 计时标志：1-开始计时 0-停止计时 */

/**
  * @brief 显示秒表页面UI
  * @param  无
  * @retval 无
  */
void Show_StopClock_UI(void)
{
	OLED_ShowImage(0,0,16,16,GoBack);			//返回图标
	OLED_Printf(32,20,OLED_8X16,"%02d:%02d:%02d",hour,min,sec);		//时分秒显示
	OLED_ShowString(8,44,"开始",OLED_8X16);
	OLED_ShowString(48,44,"停止",OLED_8X16);
	OLED_ShowString(88,44,"清除",OLED_8X16);
}

//秒数自增函数
void StopClock_Tick(void)
{
	//start_timing_flag = 0;
	static uint16_t Timer_count;
	Timer_count++;
	if(Timer_count >= 1000)		//每隔1秒进行时间递增
	{
		Timer_count = 0;
		if(start_timing_flag == 1)
		{	
			sec++;
			if(sec >= 60)
			{
				sec = 0;
				min++;
				if(min >= 60)
				{
					min = 0;
					hour++;
					if(hour >= 99)
					{
						hour = 0;
					}
				}
			}
		}
	}
}

/**
  * @brief 秒表开始计时
  * @param  无
  * @retval 无
  */
void ClkCount_Start(void)
{
	start_timing_flag = 1;	//开始计时
}

/**
  * @brief 秒表停止计时
  * @param  无
  * @retval 无
  */
void ClkCount_Stop(void)
{
	start_timing_flag = 0;  //停止计时
}

/**
  * @brief 秒表清除计时
  * @param  无
  * @retval 无
  */
void ClkCount_Clear(void)
{
	start_timing_flag = 0;
	hour = min = sec = 0;  //清零计时值
}

/*
* 秒表功能实现函数
*/
int StopClock(void)
{
	uint8_t Temp_StopClock_Flag = 0;		//返回秒表页面光标位置
	while(1) //Get key's number continuely
	{
		KeyNum = Key_GetNum(); //Get Key Number
		if(KeyNum == 1) //Key1: last item
		{
			//press key1 to move last one 
			StopClock_Flag--;
			if(StopClock_Flag <= 0) 
			{
				StopClock_Flag = 4; 		//cursor move to second function： [清除]
			}
		}
		//when press key2 move to next item, cursor move to first item
		else if(KeyNum == 2) //Key2: next item
		{
			StopClock_Flag ++;
			if(StopClock_Flag > 4)
			{
				StopClock_Flag = 1; //cursor move to first item [返回]
			}
		}
		else if(KeyNum == 3) //Key3: press to confirmation
		{
			OLED_Clear();  //Clear Screen
			OLED_Update(); //ReFlash Screen
			Temp_StopClock_Flag = StopClock_Flag;
		}

		
		if(Temp_StopClock_Flag == 1)				//光标位置：【返回】
		{
			OLED_Clear();
			return 0;
		}
		else if(Temp_StopClock_Flag == 2)			//光标位置：【开始】
		{
			ClkCount_Start();						//调用[开始计时]函数	
			OLED_Clear();			
		}
		else if(Temp_StopClock_Flag == 3)			//光标位置：【停止】
		{
			ClkCount_Stop();						//调用[停止计时]函数	
			OLED_Clear();			
		}
		else if(Temp_StopClock_Flag == 4)			//光标位置：【清除】
		{
			ClkCount_Clear();						//调用[清除计时]函数	
			OLED_Clear();			
		}

		
		/******Background_Color of Function selection******/
		switch(StopClock_Flag)
		{
			case 1: 		//[返回]图标处
				Show_StopClock_UI();
				OLED_ReverseArea(0,0,16,16);
				OLED_Update();
				break;
			case 2: 		//[开始]图标处
				Show_StopClock_UI();
				//start_timing_flag = 1;
				OLED_ReverseArea(8,44,32,16);
				OLED_Update();
				break;
			case 3:			//[停止]图标处
				Show_StopClock_UI();
				//start_timing_flag = 0;
				OLED_ReverseArea(48,44,32,16);
				OLED_Update();
				break;	
			case 4:			//[清除]图标处
				Show_StopClock_UI();
				//start_timing_flag = 0;
				//hour = min = sec = 0;
				OLED_ReverseArea(88,44,32,16);
				OLED_Update();
				break;
		}
	}
}


/********************************手电筒************************************/
uint8_t flashlight_Flag = 1;         /* 手电筒页面光标位置：1-返回 2-OFF 3-ON */

/**
  * @brief 显示手电筒页面UI
  * @param  无
  * @retval 无
  */
void Show_flashlight_UI(void)
{
	OLED_ShowImage(0,0,16,16,GoBack);			//返回图标
	OLED_ShowString(20,24,"OFF",OLED_8X16);
	OLED_ShowString(84,24,"ON",OLED_8X16);
}

/**
  * @brief 手电筒打开
  * @param  无
  * @retval 无
  */
void flashlight_ON(void)
{
	LED1_ON();
}

/**
  * @brief 手电筒关闭
  * @param  无
  * @retval 无
  */
void flashlight_OFF(void)
{
	LED1_OFF();
}

int flashlight_Func(void)
{
	uint8_t Temp_flashlight_Flag = 0;		//返回秒表页面光标位置
	//Show_flashlight_UI();
	while(1) //Get key's number continuely
	{
		KeyNum = Key_GetNum(); //Get Key Number
		if(KeyNum == 1) //Key1: last item
		{
			//press key1 to move last one 
			flashlight_Flag--;
			if(flashlight_Flag <= 0) 
			{
				flashlight_Flag = 3; 		//cursor move to second function： [清除]
			}
		}
		//when press key2 move to next item, cursor move to first item
		else if(KeyNum == 2) //Key2: next item
		{
			flashlight_Flag ++;
			if(flashlight_Flag > 3)
			{
				flashlight_Flag = 1; //cursor move to first item [返回]
			}
		}
		else if(KeyNum == 3) //Key3: press to confirmation
		{
			OLED_Clear();  //Clear Screen
			OLED_Update(); //ReFlash Screen
			Temp_flashlight_Flag = flashlight_Flag;
		}

		
		if(Temp_flashlight_Flag == 1)				//光标位置：【返回】
		{
			flashlight_OFF();
			return 0;
		}
		else if(Temp_flashlight_Flag == 2)			//光标位置：【OFF】
		{
			flashlight_OFF();
		}
		else if(Temp_flashlight_Flag == 3)			//光标位置：【ON】
		{
			flashlight_ON();
		}
		
		
		/******Background_Color of Function selection******/
		switch(flashlight_Flag)
		{
			case 1: 		//[返回]图标处
				Show_flashlight_UI();
				OLED_ReverseArea(0,0,16,16);
				OLED_Update();
				break;
			case 2: 		//[OFF]图标处 
				Show_flashlight_UI();
				OLED_ReverseArea(20,24,24,16);
				OLED_Update();
				break;
			case 3:			//[ON]图标处
				Show_flashlight_UI();
				OLED_ReverseArea(84,24,16,16);
				OLED_Update();
				break;	
			default:
				Show_flashlight_UI();
				break;
		}
	}

}


/********************************MPU6050************************************/

float delta = 0.005;                 /* 采样周期，单位秒 */
float a = 0.9;                       /* 互补滤波系数，范围0~1，越大陀螺仪权重越高 */
int16_t ax,ay,az;                    /* 加速度计X、Y、Z轴原始数据 */
int16_t gx,gy,gz;                   /* 陀螺仪X、Y、Z轴原始数据 */
float roll_a,pitch_a;               /* 加速度计计算的横滚角和俯仰角 */
float roll_g,pitch_g,yaw_g;         /* 陀螺仪计算的横滚角、俯仰角、偏航角 */
float Roll=0.0,Pitch=0.0,Yaw=0.0;    /* 融合后的欧拉角 */
double Pi = 3.1415927;

/**
  * @brief MPU6050欧拉角解算（互补滤波算法）
  * @param  无
  * @retval 无
  * @note   结合加速度计和陀螺仪数据，使用互补滤波算法融合得到稳定的角度
  *         陀螺仪动态响应好但有漂移，加速度计静态精度高但动态响应差
  */
void MPU6050_Calculation_Euler_angles(void)
{
	Delay_ms(5);									//采样周期为5ms
	MPU6050_GetData(&ax,&ay,&az,&gx,&gy,&gz);		//传地址方式，返回陀螺仪加速度值
	/*陀螺仪欧拉角计算*/
	roll_g = Roll + (float)gx*delta;				//横滚角x轴
	pitch_g = Pitch + (float)gy*delta;				//俯仰角y轴
	yaw_g = Yaw + (float)gz*delta;					//偏航角z轴

	/*加速度计欧拉角计算*/
	roll_a = atan2(ay,az)*180/Pi;					//横滚角x轴
	pitch_a = atan2((-1)*ax,az)*180/Pi;				//俯仰角y轴

	/*互补滤波*/
	//参数a越大；陀螺仪数据占比越大
	//参数a越小：加速度计数据占比越大
	Roll =  a*roll_g + (1-a)*roll_a;
	Pitch = a*pitch_g +(1-a)*pitch_a;
	Yaw = a*yaw_g;
}

/**
  * @brief 显示MPU6050数据页面UI
  * @param  无
  * @retval 无
  */
void Show_MPU6050_UI(void)
{
	OLED_ShowImage(0,0,16,16,GoBack);			//返回图标
	OLED_Printf(0,16,OLED_8X16,"Roll:  %.2f",Roll);
	OLED_Printf(0,32,OLED_8X16,"Pitch: %.2f",Pitch);
	OLED_Printf(0,48,OLED_8X16,"Yaw:   %.2f",Yaw);
}

int MPU6050_Main(void)
{
	while (1)
	{
		KeyNum = Key_GetNum();
		if(KeyNum == 3)			//确认键按下
		{
			OLED_Clear();		//清屏
			OLED_Update();		//刷新屏幕
			return 0;
		}

		OLED_Clear();
		MPU6050_Calculation_Euler_angles();
		Show_MPU6050_UI();
		OLED_ReverseArea(0,0,16,16);	//反相显示返回图标
		OLED_Update();
	}
	
}

/*********************************Dino_Game********************************************/

/**
  * @brief 显示游戏选择页面UI
  * @param  无
  * @retval 无
  */
void Show_Game_UI(void)
{
	OLED_ShowImage(0,0,16,16,GoBack);			//返回图标
	OLED_ShowString(0,16,"Google DinoGame",OLED_8X16);
}

uint8_t game_flag = 1;              /* 游戏选择页面光标：1-返回 2-进入游戏 */

int Game(void)
{
	uint8_t Temp_game_flag = 0;
	while(1) //Get key's number continuely
	{
		KeyNum = Key_GetNum(); //Get Key Number
		if(KeyNum == 1) //Key1: last item
		{
			//press key1 to move last one 
			game_flag--;
			if(game_flag <= 0) 
			{
				game_flag = 2; 		//游戏标题
			}
		}
		//when press key2 move to next item, cursor move to first item
		else if(KeyNum == 2) //Key2: next item
		{
			game_flag ++;
			if(game_flag >= 3)
			{
				game_flag = 1; //cursor move to first item [返回]
			}
		}
		else if(KeyNum == 3) //Key3: press to confirmation
		{
			OLED_Clear();  //Clear Screen
			OLED_Update(); //ReFlash Screen
			Temp_game_flag = game_flag;
		}

		
		if(Temp_game_flag == 1)				//光标位置：【返回】
		{
			return 0;
		}
		else if(Temp_game_flag == 2)			//光标位置：【进入游戏】
		{
			Game_Init();
			Dino_game_Animation();
			Temp_game_flag = 0;
		}
		else
		{
			;
		}
		
		
		/******Background_Color of Function selection******/
		switch(game_flag)
		{
			case 1: 		//[返回]图标处
				Show_Game_UI();
				OLED_ReverseArea(0,0,16,16);
				OLED_Update();
				break;
			case 2: 		//[]图标处 
				Show_Game_UI();
				OLED_ReverseArea(0,16,120,16);
				OLED_Update();
				break;
			default:
				Show_Game_UI();
				break;
		}
	}

}



/*********************************动态表情包********************************************/

/**
  * @brief 显示动态表情动画（睁眼/闭眼循环）
  * @param  无
  * @retval 无
  * @note   循环显示眨眼动画：闭眼->睁眼->延时->重复
  */
void Show_emoji_UI(void)
{
	//闭眼
	for(uint8_t i=0; i<=3; i++)
	{
		OLED_Clear();
		//左眉毛
		OLED_ShowImage(30,10+i,16,16,eyebrow[0]);
		//右眉毛
		OLED_ShowImage(82,10+i,16,16,eyebrow[1]);
		//左眼
		OLED_DrawEllipse(40,32,6,6-i,1);
		//右眼
		OLED_DrawEllipse(88,32,6,6-i,1);
		//嘴巴
		OLED_ShowImage(54,40,20,20,mouth);
		OLED_Update();
		Delay_ms(100);
	}

	//睁眼
	for(uint8_t i=0; i<=3; i++)
	{
		OLED_Clear();
		//左眉毛
		OLED_ShowImage(30,13-i,16,16,eyebrow[0]);
		//右眉毛
		OLED_ShowImage(82,13-i,16,16,eyebrow[1]);
		//左眼
		OLED_DrawEllipse(40,32,6,3+i,1);
		//右眼
		OLED_DrawEllipse(88,32,6,3+i,1);
		//嘴巴
		OLED_ShowImage(54,40,20,20,mouth);
		OLED_Update();
		Delay_ms(100);
	}

	Delay_ms(500);	
}

int Emoji_Func(void)
{
	while(1)
	{
		KeyNum = Key_GetNum();
		if(KeyNum == 3)			//确认键按下
		{
			OLED_Clear();		//清屏			
			OLED_Update();		//刷新屏幕
			return 0;
		}
		Show_emoji_UI();
	}

}


/*********************************水平仪********************************************/
/*
* @brief: 显示水平仪UI
*/
void Show_Gradienter_UI(void)
{
	int16_t x = 64-Roll;
	int16_t y = 32+Pitch;
	MPU6050_Calculation_Euler_angles();					//计算欧拉角
	OLED_DrawCircle(64,32,30,OLED_UNFILLED);			//绘制水平仪外围圆
	int16_t dx = x - 64;								//计算水平仪内实心圆相对大圆心的x坐标
	int16_t dy = y - 32;								//计算水平仪内实心圆相对大圆心的y坐标	
	float distance = sqrtf(dx*dx + dy*dy);				//计算实心圆距离大圆心的距离
	if(distance > 26.0f)
	{
		float scale = 26.0f / distance;					//计算缩放比例
		x = 64 + (dx * scale);							//计算水平仪内实心圆的x坐标
		y = 32 + (dy * scale);							//计算水平仪内实心圆的y坐标
	}
	OLED_DrawCircle(x,y,4,OLED_FILLED);	//绘制水平仪内实心圆
	OLED_Update();			//刷新屏幕
}

uint8_t Gradienter_Func(void)
{
	//Show_Gradienter_UI();
	while(1)
	{
		KeyNum = Key_GetNum();
		if(KeyNum == 3)			//确认键按下
		{
			OLED_Clear();		//清屏			
			OLED_Update();		//刷新屏幕
			return 0;
		}	
		OLED_Clear();
		Show_Gradienter_UI();
		OLED_Update();
	}
}

