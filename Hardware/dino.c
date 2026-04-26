#include "stm32f10x.h"                  // Device header
#include "OLED.h"
#include "Key.h"
#include "OLED.h"
#include <stdlib.h>
#include <math.h>
#include "Delay.h"

#define JUMP_HEIGHT 29                    /* 小恐龙跳跃最大高度 */

extern double Pi;

int score;                              /* 游戏得分 */
uint8_t score_count;                    /* 分数递增计数 */


uint16_t ground_count;                  /* 地面移动计数 */
uint16_t ground_Pos;                    /* 地面像素位置计数，范围：0~255（共256个像素点） */

uint8_t Barrier_Pos;                    /* 障碍物屏幕位置 */
uint8_t Barrier_Flag;                   /* 障碍物类型索引：0~2 */

uint8_t Cloud_Pos;                      /* 云朵屏幕位置 */
uint8_t Cloud_Count;                    /* 云朵移动计数 */

uint8_t Dino_jump_flag;                 /* 跳跃标志：1-跳跃中 0-地面 */
uint8_t Dino_jump_Pos;                  /* 当前跳跃高度 */
uint16_t Jump_count = 1;                /* 跳跃计时计数 */

/**
  * @brief 游戏对象边界结构体
  */
struct Object_Position{
    uint8_t minX,maxX,minY,maxY;
};


/**
  * @brief 游戏初始化
  * @param  无
  * @retval 无
  * @note   重置所有游戏变量到初始状态
  */
void Game_Init(void)
{
    score = score_count = ground_count = ground_Pos = Barrier_Pos = Barrier_Flag = Cloud_Pos = Cloud_Count = Dino_jump_flag = Dino_jump_Pos =Jump_count = 0;
}

/**
  * @brief 显示游戏分数
  * @param  无
  * @retval 无
  */
void Show_Score(void)
{
    OLED_ShowNum(96,0,score,5,OLED_6X8);
}

/*
* 游戏地面显示
* 将256像素长度的地面分为2组进行显示，每组128个像素长度
*/
void Show_Ground(void)
{
    if(ground_Pos < 128)
    {
        //地面1
        for(uint8_t i=0; i<128; i++)
        {
            //从第ground_Pos格开始的128格复制到显存数组中
            OLED_DisplayBuf[7][i] = Ground[ground_Pos+i]; //ground_Pos在循环中不变，循环结束后累加
        }
    }
    else   
    {
        //地面2
        for(uint8_t i=0; i<255-ground_Pos; i++)   //先算256像素的地面还剩多少格：255 - ground_Pos
        {
            OLED_DisplayBuf[7][i] = Ground[i+ground_Pos];
        }
        //地面1
        for(uint8_t i=255-ground_Pos; i<128; i++)   //屏幕剩下的位置用“地面”开头补齐，起始下标就是 255-ground_Pos，一直补到 127 
        {
            OLED_DisplayBuf[7][i] = Ground[i-(255-ground_Pos)]; //把下标“折回”到地面开头 
        }
    }

}


struct Object_Position Barr;

/**
  * @brief 显示障碍物
  * @param  无
  * @retval 无
  * @note   当障碍物移出屏幕左侧后，随机生成新障碍物
  */
void Show_Barrier(void)
{
    if(Barrier_Pos >= 143)
    {
        Barrier_Flag = rand()%3; //生成0~2的随机数
    }
    //以屏幕右下角为坐标原点向左为正方向计算X坐标
    OLED_ShowImage(127-Barrier_Pos,44,16,18,Barrier[Barrier_Flag]);

    /*障碍物边界值*/
    Barr.minX = 127-Barrier_Pos;
    Barr.maxX = 143-Barrier_Pos;
    Barr.minY = 44;
    Barr.maxY = 62;
}

/**
  * @brief 显示云朵
  * @param  无
  * @retval 无
  */
void Show_Cloud(void)
{
    OLED_ShowImage(127-Cloud_Pos,9,16,8,Cloud);
}


struct Object_Position dino;

/**
  * @brief 显示小恐龙
  * @param  无
  * @retval 无
  * @note   检测按键1触发跳跃，使用正弦函数实现平滑跳跃动画
  */
void Show_Dino(void)
{
    uint8_t KeyNum;
    KeyNum = Key_GetNum();
    if(KeyNum == 1 && Dino_jump_flag == 0)
    {
        Dino_jump_flag = 1;
        Dino_jump_Pos = 29;
    }

    if(Dino_jump_flag == 0)
    {
        if(Cloud_Pos%2 == 0)
        {
            OLED_ShowImage(0,44,16,18,Dino[0]);
        }
        else
        {
            OLED_ShowImage(0,44,16,18,Dino[1]);
        }
    }
    else
    {
        Dino_jump_Pos = JUMP_HEIGHT * sin((float)(Pi * Jump_count/1000));
        OLED_ShowImage(0,44-Dino_jump_Pos,16,18,Dino[2]);
    }

    /*小恐龙边界值*/
    dino.minX = 0;
    dino.maxX = 16;
    dino.minY = 44-Dino_jump_Pos;
    dino.maxY = 62-Dino_jump_Pos;
}

/**
  * @brief 碰撞检测函数
  * @param  a  对象A边界（障碍物）
  * @param  b  对象B边界（小恐龙）
  * @retval 0-未碰撞 1-碰撞
  */
uint8_t isColliding(struct Object_Position* a, struct Object_Position* b)
{
    if((a->minX < b->maxX) && (a->maxX > b->minX) && (a->minY < b->maxY) && (a->maxY > b->minY) )
    {
        OLED_Clear();
        OLED_ShowString(28,24,"Game Over",OLED_8X16);
        OLED_Update();
        Delay_s(1);
        OLED_Clear();
        OLED_Update();

        return 1;
    }

    return 0;
}

/**
  * @brief 游戏计时滴答函数（中断调用）
  * @param  无
  * @retval 无
  * @note   此函数在1ms定时器中断中调用，更新游戏中的各种计时器和位置
  */
void dino_tick(void)
{
    score_count++;
    ground_count++;
    Cloud_Count++;

    if(score_count >= 100)  //0.1秒变化一次分数值
    {
        score_count=0;
        score++;
    }

    if(ground_count >= 20)
    {
        ground_count = 0;
        ground_Pos++;
        Barrier_Pos++;
        if(ground_Pos >= 256)
        {
            ground_Pos = 0;
        }

        if(Barrier_Pos >= 144)
        {
            Barrier_Pos = 0;
        }
    }

    if(Cloud_Count >= 50)
    {
        Cloud_Pos++;
        Cloud_Count = 0;
        if(Cloud_Count > 200)
        {
            Cloud_Pos = 0;
        }
    }

    if(Dino_jump_flag == 1) 
    {
        Jump_count++;
        if(Jump_count >= 1000)
        {
            Dino_jump_flag = 0;
            Jump_count = 0;
        }
    }
}


/**
  * @brief 游戏主循环动画
  * @param  无
  * @retval 0-游戏结束返回
  * @note   循环更新游戏画面，检测碰撞
  */
uint8_t Dino_game_Animation(void)
{
    uint8_t return_flag = 0;
    while(1)
    {
        OLED_Clear();
        Show_Score();
        Show_Ground();
        Show_Barrier();
        Show_Cloud();
        Show_Dino();
        OLED_Update();

        return_flag = isColliding(&Barr,&dino);
        
        /*游戏结束退回到上一级*/
        if(return_flag  == 1)
        {
            return 0;
        }
    }
}
