# Phase 3 实施方案：页面函数改造为 Task_UI 状态机

## 目标

将 `menu.c` 中 9 个 `while(1)` 独占式页面函数改造为 `Task_UI` 任务内部的**状态机模型**，
所有页面由单一 `Task_UI` 任务统一管理，消除函数嵌套调用。

---

## 一、改造范围总览

| # | 当前函数 | 页面 | 返回类型 | 当前架构 |
|---|---------|------|---------|---------|
| 1 | `First_Page_Clock()` | 首页时钟 | `uint8_t` | while(1), return 1/2 跳转 |
| 2 | `SettingPage()` | 设置页面 | `uint8_t` | while(1), 内嵌套 SetTime_mainprocess() |
| 3 | `Menu_Page()` | 菜单页面 | `uint8_t` | while(1), 内调用 StopClock/flashlight_Func 等 |
| 4 | `StopClock()` | 秒表页面 | `int` | while(1) |
| 5 | `flashlight_Func()` | 手电筒页面 | `int` | while(1) |
| 6 | `MPU6050_Main()` | 传感器页面 | `int` | while(1) |
| 7 | `Game()` | 游戏选择页面 | `int` | while(1), 内调用 Dino_game_Animation() |
| 8 | `Emoji_Func()` | 表情动画页面 | `int` | while(1), 内嵌套 2 个 for 循环 + delay_ms() |
| 9 | `Gradienter_Func()` | 水平仪页面 | `uint8_t` | while(1) |

---

## 二、核心设计

### 2.1 页面枚举定义

在 `menu.h` 中新增：

```c
/* 页面枚举 — Task_UI 状态机 */
typedef enum {
    PAGE_CLOCK = 0,       /* 首页时钟 */
    PAGE_MENU,            /* 菜单页面 */
    PAGE_SETTING,         /* 设置页面 */
    PAGE_STOPWATCH,       /* 秒表 */
    PAGE_FLASHLIGHT,      /* 手电筒 */
    PAGE_MPU6050,         /* 传感器数据 */
    PAGE_GAME_SELECT,     /* 游戏选择 */
    PAGE_DINO_GAME,       /* 恐龙游戏 (运行中) */
    PAGE_EMOJI,           /* 表情动画 */
    PAGE_GRADIENTER,      /* 水平仪 */
    PAGE_SETTIME,         /* 设置时间（方案B：状态机） */
    PAGE_COUNT            /* 页面总数（共11个页面） */
} PageID_t;

/* SetTime 子状态枚举 */
typedef enum {
    SETTIME_MENU = 0,     /* 主选择菜单：返回/年/月/日/时/分/秒 */
    SETTIME_YEAR,
    SETTIME_MONTH,
    SETTIME_DAY,
    SETTIME_HOUR,
    SETTIME_MIN,
    SETTIME_SEC
} SetTimeState_t;

/* 全局当前页面 */
extern volatile PageID_t g_CurrentPage;
```

### 2.2 页面状态结构体

每个有"子状态"的页面（如设置页有"返回/设置时间"两个选项）需要一个状态变量。
直接在 `menu.c` 中用文件级静态变量管理（不与全局冲突，Task_UI 独占）：

```c
/* ---- 各页面子状态变量（Task_UI 独占，无需临界区保护）---- */

/* 首页时钟 */
static uint8_t Clockmoveflag = 1;   /* 1:菜单 2:设置 */

/* 设置页 */
static uint8_t SettingFlag = 1;     /* 1:返回 2:设置时间 */
static uint8_t SettingConfirmed = 0;/* 确认后的选项 */

/* 菜单页 */
static uint8_t MenuFlag = 2;
// 菜单动画变量保持不变：Pre_item, Target_item, Pre_x, move_step, move_stateFlag

/* 秒表 */
static uint8_t StopClock_Flag = 1;

/* 手电筒 */
static uint8_t flashlight_Flag = 1;

/* 游戏选择 */
static uint8_t game_flag = 1;

/* 表情动画 */
static uint8_t emoji_phase = 0;     /* 0:眨眼-闭 1:眨眼-开 2:等待间隔 */
static uint8_t emoji_frame = 0;

/* 水平仪 — 无子状态，仅需帧更新 */
```

### 2.3 Task_UI 主循环结构

在 `freertos.c` 中实现 `Task_UI` 任务函数体：

