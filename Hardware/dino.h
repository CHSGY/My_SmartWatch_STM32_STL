#ifndef __DINO_H
#define __DINO_H

/**
  * @brief 小恐龙跑酷游戏模块
  * @details Google Dino游戏的STM32移植版
  */

/**
  * @brief 游戏初始化
  */
void Game_Init(void);

/**
  * @brief 显示游戏分数
  */
void Show_Score(void);

/**
  * @brief 游戏计时滴答（中断调用）
  */
void dino_tick(void);

/**
  * @brief 显示地面
  */
void Show_Ground(void);

/**
  * @brief 显示障碍物
  */
void Show_Barrier(void);

/**
  * @brief 显示云朵
  */
void Show_Cloud(void);

/**
  * @brief 显示小恐龙
  */
void Show_Dino(void);

/**
  * @brief 游戏主循环动画
  * @retval 0-游戏结束返回
  */
uint8_t Dino_game_Animation(void);


#endif
