#include "stm32f10x.h"                  // Device header
#include "OLED.h"
#include "Key.h"
#include "OLED.h"
#include <stdlib.h>
#include <math.h>
#include "Delay.h"

#define JUMP_HEIGHT 29                    /* 小恐龙跳跃最大高度 */

extern double Pi;

int Dino_Score;                         /* 游戏得分 */
uint8_t Dino_ScoreCount;                /* 分数递增计数 */

uint16_t Dino_GroundCount;              /* 地面移动计数 */
uint16_t Dino_GroundPos;                /* 地面像素位置计数，范围：0~255（共256个像素点） */

uint16_t Dino_BarrierPos;               /* 障碍物屏幕位置，范围：0~143 */
uint8_t Dino_BarrierFlag;               /* 障碍物类型索引：0~2 */

uint16_t Dino_CloudPos;                 /* 云朵屏幕位置，范围：0~200 */
uint8_t Dino_CloudCount;                /* 云朵移动计数 */

uint8_t Dino_JumpFlag;                  /* 跳跃标志：1-跳跃中 0-地面 */
uint8_t Dino_JumpPos;                   /* 当前跳跃高度 */
uint16_t Dino_JumpCount = 1;            /* 跳跃计时计数 */

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
    Dino_Score = Dino_ScoreCount = Dino_GroundCount = Dino_GroundPos = Dino_BarrierPos = Dino_BarrierFlag = Dino_CloudPos = Dino_CloudCount = Dino_JumpFlag = Dino_JumpPos = Dino_JumpCount = 0;
}

/**
  * @brief 显示游戏分数
  * @param  无
  * @retval 无
  */
void Show_Score(void)
{
    OLED_ShowNum(96,0,Dino_Score,5,OLED_6X8);
}

/*
* 游戏地面显示
* 将256像素长度的地面分为2组进行显示，每组128个像素长度
*/
void Show_Ground(void)
{
    if(Dino_GroundPos < 128)
    {
        //地面1
        for(uint8_t i=0; i<128; i++)
        {
            //从第Dino_GroundPos格开始的128格复制到显存数组中
            OLED_DisplayBuf[7][i] = Ground[Dino_GroundPos+i]; //Dino_GroundPos在循环中不变，循环结束后累加
        }
    }
    else   
    {
        //地面2
        for(uint8_t i=0; i<255-Dino_GroundPos; i++)   //先算256像素的地面还剩多少格：255 - Dino_GroundPos
        {
            OLED_DisplayBuf[7][i] = Ground[i+Dino_GroundPos];
        }
        //地面1
        for(uint8_t i=255-Dino_GroundPos; i<128; i++)   //屏幕剩下的位置用"地面"开头补齐，起始下标就是 255-Dino_GroundPos，一直补到 127 
        {
            OLED_DisplayBuf[7][i] = Ground[i-(255-Dino_GroundPos)]; //把下标"折回"到地面开头 
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
    if(Dino_BarrierPos >= 143)
    {
        Dino_BarrierFlag = rand()%3; //生成0~2的随机数
    }
    //以屏幕右下角为坐标原点向左为正方向计算X坐标
    OLED_ShowImage(127-Dino_BarrierPos,44,16,18,Barrier[Dino_BarrierFlag]);

    /*障碍物边界值*/
    Barr.minX = 127-Dino_BarrierPos;
    Barr.maxX = 143-Dino_BarrierPos;
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
    OLED_ShowImage(127-Dino_CloudPos,9,16,8,Cloud);
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
    if(KeyNum == 1 && Dino_JumpFlag == 0)
    {
        Dino_JumpFlag = 1;
        Dino_JumpPos = 29;
    }

    if(Dino_JumpFlag == 0)
    {
        if(Dino_CloudPos%2 == 0)
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
        Dino_JumpPos = JUMP_HEIGHT * sin((float)(Pi * Dino_JumpCount/1000));
        OLED_ShowImage(0,44-Dino_JumpPos,16,18,Dino[2]);
    }

    /*小恐龙边界值*/
    dino.minX = 0;
    dino.maxX = 16;
    dino.minY = 44-Dino_JumpPos;
    dino.maxY = 62-Dino_JumpPos;
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
    Dino_ScoreCount++;
    Dino_GroundCount++;
    Dino_CloudCount++;

    if(Dino_ScoreCount >= 100)  //0.1秒变化一次分数值
    {
        Dino_ScoreCount=0;
        Dino_Score++;
    }

    if(Dino_GroundCount >= 20)
    {
        Dino_GroundCount = 0;
        Dino_GroundPos++;
        Dino_BarrierPos++;
        if(Dino_GroundPos >= 256)
        {
            Dino_GroundPos = 0;
        }

        if(Dino_BarrierPos >= 144)
        {
            Dino_BarrierPos = 0;
        }
    }

    if(Dino_CloudCount >= 50)
    {
        Dino_CloudCount = 0;
        Dino_CloudPos++;
        if(Dino_CloudPos > 200)
        {
            Dino_CloudPos = 0;
        }
    }

    if(Dino_JumpFlag == 1) 
    {
        Dino_JumpCount++;
        if(Dino_JumpCount >= 1000)
        {
            Dino_JumpFlag = 0;
            Dino_JumpCount = 0;
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