```c
void Task_UI(void *pvParameters)
{
    uint32_t key;
    (void)pvParameters;

    /* 初始页面：首页时钟 */
    g_CurrentPage = PAGE_CLOCK;

    for (;;)
    {
        /* 等待按键通知（33ms 超时 = 30FPS 帧驱动，有按键时提前唤醒）
         * 注意：ARMCC V5 不定义 ULONG_MAX，使用 0xffffffffUL 替代 */
        BaseType_t gotKey = xTaskNotifyWait(0, 0xffffffffUL, &key, pdMS_TO_TICKS(33));

        /* ---- 全局按键处理 ---- */
        if (gotKey == pdTRUE && key != 0)
        {
            UI_ProcessKey((uint8_t)key);
        }

        /* ---- 帧渲染（无论是否有按键，33ms 超时到即渲染）---- */
        OLED_Clear();
        switch (g_CurrentPage)
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
            default: break;
        }
        OLED_Update();  /* 统一在此更新 OLED，~12ms 阻塞 I2C */
    }
}
```

### 2.4 按键处理函数 `UI_ProcessKey()`

```c
static void UI_ProcessKey(uint8_t key)
{
    switch (g_CurrentPage)
    {
        case PAGE_CLOCK:
            if (key == 1) { /* Key1: 上一个 */
                if (--Clockmoveflag == 0) Clockmoveflag = 2;
            } else if (key == 2) { /* Key2: 下一个 */
                if (++Clockmoveflag == 3) Clockmoveflag = 1;
            } else if (key == 3) { /* Key3 短按: 确认 */
                if (Clockmoveflag == 1) {
                    g_CurrentPage = PAGE_MENU;
                    MenuFlag = 2;  /* 初始化菜单光标 */
                    move_stateFlag = 1;
                } else {
                    g_CurrentPage = PAGE_SETTING;
                    SettingFlag = 1;
                }
            }
            break;

        case PAGE_SETTING:
            // ... 类似改造
            break;

        /* 其余页面类似 */
    }
}
```

---

## 三、各页面改造方案详述

### 3.1 PAGE_CLOCK（首页时钟）

**当前行为：**
- while(1) 轮询 Key_GetNum()
- Key1/Key2 移动光标（Clockmoveflag）
- Key3 确认 → return 1（菜单）或 return 2（设置）
- Key4 长按关机
- 帧率控制 + __WFI()

**改造后：**
- 拆分为 `Render_Clock()`：绘制时钟 UI + 电池 + 反色光标
- 拆分为 `UI_ProcessKey` 中 PAGE_CLOCK 分支：处理按键逻辑
- Key4 长按关机已在 Task_Input 中统一处理，此处不再处理
- 帧率由 `xTaskNotifyWait(33ms)` 统一管理
- `__WFI()` 移除（Idle Hook 统一处理）

**Render_Clock() 函数：**
```c
static void Render_Clock(void)
{
    Show_Clock_UI();
    switch (Clockmoveflag) {
        case 1: OLED_ReverseArea(0,48,32,16);  break;  /* [菜单] */
        case 2: OLED_ReverseArea(96,48,32,16); break;  /* [设置] */
    }
}
```

### 3.2 PAGE_SETTING（设置页面）

**当前行为：**
- while(1) 轮询 Key_GetNum()
- Key3 确认后根据 SettingFlag 决定：return 0 或调用 SetTime_mainprocess()
- SetTime_mainprocess() 内部也是 while(1) 独占

**改造后：**
- 拆分为 `Render_Setting()`：绘制设置 UI + 反色
- SetTime 的 while(1) 需要改造为状态机子状态（见 3.3）
- 新增 `PAGE_SETTIME` 子页面或使用 SettingFlag 子状态区分

**设计决策：SetTime 的处理方式**

SetTime_mainprocess() 内部是 while(1) 独占循环（设置年月日时分秒），有两个方案：

| 方案 | 描述 | 复杂度 |
|------|------|--------|
| A | SetTime 保持独立 while(1)，Task_UI 直接调用（阻塞式） | 低 |
| B | SetTime 也改造为状态机子状态 | 中 |

