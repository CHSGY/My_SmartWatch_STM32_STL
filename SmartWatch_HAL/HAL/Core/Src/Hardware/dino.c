/**
  * @file           : dino.c
  * @brief          : 小恐龙跑酷游戏模块（HAL库版本 / FreeRTOS 单帧渲染版）
  * @author         : CHSGY
  * @date           : 2026-06-07
  *
  * @note           : Phase 3 改造：Dino_game_Animation() 的 while(1) 提取为 Dino_RenderFrame()
  *                   单帧函数，由 Task_UI 每帧调用。Show_Dino() 中 Key_GetNum() 移除，
  *                   改用全局 Dino_JumpRequest 标志。
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

static uint8_t Dino_GameOver_Countdown = 0;
static uint8_t Dino_GameActive;

int Dino_Score;                         /* 游戏得分 */
uint8_t Dino_ScoreCount;                /* 分数累加计数器 */
uint8_t Dino_JumpRequest = 0;           /* 跳跃请求标志（由 Task_UI 设置，Show_Dino 消费后清零） */

uint16_t Dino_GroundCount;              /* 地面移动计数器 */
uint16_t Dino_GroundPos;                /* 地面纹理位置（循环范围0~255，共256个像素点） */

uint16_t Dino_BarrierPos;               /* 障碍物屏幕位置，范围0~143 */
uint8_t Dino_BarrierFlag;               /* 障碍物类型标志，范围0~2 */

uint16_t Dino_CloudPos;                 /* 云朵屏幕位置，范围0~200 */
uint8_t Dino_CloudCount;                /* 云朵移动计数器 */

uint8_t Dino_JumpFlag;                  /* 跳跃标志：1=跳跃中 0=地面 */
uint8_t Dino_JumpPos;                   /* 当前跳跃高度 */
uint16_t Dino_JumpCount = 0;            /* 跳跃计时器 */

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
  * @note   重置所有游戏变量为初始状态
  */
void Game_Init(void)
{
    Dino_GameActive = 1;
    Dino_GameOver_Countdown = 0;
    Dino_Score = Dino_ScoreCount = Dino_GroundCount = Dino_GroundPos = Dino_BarrierPos = Dino_BarrierFlag = Dino_CloudPos = Dino_CloudCount = Dino_JumpFlag = Dino_JumpPos = Dino_JumpCount = 0;
    Dino_JumpRequest = 0;
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
* 地面纹理显示
* 将256个像素长度的地面纹理分为2段来显示，每段128个像素长度
*/
void Show_Ground(void)
{
    if(Dino_GroundPos < DINO_SCREEN_WIDTH)
    {
        //段1
        for(uint8_t i=0; i<DINO_SCREEN_WIDTH; i++)
        {
            OLED_DisplayBuf[DINO_GROUND_PAGE][i] = Ground[Dino_GroundPos+i];
        }
    }
    else
    {
        //段2
        for(uint8_t i=0; i<GROUND_TEXTURE_LEN-1-Dino_GroundPos; i++)
        {
            OLED_DisplayBuf[DINO_GROUND_PAGE][i] = Ground[i+Dino_GroundPos];
        }
        //段1
        for(uint8_t i=GROUND_TEXTURE_LEN-1-Dino_GroundPos; i<DINO_SCREEN_WIDTH; i++)
        {
            OLED_DisplayBuf[DINO_GROUND_PAGE][i] = Ground[i-(GROUND_TEXTURE_LEN-1-Dino_GroundPos)];
        }
    }

}


struct Object_Position Barr;

/**
  * @brief 显示障碍物
  * @param  无
  * @retval 无
  * @note   当障碍物移出屏幕后重新生成随机障碍物
  */
void Show_Barrier(void)
{
    if(Dino_BarrierPos >= BARRIER_MAX_POS - 1)
    {
        Dino_BarrierFlag = rand() % BARRIER_TYPE_COUNT; //生成0~2随机数
    }
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
  * @note   Phase 3：Key_GetNum() 已移除，跳跃由全局 Dino_JumpRequest 标志触发
  *         使用正弦函数实现平滑跳跃曲线
  */
void Show_Dino(void)
{
    /* 检查跳跃请求（由 Task_UI 的 UI_ProcessKey 设置） */
    if(Dino_JumpRequest && Dino_JumpFlag == 0)
    {
        Dino_JumpFlag = 1;
        Dino_JumpPos = DINO_JUMP_HEIGHT;
        Dino_JumpRequest = 0;  /* 消费标志 */
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
    dino.maxX = DINO_X_POS + DINO_WIDTH;
    dino.minY = DINO_GROUND_Y - Dino_JumpPos;
    dino.maxY = DINO_GROUND_Y_END - Dino_JumpPos;
}

/**
  * @brief 碰撞检测函数（纯查询无副作用）
  * @param  a  对象A边界（障碍物）
  * @param  b  对象B边界（小恐龙）
  * @retval 0-未碰撞 1-碰撞
  */
uint8_t isColliding(struct Object_Position* a, struct Object_Position* b)
{
    return (a->minX < b->maxX) && (a->maxX > b->minX)
        && (a->minY < b->maxY) && (a->maxY > b->minY);
}

/**
  * @brief 显示游戏结束画面
  * @param  无
  * @retval 无
  */
void Show_GameOver(void)
{
    OLED_Clear();
    OLED_ShowString(28, 24, "Game Over", OLED_8X16);
    //OLED_Update();
    //delay_ms(1000);
    //OLED_Clear();
    //OLED_Update();
}

/**
  * @brief 游戏计时滴答（中断调用）
  * @param  无
  * @retval 无
  * @note   此函数在1ms定时器中断中调用，控制游戏中的各种计时和位移
  */
void dino_tick(void)
{
    if(Dino_GameActive == 0)
    {
        return;
    }
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
  * @brief 游戏单帧渲染（由 Task_UI 每帧调用）
  * @param  无
  * @retval 0-继续游戏 1-游戏结束
  * @note   Phase 3：替代原 Dino_game_Animation() 的 while(1) 循环
  *          Task_UI 每 33ms 调用一次，OLED_Clear/Update 由 Task_UI 统一处理
  */
uint8_t Dino_RenderFrame(void)
{
    if(Dino_GameOver_Countdown > 0)
    {
        Dino_GameOver_Countdown--;
        Show_Score();
        OLED_ShowString(28, 24, "Game Over", OLED_8X16);
        if(Dino_GameOver_Countdown == 0)
        {
            return 1;
        }
        return 0;
    }

    /* 先更新所有对象位置，再进行碰撞检测（防止复用上次游戏的残留碰撞数据） */
    Show_Score();
    Show_Ground();
    Show_Barrier();
    Show_Cloud();
    Show_Dino();

    if(isColliding(&Barr, &dino))
    {
        Dino_GameOver_Countdown = 30;
        Dino_GameActive = 0;
    }

    return 0;
}
