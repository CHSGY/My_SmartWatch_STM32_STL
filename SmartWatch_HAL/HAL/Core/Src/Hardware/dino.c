/**
  * @file           : dino.c
  * @brief          : 小恐龙跑酷游戏模块（HAL库版本）
  * @author         : CHSGY
  * @date           : 2026-06-07
  *
  * @note           : 从标准库dino.c移植，主要修改点：
  *                   1. 头文件：stm32f10x.h -> main.h, Delay.h -> delay.h
  *                   2. OLED/Key包含路径增加 Hardware/ 前缀
  *                   3. 延时函数：Delay_s() -> delay_ms()
  */

#include "main.h"
#include "Hardware/OLED.h"
#include "Hardware/Key.h"
#include <stdlib.h>
#include <math.h>
#include "delay.h"
#include "Hardware/menu.h"
#include "Hardware/dino.h"

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
    OLED_ShowNum(SCORE_DISPLAY_X, SCORE_DISPLAY_Y, Dino_Score, 5, OLED_6X8);
}

/*
* 游戏地面显示
* 将256像素长度的地面分为2组进行显示，每组128个像素长度
*/
void Show_Ground(void)
{
    if(Dino_GroundPos < DINO_SCREEN_WIDTH)
    {
        //地面1
        for(uint8_t i=0; i<DINO_SCREEN_WIDTH; i++)
        {
            //从第Dino_GroundPos格开始的128格复制到显存数组中
            OLED_DisplayBuf[DINO_GROUND_PAGE][i] = Ground[Dino_GroundPos+i]; //Dino_GroundPos在循环中不变，循环结束后累加
        }
    }
    else   
    {
        //地面2
        for(uint8_t i=0; i<GROUND_TEXTURE_LEN-1-Dino_GroundPos; i++)   //先算256像素的地面还剩多少格
        {
            OLED_DisplayBuf[DINO_GROUND_PAGE][i] = Ground[i+Dino_GroundPos];
        }
        //地面1
        for(uint8_t i=GROUND_TEXTURE_LEN-1-Dino_GroundPos; i<DINO_SCREEN_WIDTH; i++)   //屏幕剩下的位置用"地面"开头补齐
        {
            OLED_DisplayBuf[DINO_GROUND_PAGE][i] = Ground[i-(GROUND_TEXTURE_LEN-1-Dino_GroundPos)]; //把下标"折回"到地面开头 
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
    if(Dino_BarrierPos >= BARRIER_MAX_POS - 1)
    {
        Dino_BarrierFlag = rand() % BARRIER_TYPE_COUNT; //生成0~2的随机数
    }
    //以屏幕右下角为坐标原点向左为正方向计算X坐标
    OLED_ShowImage(DINO_SCREEN_WIDTH - 1 - Dino_BarrierPos, DINO_GROUND_Y, BARRIER_WIDTH, BARRIER_HEIGHT, Barrier[Dino_BarrierFlag]);

    /*障碍物边界值*/
    Barr.minX = DINO_SCREEN_WIDTH - 1 - Dino_BarrierPos;
    Barr.maxX = DINO_SCREEN_WIDTH - 1 - Dino_BarrierPos + BARRIER_WIDTH;
    Barr.minY = DINO_GROUND_Y;
    Barr.maxY = DINO_GROUND_Y_END;
}

/**
  * @brief 显示云朵
  * @param  无
  * @retval 无
  */
void Show_Cloud(void)
{
    OLED_ShowImage(DINO_SCREEN_WIDTH - 1 - Dino_CloudPos, CLOUD_Y_POS, CLOUD_WIDTH, CLOUD_HEIGHT, Cloud);
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
        Dino_JumpPos = DINO_JUMP_HEIGHT;
    }

    if(Dino_JumpFlag == 0)
    {
        if(Dino_CloudPos%2 == 0)
        {
            OLED_ShowImage(DINO_X_POS, DINO_GROUND_Y, DINO_WIDTH, DINO_HEIGHT, Dino[0]);
        }
        else
        {
            OLED_ShowImage(DINO_X_POS, DINO_GROUND_Y, DINO_WIDTH, DINO_HEIGHT, Dino[1]);
        }
    }
    else
    {
        Dino_JumpPos = DINO_JUMP_HEIGHT * sin((float)(Pi * Dino_JumpCount / DINO_JUMP_DURATION));
        OLED_ShowImage(DINO_X_POS, DINO_GROUND_Y - Dino_JumpPos, DINO_WIDTH, DINO_HEIGHT, Dino[2]);
    }

    /*小恐龙边界值*/
    dino.minX = DINO_X_POS;
    dino.maxX = DINO_WIDTH;
    dino.minY = DINO_GROUND_Y - Dino_JumpPos;
    dino.maxY = DINO_GROUND_Y_END - Dino_JumpPos;
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
        delay_ms(1000);
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

    if(Dino_ScoreCount >= SCORE_TICK_PERIOD)  //0.1秒变化一次分数值
    {
        Dino_ScoreCount=0;
        Dino_Score++;
    }

    if(Dino_GroundCount >= GROUND_MOVE_PERIOD)
    {
        Dino_GroundCount = 0;
        Dino_GroundPos++;
        Dino_BarrierPos++;
        if(Dino_GroundPos >= GROUND_TEXTURE_LEN)
        {
            Dino_GroundPos = 0;
        }

        if(Dino_BarrierPos >= BARRIER_MAX_POS)
        {
            Dino_BarrierPos = 0;
        }
    }

    if(Dino_CloudCount >= CLOUD_MOVE_PERIOD)
    {
        Dino_CloudCount = 0;
        Dino_CloudPos++;
        if(Dino_CloudPos > CLOUD_MAX_POS)
        {
            Dino_CloudPos = 0;
        }
    }

    if(Dino_JumpFlag == 1) 
    {
        Dino_JumpCount++;
        if(Dino_JumpCount >= DINO_JUMP_DURATION)
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