**最终采用方案 B**，理由：
- 保持 Task_UI 完全不阻塞，符合 RTOS 状态机架构的一致性
- 新增 `PAGE_SETTIME` 页面 + `SetTimeState_t` 子状态枚举
- SetTime.c 中 6 个 Set_*() 函数和 SetTime_mainprocess() 的 while(1) 全部移除
- 边界检查逻辑集中到 `SetTime_BoundaryCheck()` 辅助函数

实际代码（Render_Setting 和 PAGE_SETTING 的 UI_ProcessKey 分支见下方完整实现）。

### 3.3 PAGE_MENU（菜单页面）

**当前行为：**
- while(1) 轮询 Key_GetNum()
- Key1/Key2 移动菜单光标 + 触发滑动动画
- Key3 确认 → 调用 StopClock()/flashlight_Func()/... 等子页面函数
- 菜单动画（Menu_Animation/MenuToFunction_Animation）

**改造后：**
- 拆分为 `Render_Menu()`：调用 Menu_Animation() 绘制菜单动画
- `MenuToFunction_Animation()` 在进入子页面前调用
- 子页面函数（StopClock 等）的调用改为设置 `g_CurrentPage`
- 菜单动画状态变量保留（Pre_item, Target_item, Pre_x, move_step, move_stateFlag, Direct_Flag）

**Render_Menu() 函数：**
```c
static void Render_Menu(void)
{
    /* 如果正在动画中，继续执行动画 */
    if (move_stateFlag == 1) {
        if (MenuFlag == 1) {
            /* 特殊处理：菜单位置1是[返回]，不需要滑动 */
            Menu_Animation();
        } else {
            if (Direct_Flag == 1)
                Set_Selection(1, MenuFlag, MenuFlag-1);
            else if (Direct_Flag == 2)
                Set_Selection(1, MenuFlag-2, MenuFlag-1);
            else
                Menu_Animation();
        }
    } else {
        Menu_Animation();
    }
}
```

UI_ProcessKey PAGE_MENU 分支：
```c
case PAGE_MENU:
    if (key == 1) {
        Direct_Flag = 1;
        move_stateFlag = 1;
        if (--MenuFlag == 0) MenuFlag = MENU_CURSOR_MAX;
    } else if (key == 2) {
        Direct_Flag = 2;
        move_stateFlag = 1;
        if (++MenuFlag == MENU_CURSOR_MAX + 1) MenuFlag = MENU_CURSOR_MIN;
    } else if (key == 3) {
        Direct_Flag = 0;
        OLED_Clear();
        OLED_Update();
        MenuToFunction_Animation();
        OLED_Clear();
        /* 菜单项 → 页面映射 */
        switch (MenuFlag) {
            case 1: g_CurrentPage = PAGE_CLOCK;       break;
            case 2: g_CurrentPage = PAGE_STOPWATCH;   StopClock_Flag = 1; break;
            case 3: g_CurrentPage = PAGE_FLASHLIGHT;  flashlight_Flag = 1; break;
            case 4: g_CurrentPage = PAGE_MPU6050;     break;
            case 5: g_CurrentPage = PAGE_GAME_SELECT; game_flag = 1; break;
            case 6: g_CurrentPage = PAGE_EMOJI;       emoji_phase = 0; emoji_frame = 0; break;
            case 7: g_CurrentPage = PAGE_GRADIENTER;  break;
        }
    }
    break;
```

### 3.4 PAGE_STOPWATCH（秒表）

**当前行为：**
- while(1) 轮询 Key_GetNum()
- Key1/Key2 移动光标（返回/开始/停止/清除 四个按钮）
- Key3 确认 → 执行对应操作或返回
- 计时由 TIM2 ISR 的 StopClock_Tick() 驱动

**改造后：**
- `Render_Stopwatch()`：绘制秒表 UI + 反色
- 子状态变量 StopClock_Flag 保留
- 计时逻辑不变（仍在 TIM2 ISR 中）

```c
static void Render_Stopwatch(void)
{
    Show_StopClock_UI();
    switch (StopClock_Flag) {
        case 1: OLED_ReverseArea(0,0,16,16); break;
        case 2: OLED_ReverseArea(STOPCLK_BTN_START_X, STOPCLK_BTN_Y, STOPCLK_BTN_W, STOPCLK_BTN_H); break;
        case 3: OLED_ReverseArea(STOPCLK_BTN_STOP_X, STOPCLK_BTN_Y, STOPCLK_BTN_W, STOPCLK_BTN_H); break;
        case 4: OLED_ReverseArea(STOPCLK_BTN_CLEAR_X, STOPCLK_BTN_Y, STOPCLK_BTN_W, STOPCLK_BTN_H); break;
    }
}
```

