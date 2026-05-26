#ifndef __DINO_H
#define __DINO_H

/**
  * @brief 小恐龙跑酷游戏模块
  * @details Google Dino游戏的STM32移植版
  */

/* ================================================================
 *  游戏参数宏定义
 * ================================================================ */

/* ---- 屏幕几何 ---- */
#define DINO_SCREEN_WIDTH       128     /* OLED 屏幕宽度(px) */
#define DINO_SCREEN_PAGES       8       /* OLED 显存页数 (64px / 8 = 8 pages) */
#define DINO_GROUND_PAGE        7       /* 地面所在的显存 Page 索引 */

/* ---- 恐龙 ---- */
#define DINO_WIDTH              16      /* 恐龙位图宽度(px) */
#define DINO_HEIGHT             18      /* 恐龙位图高度(px) */
#define DINO_X_POS              0       /* 恐龙固定 X 坐标 */
#define DINO_GROUND_Y           44      /* 地面 Y 起始坐标（恐龙/障碍物基准） */
#define DINO_GROUND_Y_END       (DINO_GROUND_Y + DINO_HEIGHT)  /* 地面 Y 结束坐标 */
#define DINO_JUMP_HEIGHT        29      /* 跳跃最大高度(px) */
#define DINO_JUMP_DURATION      1000    /* 跳跃持续时间(ms)，决定正弦曲线周期 */

/* ---- 障碍物 ---- */
#define BARRIER_WIDTH           16      /* 障碍物宽度(px) */
#define BARRIER_HEIGHT          18      /* 障碍物高度(px) */
#define BARRIER_MAX_POS         144     /* 障碍物最大 X 位置，超出后重新生成 */
#define BARRIER_TYPE_COUNT      3       /* 障碍物类型数量 (rand()%3) */

/* ---- 云朵 ---- */
#define CLOUD_WIDTH             16      /* 云朵位图宽度(px) */
#define CLOUD_HEIGHT            8       /* 云朵位图高度(px) */
#define CLOUD_Y_POS             9       /* 云朵 Y 坐标 */
#define CLOUD_MAX_POS           200     /* 云朵最大 X 位置，超出后归零环绕 */

/* ---- 地面纹理 ---- */
#define GROUND_TEXTURE_LEN      256     /* 地面纹理总长度(px) */

/* ---- 定时参数（单位：tick，1 tick = 1ms）---- */
#define SCORE_TICK_PERIOD       100     /* 分数递增周期：每 100ms +1 分 */
#define GROUND_MOVE_PERIOD      20      /* 地面/障碍物移动周期：每 20ms 移 1px */
#define CLOUD_MOVE_PERIOD       50      /* 云朵移动周期：每 50ms 移 1px */

/* ---- UI 布局 ---- */
#define SCORE_DISPLAY_X         96      /* 分数显示 X 坐标 */
#define SCORE_DISPLAY_Y         0       /* 分数显示 Y 坐标 */

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