### 3.5 PAGE_FLASHLIGHT（手电筒）

**当前行为：**
- while(1) 轮询 Key_GetNum()
- Key1/Key2 移动光标（返回/OFF/ON）
- Key3 确认 → 执行对应操作或返回

**改造后：**
- `Render_Flashlight()`：绘制 UI + 反色
- 开关逻辑直接在 UI_ProcessKey 中处理

### 3.6 PAGE_MPU6050（传感器数据）

**当前行为：**
- while(1) 轮询 Key_GetNum()
- Key3 确认 → 返回
- 每帧调用 MPU6050_Calculation_Euler_angles() 采样 + 显示

**改造后：**
- `Render_MPU6050()`：调用 MPU6050_Calculation_Euler_angles() + Show_MPU6050_UI() + 反色返回图标
- Key3 → 返回 PAGE_MENU

> **注意：** 当前 Phase 3 不创建 Task_Sensor。MPU6050 采样仍在 Task_UI 中执行，
> 由 Render_MPU6050() 每帧调用一次。Phase 4 再将采样逻辑迁移到独立 Task_Sensor。

### 3.7 PAGE_GAME_SELECT（游戏选择） + PAGE_DINO_GAME（恐龙游戏中）

**当前行为：**
- Game() 中 while(1) 轮询
- Key3 确认 → 进入 Dino_game_Animation()（内部也是 while(1)，碰撞后退出）

**改造后：**
- `Render_GameSelect()`：绘制游戏选择 UI
- 进入游戏时切换到 `PAGE_DINO_GAME`
- `Render_DinoGame()`：每帧调用 Dino_game_Animation() 的一帧逻辑

**关键问题：Dino_game_Animation() 的 while(1) 改造**

Dino_game_Animation() 当前是 while(1) 独占循环，每帧：OLED_Clear → Show_Score → Show_Ground → Show_Barrier → Show_Cloud → Show_Dino → OLED_Update → 碰撞检测。

方案：将 Dino_game_Animation() 的 while(1) 体提取为单帧函数 `Dino_RenderFrame()`，由 Task_UI 每帧调用：

```c
/* 从 Dino_game_Animation() 提取单帧渲染 */
static uint8_t Dino_RenderFrame(void)
{
    Show_Score();
    Show_Ground();
    Show_Barrier();
    Show_Cloud();
    Show_Dino();

    if (isColliding(&Barr, &dino)) {
        Show_GameOver();
        return 1; /* 游戏结束 */
    }
    return 0; /* 继续 */
}
```

Task_UI 中 PAGE_DINO_GAME 渲染：
```c
case PAGE_DINO_GAME:
    if (Dino_RenderFrame()) {
        /* 游戏结束，返回游戏选择页 */
        g_CurrentPage = PAGE_GAME_SELECT;
    }
    break;
```

**注意：** Show_Dino() 内部调用了 Key_GetNum() 检测按键1跳跃。在 FreeRTOS 架构下，
按键由 Task_Input 消费并通过 Notification 转发，Show_Dino() 中的 Key_GetNum() 将始终返回 0。

**需要修改：** 在 UI_ProcessKey 的 PAGE_DINO_GAME 分支中处理 Key1（跳跃）：
```c
case PAGE_DINO_GAME:
    if (key == 3) { /* Key3: 退出游戏 */
        g_CurrentPage = PAGE_GAME_SELECT;
    }
    /* Key1 跳跃通过 dino.c 中的全局变量处理 */
    break;
```

同时修改 dino.c 中 Show_Dino() 的跳跃触发方式：
- 移除 Show_Dino() 中的 Key_GetNum() 调用
- 改用全局标志 `extern uint8_t Dino_JumpRequest;` 
- Task_UI 的 UI_ProcessKey 中检测到 Key1 时设置该标志

### 3.8 PAGE_EMOJI（表情动画）

**当前行为：**
- while(1) 轮询 Key_GetNum()
- Key3 确认 → 返回
- 每帧调用 Show_emoji_UI()（内部有 2 个 for 循环 + delay_ms 阻塞）

**改造后：**
- `Render_Emoji()`：每帧绘制一帧眨眼动画
- Show_emoji_UI() 中的 for 循环改为状态机逐步推进

**Emoji 状态机改造：**

当前 Show_emoji_UI() 的执行流程：
1. 闭眼动画：for(i=0; i<=3; i++) { 绘制第i帧; OLED_Update; delay_ms(100); }
2. 睁眼动画：for(i=0; i<=3; i++) { 绘制第i帧; OLED_Update; delay_ms(100); }
3. delay_ms(500) 等待

改造为每帧推进一个子帧：
```c
static void Render_Emoji(void)
{
    int8_t i;
    switch (emoji_phase) {
        case 0: /* 闭眼阶段 */
            i = emoji_frame;
            OLED_Clear();
            OLED_ShowImage(EMOJI_L_EYEBROW_X, EMOJI_EYEBROW_Y + i, 16, 16, eyebrow[0]);
            OLED_ShowImage(EMOJI_R_EYEBROW_X, EMOJI_EYEBROW_Y + i, 16, 16, eyebrow[1]);
            OLED_DrawEllipse(EMOJI_L_EYE_CX, EMOJI_EYE_CY, EMOJI_EYE_RX, EMOJI_EYE_RY_MAX - i, 1);
            OLED_DrawEllipse(EMOJI_R_EYE_CX, EMOJI_EYE_CY, EMOJI_EYE_RX, EMOJI_EYE_RY_MAX - i, 1);
            OLED_ShowImage(EMOJI_MOUTH_X, EMOJI_MOUTH_Y, EMOJI_MOUTH_W, EMOJI_MOUTH_H, mouth);
            emoji_frame++;
            if (emoji_frame > EMOJI_BLINK_FRAMES) {
                emoji_frame = 0;
                emoji_phase = 1;
            }
            break;
        case 1: /* 睁眼阶段 */
            i = EMOJI_BLINK_FRAMES - emoji_frame;
            OLED_Clear();
            OLED_ShowImage(EMOJI_L_EYEBROW_X, EMOJI_EYEBROW_Y + i, 16, 16, eyebrow[0]);
            OLED_ShowImage(EMOJI_R_EYEBROW_X, EMOJI_EYEBROW_Y + i, 16, 16, eyebrow[1]);
            OLED_DrawEllipse(EMOJI_L_EYE_CX, EMOJI_EYE_CY, EMOJI_EYE_RX, EMOJI_EYE_RY_MAX - EMOJI_BLINK_FRAMES + emoji_frame, 1);
            OLED_DrawEllipse(EMOJI_R_EYE_CX, EMOJI_EYE_CY, EMOJI_EYE_RX, EMOJI_EYE_RY_MAX - EMOJI_BLINK_FRAMES + emoji_frame, 1);
            OLED_ShowImage(EMOJI_MOUTH_X, EMOJI_MOUTH_Y, EMOJI_MOUTH_W, EMOJI_MOUTH_H, mouth);
            emoji_frame++;
            if (emoji_frame > EMOJI_BLINK_FRAMES) {
                emoji_frame = 0;
                emoji_phase = 2;
            }
            break;
        case 2: /* 等待间隔 */
            emoji_frame++; /* 用 frame 计数等待帧数 */
            if (emoji_frame >= (EMOJI_BLINK_GAP_MS / FRAME_PERIOD_MS)) {
                emoji_frame = 0;
                emoji_phase = 0;
            }
            /* 等待期间显示睁眼状态 */
            OLED_Clear();
            OLED_ShowImage(EMOJI_L_EYEBROW_X, EMOJI_EYEBROW_Y, 16, 16, eyebrow[0]);
            OLED_ShowImage(EMOJI_R_EYEBROW_X, EMOJI_EYEBROW_Y, 16, 16, eyebrow[1]);
            OLED_DrawEllipse(EMOJI_L_EYE_CX, EMOJI_EYE_CY, EMOJI_EYE_RX, EMOJI_EYE_RY_MAX, 1);
            OLED_DrawEllipse(EMOJI_R_EYE_CX, EMOJI_EYE_CY, EMOJI_EYE_RX, EMOJI_EYE_RY_MAX, 1);
            OLED_ShowImage(EMOJI_MOUTH_X, EMOJI_MOUTH_Y, EMOJI_MOUTH_W, EMOJI_MOUTH_H, mouth);
            break;
    }
}
```

### 3.9 PAGE_GRADIENTER（水平仪）

**当前行为：**
- while(1) 轮询 Key_GetNum()
- Key3 确认 → 返回
- 每帧调用 Show_Gradienter_UI()

**改造后：**
- `Render_Gradienter()`：调用 Show_Gradienter_UI()
- Key3 → 返回 PAGE_MENU

---

## 四、Idle Hook + __WFI() 统一

### 当前状态

9 个页面函数中各有 `__WFI()`，FreeRTOS 下应该统一在 Idle Hook 中执行。

### 改造

`freertos.c` 中已有 `vApplicationIdleHook()` 的 `__weak` 定义，需要覆盖它：

```c
void vApplicationIdleHook(void)
{
    /* 统一休眠点：替代原来 9 个页面函数中各自的手动 __WFI() */
    __WFI();
}
```

同时在 FreeRTOSConfig.h 中确认 `configUSE_IDLE_HOOK` 已设为 1（Phase 1 已配置 ✅）。

---

## 五、main.c 改造

### 当前状态

```c
/* 裸机超级循环 — while(1) 在 osKernelStart() 之后 */
while (1)
{
    OLED_Clear();
    Battery_Show_UI();
    OLED_Update();
    ClockUI_Move_Flag = First_Page_Clock();
    if(ClockUI_Move_Flag == 1)
        Menu_Page();
    else if(ClockUI_Move_Flag == 2)
        SettingPage();
}
```

### 改造后

```c
/* 创建 Task_UI — 统一页面渲染任务（优先级 2，栈 1280 bytes） */
xTaskCreate(Task_UI, "Task_UI", 320, NULL, 2, &Task_UI_Handle);

/* 启动调度器 */
osKernelStart();

/* 永不返回 */
while (1) {}
```

删除 `extern uint8_t ClockUI_Move_Flag;` 声明（不再需要）。

---

## 六、menu.h 改造

### 新增内容

```c
/* ---- FreeRTOS Task_UI 状态机 ---- */
#include "FreeRTOS.h"
#include "task.h"

/* 页面枚举 */
typedef enum {
    PAGE_CLOCK = 0,
    PAGE_MENU,
    PAGE_SETTING,
    PAGE_STOPWATCH,
    PAGE_FLASHLIGHT,
    PAGE_MPU6050,
    PAGE_GAME_SELECT,
    PAGE_DINO_GAME,
    PAGE_EMOJI,
    PAGE_GRADIENTER,
    PAGE_COUNT
} PageID_t;

/* 全局当前页面 */
extern volatile PageID_t g_CurrentPage;

/* Task_UI 任务句柄（供 Task_Input 发送通知） */
extern TaskHandle_t Task_UI_Handle;

/* Task_UI 任务函数 */
void Task_UI(void *pvParameters);
```

### 删除内容

`First_Page_Clock()`、`SettingPage()`、`Menu_Page()`、`StopClock()`、`flashlight_Func()`、
`MPU6050_Main()`、`Game()`、`Emoji_Func()`、`Gradienter_Func()` 的函数声明全部删除。

### 保留内容

所有 `Show_*_UI()` 渲染函数声明保留（它们现在是 Render_* 的底层绘制调用）。
`Battery_Show_UI()`、`StopClock_Tick()`、`MPU6050_Calculation_Euler_angles()` 保留。

> **注意：** `SetTime_mainprocess()` 已删除（方案 B — SetTime 改为状态机，`menu.c` 中 Render_SetTime/UI_ProcessKey 替代）。
`Set_Year/Month/Day/Hour/Min/Sec` 共 6 个函数一并删除。

---

## 七、文件变更清单

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `menu.h` | **修改** | 新增 PageID_t 枚举、g_CurrentPage extern、Task_UI_Handle extern、Task_UI 声明；删除 9 个旧页面函数声明 |
| `menu.c` | **大幅修改** | 9 个 while(1) 函数改为 Render_* + UI_ProcessKey 分支；新增 g_CurrentPage/Task_UI_Handle 全局定义；删除 __WFI()；保留所有 Show_*_UI 绘制函数 |
| `main.c` | **修改** | 删除裸机超级循环，替换为 xTaskCreate(Task_UI)；删除 ClockUI_Move_Flag extern |
| `freertos.c` | **修改** | 新增 Task_UI() 主循环（委托 menu.c 的 Task_UI_RenderFrame）；覆盖 vApplicationIdleHook() 添加 __WFI()；Task_Input 通知转发 |
| `Key.h` | **修改** | 新增 `Task_UI_Handle` extern 声明 |
| `dino.c` | **修改** | 提取 Dino_RenderFrame() 单帧函数；Show_Dino() 移除 Key_GetNum()，改用 Dino_JumpRequest 标志 |
| `dino.h` | **修改** | 新增 Dino_RenderFrame() 声明；新增 Dino_JumpRequest extern |
| `SetTime.c` | **修改** | 删除 Set_Year/Month/Day/Hour/Min/Sec + SetTime_mainprocess 共 7 个函数；保留 Show_SetDate_UI/Show_SetTime_UI/ChangeRTC_Time |
| `SetTime.h` | **修改** | 删除 7 个旧函数声明；保留 Show_SetDate_UI/Show_SetTime_UI/ChangeRTC_Time |
| `stm32f1xx_it.c` | **修改** | 新增 `#include "Hardware/dino.h"`（修复 dino_tick 隐式声明警告） |

---

## 八、实施步骤

| 步骤 | 内容 | 关键检查点 |
|------|------|-----------|
| **Step 1** | 修改 `menu.h`：新增枚举、extern 声明、删除旧函数声明 | 编译通过 |
| **Step 2** | 修改 `menu.c`：删除 9 个 while(1) 函数，添加 g_CurrentPage/Task_UI_Handle 定义 | 编译通过 |
| **Step 3** | 修改 `menu.c`：实现 10 个 Render_* 函数 | 编译通过 |
| **Step 4** | 修改 `freertos.c`：实现 Task_UI() + UI_ProcessKey() | 编译通过 |
| **Step 5** | 修改 `freertos.c`：覆盖 vApplicationIdleHook() + Task_Input 通知转发 | 编译通过 |
| **Step 6** | 修改 `main.c`：删除裸机循环，创建 Task_UI | 编译通过 |
| **Step 7** | 修改 `dino.c`/`dino.h`：提取单帧渲染 + 跳跃标志 | 编译通过 |
| **Step 8** | 修改 `Key.h`：新增 Task_UI_Handle extern | 编译通过 |
| **Step 9** | 全工程编译验证（ARMCC V5.06, 0 Error, 0 Warning） | ⚠️ 关键 |
| **Step 10** | 烧录测试：所有 9 个页面切换、按键响应、恐龙游戏 | ⚠️ 关键 |

---

## 九、风险点与注意事项

| 风险 | 等级 | 缓解措施 |
|------|------|---------|
| Emoji 动画时序变化 | 🟡 中 | 原 delay_ms(100) 现在每 33ms 渲染一帧，速度不同，需调整帧计数或增加等待帧数 |
| Dino 游戏按键响应 | 🟡 中 | 原 Show_Dino() 内联检测按键，现改为 Notification 转发 + Dino_JumpRequest 标志，最迟 33ms 响应 |
| SetTime 边界检查 | 🟡 中 | 逐一对照原 Set_*() 函数中的边界逻辑迁移到 UI_ProcessKey 的 PAGE_SETTIME 分支 |
| 菜单动画变量初始化 | 🟡 中 | 从菜单进入子页面再返回时，需确保动画状态变量正确重置 |
| 栈溢出 | 🟡 中 | Task_UI 栈 320 words (1280 bytes)，需在 Phase 5 用 uxTaskGetStackHighWaterMark() 验证 |
| 全局变量冲突 | 🟢 低 | 各页面子状态变量改为 static，限定在 menu.c 文件作用域 |

---

## 十、不做的事情

- ❌ 不创建 Task_Sensor（Phase 4 的工作）
- ❌ 不修改 OLED/MyI2C 驱动
- ❌ 不修改 TIM2 ISR（已包含 vTaskNotifyGiveFromISR）
- ❌ 不修改 Key.c（Key_GetNum 临界区已在 Phase 2 升级）
- ❌ SetTime.c 保留 3 个底层函数（Show_SetDate_UI / Show_SetTime_UI / ChangeRTC_Time），删除 7 个 while(1) 函数（逻辑移入 menu.c 状态机）