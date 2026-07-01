# FreeRTOS 移植记录

> **评估日期：** 2026-06-14（可行性分析）
> **Phase 1 实施日期：** 2026-06-16
> **Phase 2 实施日期：** 2026-06-22
> **Phase 3 实施日期：** 2026-06-23
> **Phase 4 实施日期：** 2026-06-25（待提交）
> **任务划分分析日期：** 2026-06-21
> **编译验证：** ✅ ARMCC V5.06, 0 Error, 0 Warning
> **目标 MCU：** STM32F103RBT6 (Cortex-M3)
> **FreeRTOS 版本：** V10.3.1 (CMSIS_V1)
> **当前工程：** SmartWatch_HAL (HAL 库版本)

---

## 目录

- [项目概览](#项目概览)
- [已具备的兼容条件](#一已具备的-freeertos-兼容条件)
- [需要解决的关键问题](#二需要解决的关键问题)
- [不需要修改的部分](#三不需要修改的部分)
- [推荐移植方案](#四推荐移植方案与工作量)
  - [4.1 任务划分方案](#41-任务划分方案)
  - [4.2 分阶段实施](#42-分阶段实施)
  - [4.3 总预估](#43-总预估)
- [Phase 1 实施记录](#五phase-1-实施记录--cubemx-集成-freertos)
- [Phase 1 遇到的问题](#六phase-1-遇到的问题与解决方案)
- [Phase 2 实施记录](#六续phase-2-实施记录--创建-task_input)
- [Phase 3 实施记录](#phase-3-实施记录--task_ui-统一页面渲染状态机)
- [Phase 4 实施记录](#phase-4-实施记录--task_sensor-mpu6050-独立后台采样)
- [结论](#七结论)
- [问题整理归纳](#八问题整理归纳)
  - [问题 5：ULONG_MAX 未定义](#问题-5x-tasknotifywait-中-ulong_max-未定义)
  - [问题 6：采样周期双重延时](#问题-6mpu6050_calculation_euler_angles-内部双重延时导致采样周期翻倍)
  - [问题 7：Game Over 阻塞 1 秒](#问题-7show_gameover-中-delay_ms1000-阻塞-task_ui-长达-1-秒)
  - [问题 8：Game Over 重复 I2C 传输](#问题-8show_gameover-中-oldupdate-与-task_ui-重复调用导致单帧-i2c-翻倍)
  - [问题 9：OLED I2C 被抢占 — vTaskSuspendAll 保护 I2C 时序](#问题-9oled-i2c-被抢占--vtasksuspendall-保护-i2c-时序)
  - [问题 10：恐龙游戏碰撞后无法二次进入 — GameOver_Countdown 未重置](#问题-10恐龙游戏碰撞后无法二次进入--dinogameovercountdown-未重置--渲染顺序缺陷)

---

## 项目概览

| 项目 | 详情 |
|------|------|
| MCU | STM32F103RBT6 (Cortex-M3) |
| 主频 | **72MHz** (HSE 8MHz × PLL9) |
| Flash | 128KB |
| SRAM | **20KB** |
| 编译器 | ARMCC V5.06 |
| 显示 | SSD1306 OLED 128×64 (软件 I2C) |
| 传感器 | MPU6050 六轴 (软件 I2C) |
| 当前架构 | 裸机超级循环 (Super Loop) + TIM2 1ms 中断 |
| 源码路径 | `SmartWatch_HAL/HAL/` |

---

## 一、已具备的 FreeRTOS 兼容条件

### 1.1 DWT 微秒延时 — 零冲突 ✅

**涉及文件：** [delay.c](../SmartWatch_HAL/HAL/Core/Src/delay.c)、[delay.h](../SmartWatch_HAL/HAL/Core/Inc/delay.h)

`delay_us()` 使用 Cortex-M3 内核 DWT CYCCNT 周期计数器实现，**不占用 SysTick**，硬件级精确延时。

```c
void delay_us(uint32_t us)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks);
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
}
```

| 特性 | 说明 |
|------|------|
| SysTick 占用 | ❌ 不占用，FreeRTOS 可使用 SysTick 作 tick 源 |
| 功耗 | 用完即关，空闲零额外功耗 |
| 自动适配 | 使用 `SystemCoreClock`，自动适配时钟频率 |
| 可重入 | 纯查询无锁，可在 ISR 中调用 |

> 已通过问题十四修复验证。文档中也明确标注了"与 FreeRTOS 兼容"。

---

### 1.2 系统时钟 72MHz ✅

**涉及文件：** [main.c:143-185](../SmartWatch_HAL/HAL/Core/Src/main.c#L143-L185)

问题六修复后，系统时钟已从 8MHz HSI 恢复为 72MHz PLL：

| 时钟路径 | 值 | 说明 |
|---------|-----|------|
| HSE | 8MHz | 外部晶振 |
| PLL | ×9 (72MHz) | HSE → PLLSRC → PLLMUL |
| SYSCLK | 72MHz | PLLCLK |
| HCLK (AHB) | 72MHz | /1 |
| APB1 (TIM2) | 36MHz | /2 (→ ×2 倍频 = 72MHz TIM 时钟) |
| APB2 | 72MHz | /1 |

72MHz 主频确保 FreeRTOS 的 tick 中断处理（1ms 周期）开销占比极小。

---

### 1.3 HAL 驱动框架原生支持

**涉及文件：** [stm32f1xx_hal_conf.h](../SmartWatch_HAL/HAL/Core/Inc/stm32f1xx_hal_conf.h)

CubeMX 生成的 HAL 库支持 FreeRTOS 集成。关键配置项（第 133 行）：

```c
#define USE_RTOS                     0U    /* HAL 内部 RTOS 适配开关 */
```

**重要：** 本项目使用的 STM32F1 HAL 驱动库（2016 年版）中 `USE_RTOS` **只能设为 `0U`**。
设为 `1U` 会触发编译错误 `#error "USE_RTOS should be 0 in the current HAL release"`，
因为 2016 年版 HAL 尚未实现 RTOS 版本的 `__HAL_LOCK` / `__HAL_UNLOCK` 宏（计划"Reserved for future use"）。
对本项目无实际影响——每个外设由单任务独占使用，裸机版自旋锁完全够用。
详见 [问题 2：USE_RTOS 编译错误](#问题-2编译错误-user_tos--1u-触发-error)。

HAL 库提供完整的 `HAL_Delay()` 和 `HAL_GetTick()` 抽象层，迁移时只需在 SysTick 中断中增加 OS tick 调用。

---

### 1.4 临界区保护模式已建立 ✅ → FreeRTOS 升级完成 ✅

**涉及文件：** [Key.c](../SmartWatch_HAL/HAL/Core/Src/Hardware/Key.c) — `Key_GetNum()`

问题十修复后，`Key_GetNum()` 已使用 `__disable_irq()` / `__enable_irq()` 保护读-改-写临界区。
Phase 2 已升级为 FreeRTOS 版本，同时修复了原裸机版的临界区配对 bug（两个 `__enable_irq()` 出口，FreeRTOS 嵌套计数器下会溢出）：

```c
// Phase 2 升级后（FreeRTOS 版本）：
uint8_t Key_GetNum(void)
{
    uint8_t Temp = 0;
    taskENTER_CRITICAL();       /* 使用 BASEPRI 屏蔽，嵌套安全 */
    if(Key_Num) {
        Temp = Key_Num;
        Key_Num = 0;
    }
    taskEXIT_CRITICAL();        /* 统一出口，一次 ENTER 配一次 EXIT */
    return Temp;
}
```

| 项目 | 裸机版 | FreeRTOS 版 |
|------|--------|------------|
| 屏蔽方式 | `__disable_irq()` (PRIMASK) | `taskENTER_CRITICAL()` (BASEPRI) |
| 配对 | 1 disable : 2 enable (bug) | 1 enter : 1 exit (正确) |
| ISR 响应 | 全部屏蔽 | 仅屏蔽受控范围 (BASEPRI=0x30) |

---

### 1.5 帧率控制 + __WFI() 休眠 ✅

**涉及文件：** [menu.c](../SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c)、[menu.h](../SmartWatch_HAL/HAL/Core/Inc/Hardware/menu.h)

问题十五修复后，9 个页面函数已统一添加帧率控制（30FPS），6 个缺少 `__WFI()` 的函数已补齐休眠点：

```c
#define FRAME_PERIOD_MS         33      /* 帧率控制周期(ms)，约30FPS */
static uint32_t last_draw_tick = 0;

// 页面循环内：
if(HAL_GetTick() - last_draw_tick >= FRAME_PERIOD_MS)
{
    last_draw_tick = HAL_GetTick();
    // 绘制逻辑
}
__WFI();    // 等待中断唤醒
```

FreeRTOS 下可迁移为：
- 所有页面合并到 `Task_UI` 统一任务（页面互斥，不需要并行）
- `xTaskNotifyWait()` 33ms 超时替代帧率控制 + 按键等待
- `vApplicationIdleHook()` 中统一执行 `__WFI()`

---

## 二、需要解决的关键问题

### 2.1 🔴 阻塞式软件 I2C — 最高风险

**涉及文件：** [OLED.c](../SmartWatch_HAL/HAL/Core/Src/Hardware/OLED.c)、[MyI2C.c](../SmartWatch_HAL/HAL/Core/Src/MyI2C.c)

软件 I2C 是 CPU 100% 忙等的 bit-banging 操作。问题十四添加 NOP/delay 延时后，时序合规但阻塞时间更长：

| 场景 | I2C 字节数 | 阻塞时间 | 频率 |
|------|-----------|---------|------|
| `OLED_Update()` 全屏刷新 | ~1040 | **~12ms** | 30 FPS |
| `OLED_UpdateArea()` 局部刷新 | ~768 | **~9ms** | 动画期间 |
| MPU6050 读取 6 轴 | ~30 | **~1ms** | 每次传感器读取 |

**风险分析：**
- 12ms 的 CPU 忙等意味着所有低优先级任务在此期间无法运行
- 如果 I2C 任务被更高优先级任务抢占，I2C 时序会被破坏 → **通信失败**

**建议方案：**

| 方案 | 工作量 | 优点 | 缺点 |
|------|--------|------|------|
| **A（短期推荐）** | 小 | I2C 操作集中在**高优先级专用任务**，配合 `vTaskDelay()` 帧率控制 | 仍为阻塞式，任务切换时无法抢占 |
| **B（中期推荐）** | 中 | 迁移到**硬件 I2C 外设** (STM32F103 有 I2C1/I2C2)，利用 DMA + 中断 | 需要重写 OLED/MyI2C 驱动底层 |
| **C（不推荐）** | 小 | 降低 I2C 速度 + 分段传输，每次发送后 `taskYIELD()` | 复杂且不可靠 |

---

### 2.2 🟡 页面函数架构 — while(1) 独占

当前所有页面函数采用"独占式" `while(1)` 循环架构：

```c
// 当前架构：每个页面函数占据 CPU 直到用户切换
uint8_t First_Page_Clock(void)
{
    while(1)
    {
        KeyNum = Key_GetNum();
        // ... 按键处理 ...
        if(HAL_GetTick() - last_draw_tick >= FRAME_PERIOD_MS)
        {
            last_draw_tick = HAL_GetTick();
            Show_Clock_UI();
            OLED_Update();
        }
        __WFI();
    }
}
```

**受影响的所有页面函数（共 9 个）：**

| 函数 | 页面 | 当前行为 |
|------|------|---------|
| `First_Page_Clock()` | 首页时钟 | while(1) 独占 |
| `SettingPage()` | 设置页面 | while(1) 独占 |
| `Menu_Page()` | 菜单页面 | while(1) 独占 |
| `StopClock()` | 秒表页面 | while(1) 独占 |
| `flashlight_Func()` | 手电筒页面 | while(1) 独占 |
| `MPU6050_Main()` | 传感器页面 | while(1) 独占 |
| `Game()` | 游戏选择页面 | while(1) 独占 |
| `Emoji_Func()` | 表情动画页面 | while(1) 独占 |
| `Gradienter_Func()` | 水平仪页面 | while(1) 独占 |

**改造方案：**

将所有页面函数改造为 `Task_UI` 任务内部的**状态机**模型：

```c
// FreeRTOS 架构：Task_UI 统一管理所有页面
void Task_UI(void *pvParameters)
{
    uint32_t key;
    while(1)
    {
        // 等待按键通知（33ms 超时 = 30FPS 帧驱动）
        if(xTaskNotifyWait(0, ULONG_MAX, &key, pdMS_TO_TICKS(33)) == pdTRUE)
        {
            UI_ProcessKey(key);     // 处理按键 → 修改 g_CurrentPage
        }

        // 帧率控制 + 渲染
        switch(g_CurrentPage)
        {
            case PAGE_CLOCK:      Render_Clock();       break;
            case PAGE_MENU:       Render_Menu();        break;
            case PAGE_SETTING:    Render_Setting();     break;
            case PAGE_STOPWATCH:  Render_Stopwatch();   break;
            // ... 其余页面
        }
        OLED_Update();  // 统一在此更新 OLED
    }
}
```

| 项目 | 当前架构 | FreeRTOS 架构 |
|------|---------|--------------|
| 页面切换 | 函数嵌套调用 | `g_CurrentPage` 赋值 |
| 按键获取 | `Key_GetNum()` 轮询 | `xTaskNotifyWait()` 阻塞等待（经 Task_Input 转发） |
| 帧率控制 | `HAL_GetTick()` 轮询 | `xTaskNotifyWait()` 33ms 超时 |
| 休眠 | 9 个页面中各一个 `__WFI()` | `vApplicationIdleHook()` 中统一 `__WFI()` |
| OLED 访问 | 各页面各自调用 | Task_UI 独占，无需互斥锁 |

> **为什么不是一个页面一个任务：** 页面之间互斥（用户同时只看一个），不需要并行运行；
> 软件 I2C 不可抢占，多任务共享 OLED 需要互斥锁且会引发优先级反转。
> 将互斥的操作放在同一任务中，从设计上消除锁的需求。

---

### 2.3 🟡 TIM2 ISR 回调 — 1ms 高频率中断

**涉及文件：** [stm32f1xx_it.c:206-218](../SmartWatch_HAL/HAL/Core/Src/stm32f1xx_it.c#L206-L218)

TIM2 以 **1ms** 周期调用 4 个回调函数：

```c
void TIM2_IRQHandler(void)
{
    Key3_Tick();       // 按键消抖
    KeyTick();         // 按键扫描
    StopClock_Tick();  // 秒表计时
    dino_tick();       // 恐龙游戏障碍物移动
    HAL_TIM_IRQHandler(&htim2);
}
```

| 回调 | 功能 | 执行时间 | FreeRTOS 兼容 |
|------|------|---------|-------------|
| `Key3_Tick()` | 按键 3 消抖状态机 | ~1-2μs | ✅ 可在 ISR 中执行 |
| `KeyTick()` | 按键 1/2 消抖状态机 | ~1-2μs | ✅ 可在 ISR 中执行 |
| `StopClock_Tick()` | 秒表计时累加 | ~0.5μs | ✅ 可在 ISR 中执行 |
| `dino_tick()` | 恐龙游戏障碍物/分数更新 | ~1μs | ✅ 可在 ISR 中执行 |

**影响分析：**
- 每个回调执行时间极短（微秒级），直接在 ISR 中执行是安全的
- FreeRTOS 的 tick 也是 1ms，两个 1ms 中断会增加 ~0.1% 的上下文切换开销
- 可以使用 `vTaskNotifyGiveFromISR()` 在 ISR 中通知任务

**建议：** 保持 TIM2 ISR 中执行短回调不变，暂不需要迁移到软件定时器。

---

### 2.4 🟡 HAL 的 SysTick 与 FreeRTOS 的 SysTick 共享 — 已解决 ✅

**涉及文件：** [stm32f1xx_it.c:185-194](../SmartWatch_HAL/HAL/Core/Src/stm32f1xx_it.c#L185-L194)

当前 SysTick 仅用于 `HAL_IncTick()`：

```c
void SysTick_Handler(void)
{
    HAL_IncTick();
}
```

FreeRTOS 默认也使用 SysTick 作为系统 tick 源，需要在同一个 ISR 中同时服务两者。

**实际采用方案：SysTick 共享（原方案 B）**

DWT 延时（`delay_us()`）不占用 SysTick，SysTick 完全空闲。
FreeRTOS 使用 SysTick 是最标准的做法，且能省下 TIM3 外设。

**实现方式：**

```c
void SysTick_Handler(void)
{
    HAL_IncTick();              // HAL 的 tick（uwTick++）
#if (INCLUDE_xTaskGetSchedulerState == 1)
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
#endif
    xPortSysTickHandler();      // FreeRTOS 的 tick
#if (INCLUDE_xTaskGetSchedulerState == 1)
    }
#endif
}
```

带 `xTaskGetSchedulerState()` 保护，确保调度器未启动时不会调用 FreeRTOS API（避免 HardFault）。

**为什么没有采用原方案 A（TIM3）：**
- SysTick 方案是 FreeRTOS 标准做法，兼容性最好
- 省下一个 TIM 外设（TIM3 可留给未来其他功能）
- DWT 延时已确保 SysTick 完全空闲，不存在冲突
- CubeMX 原生支持，无需额外配置 TIM3

> 原可行性分析中方案 A 被标注为"推荐"，实际执行中经过讨论决定采用方案 B，
> 理由如上。两者均可正常工作，方案 B 对硬件资源利用更优。

---

### 2.5 🟡 SRAM 20KB — 资源紧张

**SRAM 使用预估（3 用户任务方案）：**

| 项目 | 预估用量 |
|------|---------|
| FreeRTOS heap（5 任务 + TCB + 余量） | ~4.3 KB |
| 全局变量 (`OLED_DisplayBuf[8][128]` 等) | ~5 KB |
| HAL 缓冲区 + startup 栈 | ~1.5 KB |
| **总计预估** | **~10.8 KB** |

20KB SRAM 中 ~10.8KB 已用，余量 ~9.2KB。关键策略：

| 策略 | 说明 |
|------|------|
| 精确配置栈大小 | 使用 `uxTaskGetStackHighWaterMark()` 调优 |
| 避免递归 | 所有函数禁止递归调用 |
| 减少大数组 | 检查是否有不必要的全局缓冲区 |
| OLED 显存 | `OLED_DisplayBuf[8][128]` 在 MCU SRAM 中（1KB），已计入全局变量；SSD1306 内部 GDDRAM 仅作 I2C 传输目标，不额外占 MCU 内存 |
| 任务数精简 | 3 个用户任务 vs 初版 13 个，直接省 ~5.3KB heap |

---

### 2.6 🟢 HAL_Delay() 需要替换 — 已确认无需处理 ✅

代码中**不存在** `HAL_Delay()` 调用。项目所有延时均使用 DWT 的 `delay_us()` / `delay_ms()`，
完全不依赖 SysTick 轮询，与 FreeRTOS 零冲突。

> 已通过 grep 全工程验证：0 处 `HAL_Delay(` 调用。

---

## 三、不需要修改的部分

| 模块 | 原因 |
|------|------|
| DWT 延时 (`delay.c`) | 不依赖 OS，直接操作内核寄存器 |
| OLED 驱动 (`OLED.c`) | 软件 I2C 时序与 OS 无关（只需确保 ISR 不抢占） |
| OLED 字库 (`OLED_Data.c/.h`) | 纯数据，无执行逻辑 |
| MyI2C (`MyI2C.c`) | 纯软件时序，与 OS 无关 |
| MPU6050 驱动 | 仅调用 MyI2C + DWT delay |
| MyRTC | RTC 是硬件外设，与 OS 无关 |
| GPIO 配置 (`MX_GPIO_Init()`) | CubeMX 生成，与 OS 无关 |
| ADC 电池检测 | 单次转换，与 OS 无关 |
| 菜单数据 (`menu.h` 宏定义) | 仅常量和宏，无执行逻辑 |
| 所有 `.h` 头文件的函数声明 | 声明与 OS 无关 |

---

## 四、推荐移植方案与工作量

### 4.1 任务划分方案

> **设计原则：** 任务划分的依据不是"有几个页面"，而是"哪些事情需要同时做"。
> 互斥的操作放在同一任务（消除锁的需求），并行的操作才拆分为独立任务。

#### 4.1.1 为什么不是"每个页面一个任务"

文档初版曾规划 13 个任务（每个页面一个 + Task_Input + Idle/Timer），经深入分析后否定该方案，
原因如下：

| 约束 | 分析 | 结论 |
|:---|:---|:---|
| **页面互斥** | 用户同一时刻只看一个页面。时钟/菜单/秒表/游戏不会同时运行 | 不需要并行 → 不需要独立任务 |
| **软件 I2C 不可抢占** | `OLED_Update()` 是 12ms CPU 忙等。如果两个任务都要操作 OLED，必须用互斥锁——但高优先级任务会被正在做 I2C 的低优先级任务阻塞 12ms（优先级反转） | 单任务独占 OLED → 根本不需要锁 |
| **SRAM 20KB 紧张** | 每增加一个任务 = ~80B TCB + N×4B 栈。13 任务 ≈ 9.6KB heap | 减少任务数直接省内存 |

**核心认知：** 把裸机 while(1) 一对一映射为 FreeRTOS task 是过渡阶段的自然思维，
但 RTOS 的真正价值在于"需要并行时才拆任务"，而非"有几个函数就建几个任务"。

#### 4.1.2 最终任务划分（3 个用户任务）

```
FreeRTOS 架构:

main()
├── HAL_Init()
├── SystemClock_Config()
├── MX_GPIO_Init() / ... / MX_RTC_Init()
├── Key_Init() / OLED_Init() / MPU6050_Init() / MyRTC_Init()
├── 创建 3 个用户任务
│   ├── Task_Input   (优先级 3, 栈 384B)   ← 按键接收 + 路由
│   ├── Task_UI      (优先级 2, 栈 1280B)  ← 全部 9 个页面渲染
│   └── Task_Sensor  (优先级 1, 栈 512B)   ← MPU6050 后台连续采样
├── vTaskStartScheduler()
└── (永不返回)

TIM2_IRQHandler (1ms)
├── Key3_Tick()
├── KeyTick()                         ← ISR 中检测按键
├── StopClock_Tick()
├── dino_tick()
└── vTaskNotifyGiveFromISR()          ← 通知 Task_Input

空闲任务
└── vApplicationIdleHook() → __WFI()  ← 统一休眠点
```

#### 4.1.3 各任务详细说明

**Task_Input — 按键处理任务（优先级 3，最高用户任务）**

| 属性 | 值 | 理由 |
|:---|:---|:---|
| 优先级 | 3 | 按键响应是用户体验核心，不能被 UI 渲染阻塞 |
| 栈大小 | 384 bytes (96 words) | 仅 Key_GetNum() + xTaskNotify()，无深层调用 |
| 阻塞方式 | `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)` | 无限阻塞，零 CPU，ISR 通知唤醒 |
| 通信 | TIM2 ISR → Task Notification → Task_Input → Task Notification → Task_UI | 全链路 Task Notification，零额外 RAM |

按键是**异步事件**，天然适合事件驱动模型。Task_Input 平时完全阻塞，
按键到来时才被 ISR 唤醒。优先级 3 确保即使 Task_UI 正在执行 12ms 的 OLED I2C 传输，
ISR 也能正常通知 Task_Input（通知在 ISR 中排队，退出 ISR 后投递）。

全局按键（Key3 长按关机）在此统一处理，不依赖任何页面。

**Task_UI — 统一页面渲染任务（优先级 2）**

| 属性 | 值 | 理由 |
|:---|:---|:---|
| 优先级 | 2 | 低于 Task_Input（可被按键抢占），高于 Task_Sensor |
| 栈大小 | 1280 bytes (320 words) | 覆盖所有页面函数 + OLED 驱动 + SetTime 嵌套调用 |
| 阻塞方式 | `xTaskNotifyWait(0, ULONG_MAX, &key, pdMS_TO_TICKS(33))` | 33ms 超时 = 30FPS 帧驱动；有按键时提前唤醒 |
| 拥有资源 | OLED 帧缓冲 + OLED I2C 总线 (PB8/PB9) | 独占，无需互斥锁 |

将 9 个页面函数改造为 Task_UI 内部的**状态机**：

```
Task_UI 主循环:
  while(1) {
      xTaskNotifyWait(0, ULONG_MAX, &key, pdMS_TO_TICKS(33));  // 阻塞等待按键或 33ms 超时
      
      if (有按键) UI_ProcessKey(key);  // 修改 g_CurrentPage / 子状态
      
      switch(g_CurrentPage) {
          case PAGE_CLOCK:      Render_Clock();       break;
          case PAGE_MENU:       Render_Menu();        break;
          case PAGE_SETTING:    Render_Setting();     break;
          case PAGE_STOPWATCH:  Render_Stopwatch();   break;
          case PAGE_FLASHLIGHT: Render_Flashlight();  break;
          case PAGE_MPU6050:    Render_MPU6050();     break;
          case PAGE_GAME_SELECT:Render_GameSelect();  break;
          case PAGE_DINO:       Render_DinoGame();    break;
          case PAGE_EMOJI:      Render_Emoji();       break;
          case PAGE_GRADIENTER: Render_Gradienter();  break;
      }
      OLED_Update();  // ~12ms 阻塞 I2C
  }
```

页面切换只需 `g_CurrentPage = PAGE_MENU` 一行代码，无需 `vTaskSuspend()`/`vTaskResume()` 的复杂同步。

**为什么恐龙游戏不独立成任务：** 游戏的时序驱动来自 TIM2 ISR 的 `dino_tick()`（1ms 更新分数/位置/跳跃），
渲染循环只是读取并绘制。30FPS 渲染完全够用——游戏逻辑仍然是 1000Hz 精度。独立任务需要互斥锁保护 OLED，
反而引入优先级反转问题。

**为什么秒表不独立成任务：** 计时由 `StopClock_Tick()` 在 TIM2 ISR 中完成，Task_UI 只需每 33ms 读取显示，
与独立任务效果一致，省一个 TCB + 512B 栈。

**Task_Sensor — MPU6050 后台采样任务（优先级 1）**

| 属性 | 值 | 理由 |
|:---|:---|:---|
| 优先级 | 1（最低用户任务） | 传感器采样不紧急，不应抢占 UI 渲染 |
| 栈大小 | 512 bytes (128 words) | MPU6050_GetData + 互补滤波 + I2C 通信 |
| 阻塞方式 | `vTaskDelay(pdMS_TO_TICKS(5))` | 5ms 采样间隔，释放 CPU |
| 拥有资源 | MPU6050 I2C 总线 (PB10/PB11) | 独占，无需互斥锁 |

独立采样任务的核心价值：

- **解耦采样与显示。** 当前代码中 `MPU6050_Calculation_Euler_angles()` 嵌入在 `MPU6050_Main()` 和
  `Gradienter_Func()` 各自的 while(1) 中。切换到水平仪页面时，Euler 角需重新收敛；离开 MPU6050 页面后，
  数据停止更新。独立任务让传感器**持续采样**，无论当前显示哪个页面，数据始终最新。
- **互补滤波器需要连续数据。** 互补滤波（α=0.9）依赖连续的时间序列积分，采样被页面切换打断会导致抖动。
- **独立 I2C 总线。** MPU6050 使用 PB10/PB11，与 OLED (PB8/PB9) 物理隔离。Task_Sensor (Prio 1)
  的 ~1ms I2C 传输可能被 Task_UI (Prio 2) 的 12ms OLED 传输抢占——这完全可接受，偶发的采样延迟
  不影响数据质量。
- **不放在 Task_UI 中。** 如果 Task_UI 既渲染又采样，两个操作串行化，帧率从 30FPS 降至 ~15FPS。
  独立任务让两者并行。

#### 4.1.4 资源共享与同步策略

```
OLED_DisplayBuf[8][128] + OLED I2C (PB8/PB9)
    └── 拥有者: Task_UI (独占，无需锁)

MPU6050 I2C (PB10/PB11)
    └── 拥有者: Task_Sensor (独占，无需锁)

g_Roll, g_Pitch, g_Yaw (Euler 角)
    ├── 写入者: Task_Sensor (taskENTER_CRITICAL 保护)
    └── 读取者: Task_UI (volatile 读取)

MyRTC_Time[6]
    ├── 写入者: Task_UI (SetTime 子状态)
    └── 读取者: Task_UI (时钟页面)
        → 同一任务，无需保护

Key_Num (按键键码)
    ├── 写入者: TIM2 ISR (KeyTick)
    └── 读取者: Task_Input (taskENTER_CRITICAL 保护)

Dino 游戏状态 (score, pos, ...)
    ├── 写入者: TIM2 ISR (dino_tick)
    └── 读取者: Task_UI (volatile 读取)
        → ISR 写入 32-bit 单字是原子的，无需锁
```

**设计原则：每个共享资源只有一个写入者任务**（或 ISR），将并发冲突从"用锁解决"降维为"设计上不存在"。

#### 4.1.5 内存预算

| 项目 | 大小 | 累计 |
|:---|:---|:---|
| Idle Task 栈 | 512 B | 512 |
| Timer Task 栈 | 1,024 B | 1,536 |
| Task_Input 栈 | 384 B | 1,920 |
| Task_UI 栈 | 1,280 B | 3,200 |
| Task_Sensor 栈 | 512 B | 3,712 |
| 5 个 TCB（~80B/个） | ~400 B | 4,112 |
| 队列/互斥量 | ~200 B | 4,312 |
| **FreeRTOS heap 使用** | **~4.3 KB** | |
| **configTOTAL_HEAP_SIZE** | **10 KB** | |
| **Heap 余量** | **~5.7 KB** | 充足 |

> 20KB SRAM 总预算：~4.3KB FreeRTOS heap + 1KB startup 栈 + 0.5KB startup heap + ~5KB 全局变量（含 OLED 帧缓冲 1KB）≈ 10.8KB/20KB，余量 ~9.2KB。

#### 4.1.6 与初版方案的对比

| 维度 | 初版方案（每页面一任务） | 最终方案（3 用户任务） | 改善 |
|:---|:---|:---|:---|
| 用户任务数 | 10-12 个 | **3 个** | -7~9 个 TCB |
| Heap 使用量 | ~9.6 KB | **~4.3 KB** | 节省 5.3 KB |
| 互斥锁需求 | OLED 需要 | **不需要** | 消除优先级反转风险 |
| 页面切换 | vTaskSuspend/vTaskResume | **g_CurrentPage 赋值** | 极简可靠 |
| 帧率控制 | 每个任务各写一遍 | **Task_UI 统一管理** | 代码复用 |
| __WFI() | 需要每个任务处理 | **Idle Hook 统一** | 符合 RTOS 最佳实践 |
| 未来扩展（BLE 等） | 余量 ~0.6KB | **余量 ~5.7KB** | 可安全添加新任务 |

### 4.2 分阶段实施

#### Phase 1：CubeMX 集成 FreeRTOS 组件 ✅ 已完成

| 任务 | 说明 | 状态 |
|:---|:---|:---|
| 在 CubeMX 中勾选 FreeRTOS | 使用 CMSIS_V1（与 ARMCC V5.06 最兼容） | ✅ 已完成 |
| 配置 SysTick 为 FreeRTOS tick 源 | SysTick 共享：`HAL_IncTick()` + `xPortSysTickHandler()` | ✅ 已完成 |
| 配置 TIM2 中断优先级 | 调整为 Preemption=4, Sub=0（确保在 FromISR 安全范围内） | ✅ 已完成 |
| 配置 FreeRTOS 内核参数 | 详见 [Phase 1 实施记录](#五phase-1-实施记录--cubemx-集成-freertos) | ✅ 已完成 |
| `USE_RTOS` 保持 `0U` | 2016 年版 HAL 不支持 RTOS 模式（见 [问题 2](#问题-2编译错误-user_tos--1u-触发-error)） | ✅ 已确认 |
| MDK 编译验证 | ARMCC V5.06 编译通过，0 Error, 0 Warning | ✅ 已通过 |

#### Phase 2：改造按键驱动（创建 Task_Input） ✅ 已完成 (2026-06-22)

| 任务 | 说明 | 状态 |
|------|------|------|
| `Key_GetNum()` 临界区升级 | `__disable_irq()` → `taskENTER_CRITICAL()`，修复配对 bug | ✅ 已完成 (2026-06-21) |
| 创建 `Task_Input` | 使用 `ulTaskNotifyTake()` 阻塞等待 ISR 通知 | ✅ 已完成 (2026-06-22) |
| TIM2 ISR 中发送通知 | `KeyTick()` 后调用 `vTaskNotifyGiveFromISR()` | ✅ 已完成 (2026-06-22) |
| 按键路由 | Task_Input → Task_UI 按键转发（全局按键在此处理） | ✅ 全局关机已实现；页面按键转发预留 TODO，待 Phase 3 Task_UI 创建后完成 |

#### Phase 3：页面函数改造为 Task_UI 状态机 ✅ 已完成 (2026-06-23)

| 任务 | 说明 | 状态 |
|------|------|------|
| 定义页面枚举 + 状态机框架 | `PageID_t` 枚举 + `PAGE_CLOCK` ~ `PAGE_SETTIME` 共 11 个页面 | ✅ 已完成 |
| 改造 9 个页面函数 | while(1) → `Render_*()` 单帧函数 + `UI_ProcessKey()` 按键→状态转换 | ✅ 已完成 |
| 统一帧率控制 | `xTaskNotifyWait(0, 0xffffffffUL, &key, pdMS_TO_TICKS(33))` 替代 `HAL_GetTick()` 轮询 | ✅ 已完成 |
| 页面切换机制 | `g_CurrentPage = PAGE_XXX` 一行赋值，替代函数嵌套调用 | ✅ 已完成 |
| 统一 `__WFI()` 休眠 | 移至 `vApplicationIdleHook()`，删除 9 个页面中的手动 `__WFI()` | ✅ 已完成 |
| Task_Input → Task_UI 按键转发 | 删除 TODO 标记，实际调用 `xTaskNotify(Task_UI_Handle, key, eSetValueWithOverwrite)` | ✅ 已完成 |
| 恐龙游戏重构 | `Dino_RenderFrame()` 单帧渲染函数 + `Dino_JumpRequest` 标志位替代 `Key_GetNum()` 轮询 | ✅ 已完成 |
| SetTime 精简 | 删除 7 个 while(1) 函数，保留 3 个底层辅助函数 | ✅ 已完成 |
| 编译验证 | ARMCC V5.06, 0 Error, 0 Warning | ✅ 已通过 |

#### Phase 4：MPU6050 独立采样 + Sleep/Wake 功耗管理 ✅ 已完成 (2026-06-25，待提交)

| 任务 | 说明 | 状态 |
|------|------|------|
| 创建 `Task_Sensor` | 独立任务，优先级 1，栈 512 bytes；`xTaskNotifyWait` 阻塞等待 `SENSOR_CMD_START/STOP` 命令 | ✅ 已完成 |
| 互补滤波独立 | `MPU6050_Calculation_Euler_angles()` 从页面函数中解耦为独立函数，由 `Task_Sensor` 在后台连续 5ms 周期调用 | ✅ 已完成 |
| Sleep/Wake 功耗管理 | 进入传感器页 → `MPU6050_Wake()`，离开 → `MPU6050_Sleep()`；首次运行时跳过 Wake（init 后已活跃） | ✅ 已完成 |
| 全局变量保护 | `g_Roll/g_Pitch/g_Yaw` 写入用 `taskENTER_CRITICAL`/`taskEXIT_CRITICAL` 保护 | ✅ 已完成 |
| `MPU6050_Sleep/Wake` 接口 | 在 `MPU6050.c/h` 中新增 `Sleep()`/`Wake()` 函数，操作 `PWR_MGMT_1` 寄存器 | ✅ 已完成 |
| 页面联动 | `UI_ProcessKey()` 中进入 MPU6050/水平仪页 → `g_SensorActive=1` + `xTaskNotify(START)`；离开 → `g_SensorActive=0` + `xTaskNotify(STOP)` | ✅ 已完成 |
| 硬件 I2C 迁移评估 | 研究 STM32F103 I2C1/I2C2 外设替代软件 I2C | ⏸️ 暂缓 |

> **Note:** Phase 4 代码已完成但尚未提交（`git add` + `commit` 待执行），当前仅在工作树中。

#### Phase 5：回归测试与栈调优

> **设计方案：** 采用 **OLED Debug 页面**进行栈使用量分析，入口方式为 **方案 B：设置页扩展**。
> - 入口路径：时钟 → 菜单 → 设置页 → KEY2 翻到第 3 项 "Debug" → KEY3 进入
> - 不采用串口方案（方案 1），理由：
>   - 需要额外 USB-TTL 串口板硬件 + CubeMX 重新生成 USART 代码，有覆盖手写代码风险
>   - 栈调优只需读取 5 个 `uxTaskGetStackHighWaterMark` 返回值 + 1 个 `xPortGetFreeHeapSize`，串口方案过度
> - Debug 入口放在设置页，语义自然（系统工具），无需像素图标，代码改动量仅 ~4 处

| 任务 | 说明 | 预估时间 |
|------|------|---------|
| 前置准备 | 在 `FreeRTOSConfig.h` 启用 `INCLUDE_uxTaskGetStackHighWaterMark`；提交 Phase 4 代码 | 15 min |
| ① menu.h — PageID_t 加 `PAGE_DEBUG` | 在 `SETTIME` 与 `PAGE_COUNT` 之间插入 | 2 min |
| ② menu.c — `Show_Debug_UI()` + `Render_Debug()` | 使用 OLED_Printf + GoBack 图标，展示 5 任务栈水位 + 堆空闲，参考 MPU6050 模式约 25 行 | 15 min |
| ③ menu.c — `Task_UI_RenderFrame` 加 `case PAGE_DEBUG` | 注册到渲染 dispatch | 1 min |
| ④ menu.c — `UI_ProcessKey` 处理 `PAGE_SETTING` 扩展 + `PAGE_DEBUG` 按键 | SettingFlag 范围 2→3，KEY3 确认进入/返回 | 10 min |
| 栈水位测量与调优 | 运行所有功能→记录峰值→调整栈大小→复测 | 1-2 天 |
| 功能回归测试 | 所有 11 个页面、按键响应、传感器数据、恐龙游戏、SetTime | 1-2 天 |
| 功耗对比测试 | 移植前后电流对比（预期：空闲时自动 WFI，功耗持平或更优） | 1 天 |

##### 实施细节：设置页扩展

**设置页当前状态：**

```
SettingFlag 1 → [← 返回]         (KEY3 → 回时钟)
SettingFlag 2 → [Set DateTime]   (KEY3 → 进入 SetTime 状态机)
```

**扩展后：**

```
SettingFlag 1 → [← 返回]
SettingFlag 2 → [Set DateTime]
SettingFlag 3 → [Debug]          ← 新增选项
```

**改动点汇总（4 处）：**

| # | 文件 | 改动 | 代码 |
|:---|:---|:---|:---|
| ① | `menu.h` PageID_t | 新增 `PAGE_DEBUG` | 插在 `PAGE_SETTIME` 与 `PAGE_COUNT` 之间 |
| ② | `menu.c` 新增函数 | `Show_Debug_UI()` + `Render_Debug()` | ~25 行，参考 `Show_MPU6050_UI()` 模式 |
| ③ | `menu.c` switch | `case PAGE_DEBUG: Render_Debug(); break;` | 1 行 |
| ④ | `menu.c` UI_ProcessKey | `PAGE_SETTING` case 的 `SettingFlag` 范围从 1~2 扩展到 1~3；`case 3: g_CurrentPage = PAGE_DEBUG;`；新增 `case PAGE_DEBUG:` 处理 KEY3 返回 | ~10 行 |

**设置页 KEY 处理变更（`UI_ProcessKey`）：**

```c
// 修改前：
case PAGE_SETTING:
    if(key == 1) { if(--SettingFlag == 0) SettingFlag = 2; }
    else if(key == 2) { if(++SettingFlag == 3) SettingFlag = 1; }
    else if(key == 3) {
        if(SettingFlag == 1) { g_CurrentPage = PAGE_CLOCK; }
        else { /* 进入设置时间 */ g_CurrentPage = PAGE_SETTIME; }
    }

// 修改后：
case PAGE_SETTING:
    if(key == 1) { if(--SettingFlag == 0) SettingFlag = 3; }       // 2→3
    else if(key == 2) { if(++SettingFlag == 4) SettingFlag = 1; }  // 3→4
    else if(key == 3) {
        if(SettingFlag == 1) { g_CurrentPage = PAGE_CLOCK; }
        else if(SettingFlag == 2) { g_CurrentPage = PAGE_SETTIME; }
        else { g_CurrentPage = PAGE_DEBUG; }                       // ← 新增
    }

case PAGE_DEBUG:                                                     // ← 新增
    if(key == 3) { g_CurrentPage = PAGE_SETTING; SettingFlag = 3; } // 返回设置页
```

##### OLED Debug 页面渲染设计

利用现有 128×64 OLED 和 6×8 字体，直接在手表上显示调试信息：

```
┌─────────────────────┐
│[←]  ← GoBack 16×16  │
│ Inp:  20/ 96 w       │  ← Task_Input: 峰值/总量 words
│ UI:  180/320 w       │  ← Task_UI
│ Sen:  28/128 w       │  ← Task_Sensor
│ Idle: 24/128 w       │  ← Idle Task
│ Tmr:  48/256 w       │  ← Timer Task
│ Heap: 5840/10240 B   │  ← 空闲堆/总堆
│ KEY3=返回设置        │
└─────────────────────┘
```

- **渲染模式：** 完全参照 `Show_MPU6050_UI()` —— `OLED_ShowImage(0,0,16,16,GoBack)` + 多行 `OLED_Printf(0,Y,OLED_6X8, fmt, ...)`
- **数据来源：** `uxTaskGetStackHighWaterMark(handle)` + `xPortGetFreeHeapSize()`
- **自干扰分析：** `uxTaskGetStackHighWaterMark` 返回**历史峰值**（自任务创建以来最小值）。先遍历完所有 11 个正常页面（记录峰值栈使用），再进入 Debug 页读数。Debug 页面自身的绘制栈消耗不计入峰值，测量准确
- **返回方式：** KEY3 → 回设置页，`SettingFlag=3`（保持 Debug 选项选中状态）

##### Stack High Water Mark 测量机制

```
uxTaskGetStackHighWaterMark(task_handle) 返回:
   自任务创建以来，栈区残留 0xa5a5a5a5 填充值的 word 数量（最小值）

峰值使用量 = 总栈大小 - uxTaskGetStackHighWaterMark()

调优目标: 峰值使用量 × 1.5 ≤ 调整后栈大小（保留 50% 安全余量）
```

##### 栈调整决策矩阵

| 任务 | 当前栈 (words) | 预期峰值 | 调整后 |
|------|:-----------:|:--------:|:-----:|
| Task_Input | 96 | < 30 | 可能降至 48-64 |
| Task_UI | 320 | 待测（最深调用链） | 保留或微调 |
| Task_Sensor | 128 | < 50 | 可能降至 64-96 |
| Idle Task | 128 | < 30 | 可降至 64 |
| Timer Task | 256 | < 50 | 可降至 128 |

> **测试流程：** 
> 1. 编译烧录固件（含 `INCLUDE_uxTaskGetStackHighWaterMark=1` + Debug 页面）
> 2. 依次遍历所有 11 个页面，操作各功能（按键、游戏碰撞、传感器采样）
> 3. 操作：时钟 → KEY3 → 菜单 → 滑到"设置" → KEY3 → 设置页 → KEY2 翻到 "Debug" → KEY3 进入
> 4. 拍照记录各任务水位
> 5. 根据峰值数据调整 `xTaskCreate()` 中的栈深度参数
> 6. 缩小栈后重新运行，确认 `uxTaskGetStackHighWaterMark` 余量 > 30%

### 4.3 总预估与实际进展

| 指标 | 计划 | 实际 |
|------|------|------|
| **总工作量** | **4-7 个工作日** | **Phase 1-4 已完成，剩余 Phase 5** |
| Phase 1 | 1 天 | ✅ 1 天 (2026-06-16) |
| Phase 2 | 1 天 | ✅ 1 天 (2026-06-22) |
| Phase 3 | 2-3 天 | ✅ 1 天 (2026-06-23) |
| Phase 4 | 1 天 | ✅ 1 天 (2026-06-25) |
| 新增/修改文件 | ~8-12 个 | ~14 个（含 .gitattributes、doc） |
| 核心改动量 | ~400-600 行 C 代码 | ~1,686 行（Phase 3 重构量大，含大量删除） |
| 风险等级 | 中等 | 🟢 低 — 已按计划推进 |

> **Phase 4 待办：** `git add` + `git commit` 尚未执行，代码当前仅在工作树中。

---

## 五、Phase 1 实施记录 — CubeMX 集成 FreeRTOS

> **执行日期：** 2026-06-16
> **编译状态：** ✅ ARMCC V5.06 编译通过，0 Error, 0 Warning
> **FreeRTOS 版本：** V10.3.1 (STM32CubeMX 6.17.0 内置)

### 5.1 前置准备

#### 5.1.1 Git 分支管理

| 操作 | 命令 | 说明 |
|:---|:---|:---|
| 打标签 | `git tag v1.0_baremetal` | 裸机最终版快照，指向 `acfa777` |
| 建分支 | `git checkout -b FreeRTOS_Version` | 从当前节点创建专用分支 |

#### 5.1.2 备份关键文件

在 CubeMX 生成代码前，备份会被覆盖的 3 个关键文件：

```bash
mkdir backup/
cp SmartWatch_HAL/HAL/Core/Src/main.c              backup/main.c.bak
cp SmartWatch_HAL/HAL/Core/Src/stm32f1xx_it.c       backup/stm32f1xx_it.c.bak
cp SmartWatch_HAL/HAL/Core/Inc/stm32f1xx_hal_conf.h backup/stm32f1xx_hal_conf.h.bak
```

### 5.2 CubeMX 配置详情

#### 5.2.1 启用 FreeRTOS

`Pinout & Configuration` → `Middleware` → **`FREERTOS`** → 勾选 ✅

| 参数 | 选择 | 理由 |
|:---|:---|:---|
| **Interface** | **CMSIS_V1** | 编译器为 ARMCC V5.06，CMSIS_V2 对旧编译器兼容性不稳定。V1 使用 `osThreadDef()` / `osThreadCreate()` API，与 V5 编译器完美配合 |

#### 5.2.2 内核参数配置

| 参数 | 值 | 分类 | 理由 |
|:---|:---|:---|:---|
| **USE_PREEMPTION** | `1` | 调度 | 抢占式调度，按键任务可打断 UI 渲染 |
| **TICK_RATE_HZ** | `1000` | 时间基准 | 1ms tick，与 TIM2 周期一致，UI 帧率控制精度足够 |
| **MAX_PRIORITIES** | `5` | 内存 | 5 个优先级够用（见 5.2.7 优先级分配），每个优先级增加一个就绪链表，少一级省 ~20 字节 RAM |
| **MINIMAL_STACK_SIZE** | `128` (512 bytes) | 内存 | 给 idle task + timer task 用，Cortex-M3 上下文 16 字 + 嵌套中断栈帧，余量充足 |
| **MAX_TASK_NAME_LEN** | `16` | 调试 | 够写 `Task_Sensor`、`Task_Input` 等名字 |
| **USE_16_BIT_TICKS** | `0` | 时间基准 | 32-bit tick，1ms tick → 溢出时间 49.7 天，远超手表使用场景 |
| **IDLE_SHOULD_YIELD** | `1` | 调度 | 有同优先级就绪任务时 idle task 主动让出 CPU |

#### 5.2.3 同步与通信

| 参数 | 值 | 理由 |
|:---|:---|:---|
| **USE_MUTEXES** | `1` | 保留启用，为未来共享资源（如 BLE 串口）预留。当前方案中 OLED 和 MPU6050 I2C 均为单任务独占，实际不需要互斥锁 |
| **USE_RECURSIVE_MUTEXES** | `0` | 不需要递归锁 |
| **USE_COUNTING_SEMAPHORES** | `0` | 暂不需要，按键通知用 Task Notifications 更轻量 |
| **USE_TASK_NOTIFICATIONS** | `1` ⭐ | **Phase 2 的关键依赖！** ISR 中用 `vTaskNotifyGiveFromISR()` 通知按键任务，比信号量快 45%、省一个队列控制块 |

**为什么 Task Notification 是最关键的选项：**

| 通知方式 | RAM 占用 | 速度 | 适用场景 |
|:---|:---|:---|:---|
| Binary Semaphore | 队列控制块 + 数据（~100 字节） | 慢 | 多对多通知 |
| **Task Notification** | **0 字节（复用 TCB 内字段）** | **快 45%** | 一对一通知 |

本项目按键 ISR → 按键处理任务正好是一对一模式，Task Notification 是最优解。

#### 5.2.4 内存管理

| 参数 | 值 | 理由 |
|:---|:---|:---|
| **TOTAL_HEAP_SIZE** | `10240` (10KB) | 20KB SRAM 中分配 10KB 给 FreeRTOS heap，详见下方计算 |
| **Memory Management** | `heap_4.c` | 最佳适配算法 + 相邻空闲块自动合并，防碎片 |

**10KB 内存详细计算（3 用户任务方案）：**

| 项目 | 估算 | 累计 |
|:---|:---|:---|
| Idle Task 栈 | 512 bytes | 512 |
| Timer Task 栈 | 1,024 bytes | 1,536 |
| Task_Input 栈 | 384 bytes | 1,920 |
| Task_UI 栈 | 1,280 bytes | 3,200 |
| Task_Sensor 栈 | 512 bytes | 3,712 |
| 5 个 TCB（~80B/个） | ~400 bytes | 4,112 |
| 队列/互斥量（OLED 保护备用） | ~200 bytes | 4,312 |
| **安全余量** | **~5,928 bytes** | **10,240** |

> 20KB SRAM 总预算：~4.3KB FreeRTOS heap + 1KB startup 栈 + 0.5KB startup heap + ~5KB 全局变量（含 OLED 帧缓冲 1KB）≈ 10.8KB/20KB，余量 ~9.2KB。
> 对比初版 13 任务方案（~9.6KB heap 占用），3 任务方案节省约 5.3KB，为 BLE 通信、SPI Flash 等功能扩展预留了充裕空间。

#### 5.2.5 调试与 Hook

| 参数 | 值 | 理由 |
|:---|:---|:---|
| **USE_IDLE_HOOK** | `1` | 在 idle hook 里执行 `__WFI()`，替代当前裸机版 9 个页面函数中各自的手动 `__WFI()` |
| **USE_TICK_HOOK** | `0` | 不需要 |
| **USE_MALLOC_FAILED_HOOK** | `1` | 20KB SRAM 紧张，内存分配失败时至少能进断点调试 |
| **USE_DAEMON_TASK_STARTUP_HOOK** | `0` | 不需要 |
| **CHECK_FOR_STACK_OVERFLOW** | `2` | 开发阶段用最严检查（方法 2：检查栈顶 canary 字是否被改写） |
| **USE_TRACE_FACILITY** | `1` | Phase 5 栈调优的必备条件，`uxTaskGetStackHighWaterMark()` 需要它 |
| **USE_STATS_FORMATTING_FUNCTIONS** | `1` | 提供 `vTaskList()` / `vTaskGetRunTimeStats()`，调试利器 |
| **GENERATE_RUN_TIME_STATS** | `0` | 需要额外的硬件定时器做时间戳，先关掉省 RAM |

#### 5.2.6 软件定时器

| 参数 | 值 | 理由 |
|:---|:---|:---|
| **USE_TIMERS** | `1` | 为未来的定时传感器采样等预留 |
| **TIMER_TASK_PRIORITY** | `2` | 与 UI 任务同级，不被低优先级任务阻塞 |
| **TIMER_TASK_STACK_DEPTH** | `256` (1KB) | timer task 回调可能涉及外设操作 |
| **TIMER_QUEUE_LENGTH** | `10` | 10 个待处理定时器命令，远超实际需求 |

#### 5.2.7 任务优先级分配方案

| 任务 | 优先级 | 理由 |
|:---|:---|:---|
| **Task_Input** (按键处理) | **3** | 实时响应按键，不能被 UI 渲染阻塞 |
| **Task_UI** (统一页面渲染) | **2** | 拥有 OLED，所有页面在此任务内运行 |
| **Task_Sensor** (MPU6050 采样) | **1** | 后台连续采样，不阻塞 UI；偶发的采样延迟可接受 |
| **Idle Task** | **0** | FreeRTOS 自动管理，idle hook 中执行 `__WFI()` |
| **Timer Task** | **2** | FreeRTOS 内部，与 UI 同级，为未来定时功能预留 |

#### 5.2.8 中断优先级配置

**FreeRTOS 的铁律：**

> `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`（=3）以上的中断才能调用 `*FromISR()` API。
> **SysTick 优先级必须为最低（15）**，否则 ISR 中的上下文切换会导致竞态条件。

| 中断 | 优先级 | 配置方式 | 理由 |
|:---|:---|:---|:---|
| **SysTick** | **15** (最低) | CubeMX 自动设置 | FreeRTOS 要求，防止 ISR 中的竞态条件 |
| **PendSV** | **15, 0** | FreeRTOS `port.c` 接管 | 上下文切换用，必须最低（CubeMX 标灰不可改） |
| **SVC** | FreeRTOS 管理 | FreeRTOS `port.c` 接管 | 启动调度器用（CubeMX 标灰不可改） |
| **TIM2** | **4, 0** | CubeMX NVIC 配置 | 抢占优先级 4 > 3，在 FromISR 安全范围内 |

**为什么 SysTick 必须是 15：**

```
❌ SysTick = 优先级 3（太高）:

TIM2_IRQHandler (优先级 1)
    └── 正在调用 vTaskNotifyGiveFromISR()  ← 修改 TCB 状态
            │
            ├── 💥 SysTick 来了（优先级 3 > 1）
            │       └── xPortSysTickHandler()
            │               └── 上下文切换！
            │                   └── TCB 状态不一致 → HardFault

✅ SysTick = 优先级 15（最低）:

TIM2_IRQHandler (优先级 1)
    └── vTaskNotifyGiveFromISR()
            │
            ├── SysTick 来了（优先级 15 < 1）→ 排队等着
            └── ISR 安全返回
    → 退出 ISR → SysTick 才执行上下文切换 ✅
```

#### 5.2.9 SYS Timebase 保持 SysTick

| 参数 | 值 | 理由 |
|:---|:---|:---|
| **Timebase Source** | **SysTick** | 不动！`HAL_GetTick()` 系在 SysTick 上，所有页面函数的帧率控制都依赖它 |

### 5.3 CubeMX 生成代码后的文件变更

#### 5.3.1 需要手动修复的文件

| 文件 | 变更内容 | 处理 |
|:---|:---|:---|
| `stm32f1xx_hal_conf.h` | `TICK_INT_PRIORITY: 0U → 15U`；`USE_RTOS` 保持 `0U` | ✅ 已手动修正 |
| `stm32f1xx_it.c` | `SysTick_Handler()` 新增 `xPortSysTickHandler()`；`SVC_Handler` / `PendSV_Handler` 被移除 | ✅ 正确（FreeRTOS 接管） |
| `stm32f1xx_it.h` | 移除 `SVC_Handler` / `PendSV_Handler` 声明 | ✅ 正确 |
| `stm32f1xx_hal_msp.c` | `NVIC_PRIORITYGROUP_2` 被移除；TIM2 优先级 `2,1` → `4,0` | ✅ 正确（FreeRTOS `port.c` 接管优先级分组） |
| `main.c` | 新增 `cmsis_os.h` include、`osKernelStart()`、`StartDefaultTask()` | ✅ 手写代码未丢失 |
| `HAL.uvprojx` | 新增 FreeRTOS 源码文件和 include 路径 | ✅ 正确 |
| `HAL.ioc` | CubeMX 配置更新 | ✅ 正常 |

#### 5.3.2 手写代码完整性验证

| 检查项 | 结果 |
|:---|:---|
| `main.c` — `SystemClock_Config()` / `MX_*_Init()` 保留 | ✅ |
| `stm32f1xx_it.c` — `TIM2_IRQHandler()` 中 4 个回调保留 | ✅ |
| `stm32f1xx_it.c` — `#include "Hardware/Key.h"` 保留 | ✅ |
| `stm32f1xx_hal_msp.c` — 外设初始化代码保留 | ✅ |

---

## 六、Phase 1 遇到的问题与解决方案

### 问题 1：PendSV / SVC 优先级在 CubeMX 中被标灰

**现象：** CubeMX NVIC 配置中，PendSV 和 SVC 的 Preemption Priority 和 Sub Priority 字段被标灰，无法修改。当前值显示为 15。

**分析：** 当 CubeMX 检测到 FreeRTOS 已启用时，自动锁定这两个中断。PendSV 用于上下文切换（必须最低优先级），SVC 用于启动调度器。优先级由 FreeRTOS 的 `port.c` 在运行时强制设置，CubeMX 标灰是保护机制——如果用户改错，调度器直接崩溃。

**结论：** 正常现象，无需处理。

---

### 问题 2：编译错误 — `USE_RTOS` = 1U 触发 `#error`

**现象：**

Keil MDK ARMCC V5.06 编译时，所有 34 个源文件报同一个错误：

```
../Drivers/STM32F1xx_HAL_Driver/Inc/stm32f1xx_hal_def.h(91):
error: #35: #error directive:
"USE_RTOS should be 0 in the current HAL release"
```

**排查过程：**

1. 查看 [stm32f1xx_hal_def.h:89-91](../SmartWatch_HAL/HAL/Drivers/STM32F1xx_HAL_Driver/Inc/stm32f1xx_hal_def.h#L89-L91)：

   ```c
   #if (USE_RTOS == 1U)
   /* Reserved for future use */
   #error "USE_RTOS should be 0 in the current HAL release"
   #else
   #define __HAL_LOCK(__HANDLE__)   ...   // 裸机版自旋锁
   #define __HAL_UNLOCK(__HANDLE__) ...
   #endif
   ```

2. 查看 HAL 驱动源码 [stm32f1xx_hal.c](../SmartWatch_HAL/HAL/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c) 文件头：
   ```
   Copyright (c) 2016 STMicroelectronics
   ```

**根因：**

本项目的 STM32F1 HAL 驱动库是 **2016 年版**的。那时 ST 还没给 STM32F1 的 HAL 库添加 RTOS 支持。
`#if (USE_RTOS == 1U)` 下的注释 `"Reserved for future use"` 表明这个功能被预留给未来版本，但 ST 此后一直没有为 STM32F1 系列实现 RTOS 版本的 `__HAL_LOCK`。

**`__HAL_LOCK` / `__HAL_UNLOCK` 的作用：**

这两个宏是 HAL 库内部的**重入保护锁**，防止同一个外设被嵌套调用：

| 场景 | `USE_RTOS=0`（自旋锁） | `USE_RTOS=1`（未来计划） |
|:---|:---|:---|
| 任务 A 调用 `HAL_ADC_Start()` | 加锁 → 执行 → 解锁 | 用 `osMutexAcquire()` 加锁 |
| 任务 B 同时调用同一外设 | 发现已锁 → 返回 `HAL_BUSY` | **阻塞等待**直到任务 A 解锁 |

**对本项目的影响：零**

| 条件 | 结论 |
|:---|:---|
| 外设使用模式 | 每个外设（ADC、TIM2、RTC）只被一个任务使用 |
| 会发生多任务竞争吗？ | ❌ 不会 |
| 软件 I2C | 不走 HAL，走 `MyI2C.c`，不受影响 |

`__HAL_LOCK` / `__HAL_UNLOCK` 的裸机自旋锁版本对本项目完全够用。

**解决方案：**

[stm32f1xx_hal_conf.h:133](../SmartWatch_HAL/HAL/Core/Inc/stm32f1xx_hal_conf.h#L133) 将 `USE_RTOS` 保持为 `0U`：

```c
#define  USE_RTOS   0U   // 2016 年版 HAL 不支持 RTOS 模式，保持 0U
```

**结论：** `USE_RTOS` 这个宏只控制 `__HAL_LOCK` 的实现方式，不影响 FreeRTOS 的调度、任务通知、互斥量等核心功能。本项目外设使用模式简单（单任务独占），自旋锁版本完全满足需求。

---

### 问题 3：CubeMX 报 "Incorrect preemption priority for system tick timer"

**现象：**

CubeMX 弹出警告对话框：

> "Incorrect preemption priority for system tick timer '3'. Do you want to fix it and set it to 15?"

**分析：**

CubeMX 生成的默认 SysTick 优先级为 3，但 FreeRTOS 要求 SysTick 优先级必须为最低（15）。
点击 "Yes" 后 CubeMX 自动将 `TICK_INT_PRIORITY` 从 `3U` 改为 `15U`。

**根因：** 见 5.2.8 节的 SysTick 优先级竞态条件分析。

**解决方案：** 点击 "Yes"，让 CubeMX 自动修正。

---

### 问题 4：TIM2 优先级从裸机版的 `2,1` 变为 `4,0`

**现象：**

CubeMX 生成代码后，[stm32f1xx_hal_msp.c](../SmartWatch_HAL/HAL/Core/Src/stm32f1xx_hal_msp.c) 中 TIM2 优先级从裸机版的 `Preemption=2, Sub=1` 变为 `Preemption=4, Sub=0`。

**分析：**

`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 3`，意味着只有抢占优先级 ≥ 3 的中断（数值更大，实际优先级更低）才能调用 `*FromISR()` API。

- 裸机版 TIM2 优先级 = `2, 1`（PRIORITYGROUP_2 下 Preemption=0, Sub=2）→ 抢占优先级 0，在范围内但太靠边界
- FreeRTOS 版 TIM2 优先级 = `4, 0` → 抢占优先级 1，在范围内且更安全

CubeMX 自动调整是为了确保 TIM2 ISR 能安全调用 `vTaskNotifyGiveFromISR()`（Phase 2 需要）。

**对本项目的影响：** 无。TIM2 ISR 中 4 个回调的执行时间均为微秒级，且没有抢占优先级 0-3 的其他中断会抢占 TIM2。

**结论：** 正常，无需修改。

---

## 六（续）、Phase 2 实施记录 — 创建 Task_Input

> **执行日期：** 2026-06-22
> **方案确认：** 手动编写原生 FreeRTOS API（非 CubeMX CMSIS_V1），详见 [分析](#cubeMX-vs-手动对比)

### Phase 2.1 `Key_GetNum()` 临界区升级

**修改文件：** [Key.c](../SmartWatch_HAL/HAL/Core/Src/Hardware/Key.c)

| 项目 | 裸机版 | FreeRTOS 版 |
|------|--------|------------|
| 屏蔽方式 | `__disable_irq()` (PRIMASK) | `taskENTER_CRITICAL()` (BASEPRI) |
| 配对 | 1 disable : 2 enable（bug） | 1 enter : 1 exit（正确） |
| 出口 | 两条路径分别 return | 统一出口 `return Temp` |
| 头文件 | 仅 `Hardware/Key.h` | 新增 `FreeRTOS.h` + `task.h` |

### Phase 2.2 创建 Task_Input 任务

**涉及文件（5 个）：**

| 文件 | 变更 |
|------|------|
| [Key.h](../SmartWatch_HAL/HAL/Core/Inc/Hardware/Key.h) | 新增 `FreeRTOS.h`/`task.h` include、`Key_HasPending()` 声明、`Task_Input_Handle` extern |
| [Key.c](../SmartWatch_HAL/HAL/Core/Src/Hardware/Key.c) | 新增 `Task_Input_Handle` 全局定义、`Key_HasPending()` 函数 |
| [stm32f1xx_it.c](../SmartWatch_HAL/HAL/Core/Src/stm32f1xx_it.c) | TIM2 ISR 新增 `vTaskNotifyGiveFromISR()` + `portYIELD_FROM_ISR()` |
| [freertos.c](../SmartWatch_HAL/HAL/Core/Src/freertos.c) | 新增 `Task_Input()` 任务函数体 |
| [main.c](../SmartWatch_HAL/HAL/Core/Src/main.c) | 新增 `FreeRTOS.h`/`task.h` include、`xTaskCreate(Task_Input, ...)` |

**通信链路：**

```
TIM2_IRQHandler (1ms, 优先级 4,0)
  └── KeyTick() → Key_Num 写入
  └── Key_HasPending()? → vTaskNotifyGiveFromISR(Task_Input_Handle)
      └── Task_Input 解除阻塞 (优先级 3)
          └── Key_GetNum() [taskENTER_CRITICAL 保护]
          └── Key3 长按(4)? → POWER_Shutdown()   ← 全局关机
          └── 其他按键? → TODO Phase 3: xTaskNotify(Task_UI)
```

**安全验证：**

| 检查项 | 结果 |
|--------|------|
| TIM2 优先级 4 > `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`(3) | ✅ FromISR 安全 |
| `Task_Input_Handle != NULL` 防护 | ✅ ISR 在 xTaskCreate 前触发时跳过通知 |
| `Key_HasPending()` ISR 中仅读取 `volatile uint8_t` | ✅ Cortex-M3 原子 |
| `taskENTER_CRITICAL` 嵌套安全 | ✅ BASEPRI 屏蔽，不影响 SysTick (优先级 15) |

### CubeMX vs 手动对比（创建任务）

Phase 2 确认采用**手动编写原生 FreeRTOS API**，不使用 CubeMX CMSIS_V1：

| 维度 | CubeMX CMSIS_V1 | 手动原生 API |
|------|----------------|-------------|
| Task Notification | `osSignal`（不等价，基于队列） | `vTaskNotifyGiveFromISR`（零 RAM，快 45%） |
| 与文档设计一致性 | ❌ 文档全部使用原生 API | ✅ 完全一致 |
| 重新生成风险 | CubeMX 可能覆盖任务配置 | `USER CODE` 区安全 |
| 灵活性 | 受限 GUI 参数 | 完全控制 |

---

## Phase 3 实施记录 — Task_UI 统一页面渲染状态机

> **执行日期：** 2026-06-23
> **编译验证：** ✅ ARMCC V5.06, 0 Error, 0 Warning
> **Commit：** `239fe6a`

### 改造思路

将 9 个裸机 while(1) 独占式页面函数拆分为两层：

```
[freertos.c]                         [menu.c]
                                     
Task_UI 主循环                       Render_Clock()       ← 纯绘制
  └─ xTaskNotifyWait(33ms)           Render_Menu()        ← 纯绘制
       └─ Task_UI_RenderFrame(key)   Render_Setting()     ← 纯绘制
              ├─ UI_ProcessKey(key)  Render_Stopwatch()   ← 纯绘制
              └─ switch(g_CurrentPage) ...                ← 纯绘制
                    ├─ Render_Clock()
                    ├─ Render_Menu()       UI_ProcessKey(key)   ← 按键→状态转换
                    └─ ...                 g_CurrentPage = PAGE_XXX   ← 页面切换
```

| 层 | 职责 | 典型代码 |
|:---|:---|:---|
| `Task_UI` | 帧率控制 + 按键分发 | `xTaskNotifyWait(0, ULONG_MAX, &key, pdMS_TO_TICKS(33))` |
| `Task_UI_RenderFrame()` | 按键处理 → 状态转换 → 渲染 → 刷新 OLED | `UI_ProcessKey(key)` + `switch(g_CurrentPage)` + `OLED_Update()` |
| `Render_*()` | 单帧绘制（不包含 `OLED_Clear`/`Update`） | `Show_Clock_UI();` + 高亮反转 |
| `UI_ProcessKey()` | 按键→页面状态转换 | `g_CurrentPage = PAGE_MENU;` |

### 涉及文件变更

| 文件 | 变更说明 |
|:---|:---|
| `freertos.c` | 新增 `Task_UI()` 任务函数，`vApplicationIdleHook()` 覆写为统一 `__WFI()`；`Task_Input` 按键转发从 TODO 变为实际 `xTaskNotify()` 调用 |
| `menu.c` | 删除 9 个 while(1) 函数；新增 10 个 `Render_*()` + `UI_ProcessKey()` + `PAGE_SETTIME` 子状态机；删除所有手动 `__WFI()` |
| `menu.h` | 新增 `PageID_t`/`SetTimeState_t` 枚举、`Task_UI_Handle` extern、`Task_UI_RenderFrame()` 声明；删除 9 个旧页面函数声明 |
| `main.c` | 删除裸机 super loop；创建 `Task_UI`（优先级 2，栈 320 words） |
| `dino.c/h` | 提取 `Dino_RenderFrame()` 单帧函数；`Key_GetNum()` → `Dino_JumpRequest` 标志位 |
| `SetTime.c/h` | 删除 7 个 while(1) 函数，保留 3 个底层辅助函数 |
| `.gitattributes` | 新增 UTF-8 编码强制 |

### 设计差异点

| 计划项 | 实际实现 | 原因 |
|:---|:---|:---|
| 11 个页面（+SETTIME） | ✅ 符合预期 | SetTime 子状态机作为独立页面处理 |
| Render_* 为 `static` | ✅ 符合预期 | 仅 `Task_UI_RenderFrame()` 对外暴露 |
| `OLED_Clear()` 放在 switch 前 | ✅ 符合预期 | 每帧先清再绘，避免残影 |
| 恐龙游戏独立任务 | ❌ 保留在 Task_UI 中 | 游戏逻辑在 TIM2 ISR 中 1ms 更新，渲染只需 30FPS 读取状态，不需要独立任务 |

---

## Phase 4 实施记录 — Task_Sensor MPU6050 独立后台采样

> **实施日期：** 2026-06-25（代码已完成，待提交 commit）
> **提交状态：** ⚠️ 代码在工作树中，尚未 `git commit`

### 设计架构

```
Task_UI (Prio 2)                        Task_Sensor (Prio 1)
  │                                         │
  ├─ 进入 MPU6050/水平仪页                   │
  │   g_SensorActive = 1                     │
  │   xTaskNotify(START) ─────────────────►  │
  │                                         ├─ xTaskNotifyWait(portMAX_DELAY)
  │                                         │   ├─ [首次] 跳过 Wake
  │                                         │   ├─ [后续] MPU6050_Wake() + 5ms
  │                                         │   ├─ warmup 20 帧 (100ms)
  │                                         │   ├─ while(g_SensorActive) 采样
  │                                         │   │   └─ MPU6050_Calculation_Euler_angles()
  │                                         │   └─ MPU6050_Sleep()
  │                                         │
  ├─ 离开 MPU6050/水平仪页                   │
  │   g_SensorActive = 0                     │
  │   xTaskNotify(STOP) ─────────────────►  │  (g_SensorActive 退出循环)
  │                                         │
  └─ 读取 g_Roll/g_Pitch/g_Yaw (volatile)   │
```

### 涉及文件变更

| 文件 | 变更说明 |
|:---|:---|
| `freertos.c` | 新增 `Task_Sensor()` 任务函数；`#include "Hardware/MPU6050.h"` |
| `main.c` | `xTaskCreate(Task_Sensor, "Task_Sensor", 128, NULL, 1, &Task_Sensor_Handle)` |
| `menu.c` | `MPU6050_Calculation_Euler_angles()` 中局部变量 `Roll/Pitch/Yaw` → 全局 `volatile g_Roll/g_Pitch/g_Yaw`；写入加 `taskENTER_CRITICAL`；`Show_MPU6050_UI()`/`Show_Gradienter_UI()` 使用 `g_` 全局变量；页面切换联动 `g_SensorActive` + `xTaskNotify(START/STOP)` |
| `menu.h` | 新增 `Task_Sensor_Handle` extern、`g_SensorActive` extern、`SENSOR_CMD_START/STOP` 宏定义 |
| `MPU6050.c` | 新增 `MPU6050_Sleep()` / `MPU6050_Wake()` 函数 |
| `MPU6050.h` | 新增 `MPU6050_Sleep()` / `MPU6050_Wake()` 声明 |

### 与计划的关键差异

| 计划项 | 实际实现 | 原因 |
|:---|:---|:---|
| 互补滤波迁移到 Task_Sensor 上下文 | ❌ Euler 角计算留在 `menu.c` 中 | `MPU6050_Calculation_Euler_angles()` 依赖 menu.c 中的全局变量 (`delta`, `a`, `ax`~`gz`)，且被 `Show_Gradienter_UI()` 显示函数读取，留在 `menu.c` 更方便；Task_Sensor 仅做循环调用 |
| 5ms 采样周期 | ✅ 符合预期 | `vTaskDelay(pdMS_TO_TICKS(MPU_SAMPLE_DELAY_MS))` |
| Sleep/Wake 管理 | ✅ 符合预期 | `first_run` 标志位处理首次跳过，避免重复 Wake |

---

## 七、结论

### 综合评估：✅ 移植基本完成（Phase 5 待做）

| 维度 | 评分 | 说明 |
|------|------|------|
| 硬件资源 | 🟢 充裕 | 20KB RAM 中 ~10.8KB 已用，余量 ~9.2KB；3 用户任务方案大幅节省内存 |
| 时钟系统 | 🟢 完全兼容 | 72MHz + DWT 延时方案确保零冲突 |
| 驱动兼容性 | 🟢 大部分无需修改 | OLED/MyI2C/RTC/ADC/GPIO 均与 OS 无关 |
| 架构适配 | 🟢 **已完成** | 9 个 while(1) 页面函数已全部改造为 `Render_*()` 状态机架构 |
| I2C 阻塞 | 🟡 可接受 | Task_UI 独占 OLED I2C，Task_Sensor 独占 MPU6050 I2C；无需互斥锁 |
| 传感器采样 | 🟢 **已完成** | Task_Sensor 独立后台 5ms 连续采样 + Sleep/Wake 功耗管理 |

### 当前进度

| Phase | 内容 | 状态 | 日期 |
|:---|:---|:---|:---|
| Phase 1 | CubeMX 集成 FreeRTOS 组件 | ✅ 已完成 | 2026-06-16 |
| Phase 2 | 改造按键驱动（Task_Input） | ✅ 已完成 | 2026-06-22 |
| Phase 3 | 页面函数改造为 Task_UI 状态机 | ✅ 已完成 | 2026-06-23 |
| Phase 4 | MPU6050 独立采样 + Sleep/Wake | ✅ 已完成 | 2026-06-25 |
| Phase 5 | 回归测试与栈调优 | ❌ 待开始 | — |

### 移植已实现的收益

1. **代码结构清晰** — 按键输入、UI 渲染、传感器采样三个职责分离
2. **实时响应** — 按键任务最高优先级（3），消除当前轮询延迟
3. **功耗优化** — Idle Hook 统一 `__WFI()`，比 9 个页面各自手动 `__WFI()` 更精确
4. **传感器持续采样** — MPU6050 互补滤波不再被页面切换打断
5. **扩展性** — Heap 余量 ~5.9KB，SRAM 总余量 ~9.2KB，未来添加 BLE、SPI Flash、心率传感器等只需新增任务
6. **调试便利** — FreeRTOS 的任务列表、栈监控等调试工具

### 剩余工作（Phase 5）

1. **栈使用量分析** — 对所有任务调用 `uxTaskGetStackHighWaterMark()`，精确调优栈大小
2. **功能回归测试** — 所有 9 个页面、按键响应、传感器数据、恐龙游戏
3. **功耗对比测试** — 移植前后电流对比

### 移植的实际成本

1. **工作量** — Phase 1-4 实际约 4 个工作日（Phase 3 因准备充分仅用 1 天）
2. **架构改动** — 页面函数重构为状态机（while(1) → switch-case）是主要工作量，已全部完成
3. **回归风险** — 需 Phase 5 全面测试确认
4. **Flash 占用** — FreeRTOS 内核约 6-8KB

### 建议

> **Phase 5（回归测试与栈调优）建议尽快执行**，确认 Phase 1-4 所有修改的功能完整性，
> 并调优各任务栈大小以节省 RAM。完成 Phase 5 后，FreeRTOS 移植全部完成。

---

## 八、问题整理归纳

> 本章汇总 FreeRTOS 移植过程中遇到并解决的问题，便于查阅和回溯。

---

### 问题 5：`xTaskNotifyWait()` 中 `ULONG_MAX` 未定义

**发现阶段：** Phase 3 实施 — Task_UI 编译验证

**现象：**

Keil MDK ARMCC V5.06 编译时报错：

```
../Core/Src/freertos.c(194): error: #20: identifier "ULONG_MAX" is undefined
    if (xTaskNotifyWait(0, ULONG_MAX, &key, pdMS_TO_TICKS(FRAME_PERIOD_MS)) == pdTRUE)
```

**涉及文件：** [freertos.c](../SmartWatch_HAL/HAL/Core/Src/freertos.c) 第 194 行

**根因分析：**

`ULONG_MAX` 定义在标准 C 头文件 `<limits.h>` 中（展开为 `0xffffffffUL`），而 `freertos.c` 未包含该头文件。当前文件仅包含以下头文件：

```c
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
```

其中 `task.h` 虽在注释中提及 `ULONG_MAX`（见 `/Include/task.h:1907`），但并未主动包含 `<limits.h>`，仅在其注释中说明：

> *"Setting ulBitsToClearOnExit to ULONG_MAX (if limits.h is included) or 0xffffffffUL"*

——也就是说 FreeRTOS 预期使用者自行包含 `<limits.h>` 或直接使用字面值 `0xffffffffUL`。

**解决方案：**

将 `ULONG_MAX` 替换为字面值 `0xffffffffUL`，避免新增头文件依赖：

```diff
-    if (xTaskNotifyWait(0, ULONG_MAX, &key, pdMS_TO_TICKS(FRAME_PERIOD_MS)) == pdTRUE)
+    if (xTaskNotifyWait(0, 0xffffffffUL, &key, pdMS_TO_TICKS(FRAME_PERIOD_MS)) == pdTRUE)
```

| 方案 | 优点 | 缺点 | 选择 |
|------|------|------|------|
| 添加 `#include <limits.h>` | 语义清晰，自文档化 | 增加头文件依赖，全局生效 | ❌ |
| 替换为 `0xffffffffUL` | 零额外依赖，FreeRTOS 文档推荐做法 | 字面值可读性略低 | ✅ |

**验证：** ✅ 编译通过，0 Error，0 Warning

**关联文档：** FreeRTOS `task.h` 第 1907 行注释明确将 `0xffffffffUL` 列为不包含 `limits.h` 时的替代值。

---

### 问题 6：`MPU6050_Calculation_Euler_angles()` 内部双重延时导致采样周期翻倍

**发现阶段：** Phase 5 前代码审查 — 任务栈与功能完整性分析

**现象：**

MPU6050 互补滤波器采样周期约为 **11ms**（~90Hz），而非预期设计的 **5ms**（200Hz），导致传感器数据更新率减半，水平仪/姿态显示响应变慢。

**涉及文件：**

- [menu.c:304](../SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c#L304) — `MPU6050_Calculation_Euler_angles()` 内部第一行
- [freertos.c:253](../SmartWatch_HAL/HAL/Core/Src/freertos.c#L253) — `Task_Sensor` 采样循环中的 `vTaskDelay()`

**根因分析：**

`MPU6050_Calculation_Euler_angles()` 函数顶部存在一段来自裸机版的 `delay_ms(MPU_SAMPLE_DELAY_MS)`（即 5ms DWT 忙等）：

```c
// menu.c:304 — 裸机版遗迹
void MPU6050_Calculation_Euler_angles(void)
{
    delay_ms(MPU_SAMPLE_DELAY_MS);       // ① 5ms DWT 忙等
    MPU6050_GetData(&ax,&ay,&az,&gx,&gy,&gz);
    // ... 互补滤波计算 ...
}
```

与此同时，`Task_Sensor` 在 while(g_SensorActive) 循环末尾也调用了 `vTaskDelay(5ms)`：

```c
// freertos.c:250-254 — FreeRTOS 版定时
while (g_SensorActive)
{
    MPU6050_Calculation_Euler_angles();  // ← 内部已 delay_ms(5)
    vTaskDelay(pdMS_TO_TICKS(MPU_SAMPLE_DELAY_MS));  // ② 再 vTaskDelay(5)
}
```

| 延时点 | 位置 | 延时类型 | 时间 |
|--------|------|---------|------|
| ① `delay_ms(5)` | `menu.c:304` Euler 函数内部 | DWT 忙等（阻塞所有任务） | 5ms |
| MPU6050 I2C 读取 | `MPU6050_GetData()` | I2C 位碰撞（阻塞当前任务） | ~1ms |
| ② `vTaskDelay(5)` | `freertos.c:253` 循环末尾 | FreeRTOS 睡眠（主动让出 CPU） | 5ms |
| **实际周期** | | | **~11ms** |

**为什么裸机版中没问题：** 裸机版没有 `vTaskDelay`，`delay_ms(5)` 就是唯一的采样周期控制——`delay_ms` 阻塞 5ms 后读取数据，数据就恰好是 5ms 间隔的。移植时在 Task_Sensor 中新增了 `vTaskDelay(5)` 负责定时，但遗漏了删除函数内部原有的 `delay_ms(5)`。

**影响分析：**

| 维度 | 预期 | 实际 |
|------|------|------|
| 采样周期 | 5ms | ~11ms |
| 采样频率 | 200Hz | ~90Hz |
| 互补滤波 α=0.9 的收敛时间 | ~25ms（5 个采样） | ~55ms |
| 传感器页面数据更新率 | 每帧刷新 | 约 3 帧才更新一次 |

**解决方案：**

删除 `menu.c` 第 304 行的 `delay_ms(MPU_SAMPLE_DELAY_MS)`，采样周期由 Task_Sensor 的 `vTaskDelay(pdMS_TO_TICKS(MPU_SAMPLE_DELAY_MS))` 单一控制：

```diff
  void MPU6050_Calculation_Euler_angles(void)
  {
-     delay_ms(MPU_SAMPLE_DELAY_MS);       // 删除：裸机版遗迹，FreeRTOS 中由 vTaskDelay 负责
      MPU6050_GetData(&ax,&ay,&az,&gx,&gy,&gz);
      // ... 互补滤波计算 ...
  }
```

| 方案 | 优点 | 风险 | 选择 |
|------|------|------|------|
| 删除 `delay_ms(5)` | 恢复 200Hz 采样率，零额外代码改动 | 🟢 低——vTaskDelay 已保证采样周期 | ✅ |

**验证方法：** 修改后实测传感器页的 Roll/Pitch/Yaw 更新率应明显提升，水平仪响应更平滑。

**修复状态：** ✅ **已修复**（2026-06-25 手动删除 `delay_ms(MPU_SAMPLE_DELAY_MS)`）

---

### 问题 7：`Show_GameOver()` 中 `delay_ms(1000)` 阻塞 Task_UI 长达 1 秒

**发现阶段：** Phase 5 前代码审查 — 任务栈与功能完整性分析

**现象：**

恐龙游戏碰撞结束时，屏幕显示 "Game Over" 后系统冻结 **1 秒**，期间任何按键无响应、页面不渲染。

**涉及文件：**

- [dino.c:195-203](../SmartWatch_HAL/HAL/Core/Src/Hardware/dino.c#L195-L203) — `Show_GameOver()` 函数
- [menu.c:987-1013](../SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c#L987-L1013) — `Task_UI_RenderFrame()` 调用路径

**根因分析：**

`Show_GameOver()` 使用 DWT 忙等 `delay_ms(1000)` 实现 1 秒的"Game Over"画面保持：

```c
// dino.c:195-203
void Show_GameOver(void)
{
    OLED_Clear();
    OLED_ShowString(28, 24, "Game Over", OLED_8X16);
    OLED_Update();
    delay_ms(1000);     // 🔴 1 秒 DWT 忙等，完全阻塞 Task_UI
    OLED_Clear();
    OLED_Update();
}
```

该函数由 `Dino_RenderFrame()` 在碰撞检测为 true 时调用，而 `Dino_RenderFrame()` 由 `Task_UI` 每帧调用：

```
Task_UI (Prio 2)
  └─ xTaskNotifyWait(33ms)
       └─ Task_UI_RenderFrame(key)
            ├─ UI_ProcessKey(key)
            └─ switch(g_CurrentPage)
                 └─ Render_DinoGame()
                      └─ Dino_RenderFrame()       ← 33ms 帧调用
                           └─ if(碰撞) Show_GameOver()
                                ├─ OLED_Update()  ← ~12ms I2C OK
                                ├─ delay_ms(1000) ← 🔴 1000ms 忙等!
                                └─ OLED_Update()  ← ~12ms I2C
```

`delay_ms(1000)` 是 DWT 周期计数忙等，期间：
- **Task_UI** 完全阻塞，无法渲染，无法处理后续按键
- **Task_Input**（优先级 3）可以抢占运行，但 Task_UI 本身没有任何进展
- **空闲任务**无法运行，`__WFI()` 休眠不生效——MCU 在这 1 秒内满速运行

**影响分析：**

| 维度 | 影响 |
|------|------|
| 系统响应 | 1 秒完全冻结，Game Over 后按返回键无效 |
| 功耗 | 1 秒 72MHz 满速忙等，空闲本应有 `__WFI()` 休眠 |
| 用户体验 | 游戏结束 → 1 秒冻结 → 跳回菜单，体验差 |

**解决方案：**

将 `Show_GameOver()` 改为非阻塞状态机——在 Task_UI 的 33ms 帧循环中通过超时计数控制显示持续时间：

```c
// dino.c — 新增倒计时变量
static uint8_t Dino_GameOver_Countdown = 0;

// dino.c — 新增游戏活跃标志（控制 dino_tick 是否继续计分）
uint8_t Dino_GameActive = 0;

// dino.c — 修改后的 Dino_RenderFrame()
uint8_t Dino_RenderFrame(void)
{
    if(Dino_GameOver_Countdown > 0)
    {
        Dino_GameOver_Countdown--;
        Show_Score();
        OLED_ShowString(28, 24, "Game Over", OLED_8X16);
        if(Dino_GameOver_Countdown == 0)
            return 1;             // 倒计时结束，跳转回菜单
        return 0;                 // 仍在显示 Game Over
    }

    // 正常游戏渲染
    if(isColliding(&Barr, &dino))
    {
        Dino_GameOver_Countdown = 30;   // 30 帧 × 33ms ≈ 1 秒
        Dino_GameActive = 0;            // 停止计分+位移
        return 0;                       // 保留碰撞瞬间的画面
    }

    // ... 正常绘制 ...
    return 0;
}

// dino_tick() — 增加 GameActive 守卫
void dino_tick(void)
{
    if(Dino_GameActive == 0) return;   // 游戏已结束，不再更新任何数据
    // ... 原计分/位移逻辑不变 ...
}
```

| 方案 | 优点 | 工作量 |
|------|------|--------|
| 状态机改造 | 零阻塞，保持帧率，按键可打断 | ~30 分钟 |
| 改为 `vTaskDelay(1000)` | 至少让出 CPU 给空闲任务，降低功耗 | 5 分钟 | 
| **已实现：帧计数方案** | **零阻塞 + 零额外代码量** | ✅ |

**实际采用的修复方案：**

在 `dino.c` 中新增一个 `static uint8_t Dino_GameOver_Countdown` 计数器：

- **碰撞时**：`Dino_GameOver_Countdown = 30`，不调用 `Show_GameOver()`，直接 `return 0`（这帧保留碰撞画面，让玩家看到恐龙撞到障碍物的瞬间）
- **后续 30 帧（~1 秒）**：`Dino_RenderFrame()` 的倒计时分支接管，每帧显示分数 + "Game Over" 文字，计数器递减
- **倒计时到 0**：`return 1`，Task_UI 跳转回游戏选择页
- 同时新增 `Dino_GameActive` 标志位，碰撞时清零，`dino_tick()` 中检测到已结束则跳过所有计分/位移逻辑

**涉及改动：**
| 文件 | 变更 |
|------|------|
| `dino.c` | 新增 `Dino_GameOver_Countdown`、`Dino_GameActive` 两个 `static` 变量；`Dino_RenderFrame()` 增加倒计时分支；`dino_tick()` 增加 `GameActive` 守卫；`Game_Init()` 初始化 `GameActive = 1` |
| `dino.h` | 无需修改（`Dino_GameActive` 仅内部使用，已设为 `static`） |

**验证：** 碰撞后恐龙/障碍物/地面/分数全部立即停止，仅 "Game Over" 文字显示 ~1 秒后返回菜单。

**修复状态：** ✅ **已修复**（2026-06-25 手动实现帧计数方案）

**关联问题：** 同时修复 [问题 8](#问题-8show_gameover-中-oldupdate-与-task_ui-重复调用导致单帧-i2c-翻倍) 中的重复 OLED_Update 问题。

---

### 问题 8：`Show_GameOver()` 中 `OLED_Update()` 与 `Task_UI` 重复调用导致单帧 I2C 翻倍

**发现阶段：** Phase 5 前代码审查 — 任务栈与功能完整性分析

**现象：**

恐龙游戏结束时，单帧内两次全屏 OLED I2C 传输（~24-56ms），可能造成该帧渲染超时。

**涉及文件：**

- [dino.c:199](../SmartWatch_HAL/HAL/Core/Src/Hardware/dino.c#L199) — `Show_GameOver()` 内部的 `OLED_Update()`
- [menu.c:1012](../SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c#L1012) — `Task_UI_RenderFrame()` 末尾的 `OLED_Update()`

**根因分析：**

`Show_GameOver()` 内部调用了一次 `OLED_Update()` 将 "Game Over" 文字刷到屏幕，但返回后 `Task_UI_RenderFrame()` 在 switch 语句之后又会调用一次 `OLED_Update()`：

```
Task_UI_RenderFrame(key)
  ├─ if(key) UI_ProcessKey(key)
  ├─ OLED_Clear()
  ├─ switch(g_CurrentPage)
  │    └─ PAGE_DINO_GAME:
  │         └─ Render_DinoGame()
  │              └─ Dino_RenderFrame()
  │                   └─ 碰撞 → Show_GameOver()
  │                        ├─ OLED_Clear()        ← 清 buffer
  │                        ├─ OLED_ShowString()   ← 写 "Game Over"
  │                        ├─ OLED_Update()       ← 🔴 第一次 I2C 传输 (~12-28ms)
  │                        └─ delay_ms(1000)
  └─ OLED_Update()                                 ← 🔴 第二次 I2C 传输 (~12-28ms)
```

两次 `OLED_Update()` 之间没有 `OLED_Clear()`（buffer 内容保持不变），传输的是相同的数据，纯属浪费。

**影响分析：**

| 项目 | 第一次 OLDE_Update | 第二次 OLDE_Update | 合计 |
|------|-------------------|-------------------|------|
| I2C 传输时间 | ~12-28ms | ~12-28ms | **~24-56ms** |
| 33ms 帧预算 | — | — | 超出预算风险 |

如果 `OLED_Update()` 单次传输已达 ~28ms，连续两次将超过 33ms 的帧周期，可能导致下一次 `xTaskNotifyWait()` 被延迟触发，帧率短暂下降。

**解决方案：**

移除 `Show_GameOver()` 中的 `OLED_Update()` 调用，仅操作 OLED buffer。Task_UI 的末尾统一 `OLED_Update()` 会负责将内容刷到屏幕：

```diff
  void Show_GameOver(void)
  {
      OLED_Clear();
      OLED_ShowString(28, 24, "Game Over", OLED_8X16);
-     OLED_Update();          // 删除：Task_UI 末尾统一 Update
      delay_ms(1000);
      OLED_Clear();
-     OLED_Update();          // 删除：同上
  }
```

| 方案 | 优点 | 风险 |
|------|------|------|
| 移除两个 `OLED_Update()` | 消除重复传输，恢复 33ms 帧预算 | 🟢 低——Task_UI 已保证每帧末尾有一次 OLED_Update |
| 改为 `OLED_UpdateArea()` 局部刷新 | 更节省 I2C 带宽 | 🟡 需要额外计算 Game Over 文字区域坐标 |

**验证方法：** 游戏结束时逻辑帧时间应降至单次 OLED_Update 的水平（~12-28ms），不再出现帧率抖动。

**修复状态：** ✅ **已修复**（2026-06-25，与问题 7 同步修复）
- `Show_GameOver()` 内部的 2 个 `OLED_Update()` 已注释（dino.c:203-206）
- `Show_GameOver()` 本身也不再被调用（dino.c:295 已注释），Game Over 显示由问题 7 的帧计数方案统一接管
- 单帧 I2C 传输从 ~24-56ms 降至 ~12-28ms，恢复 33ms 帧预算

---

### 问题 9：OLED I2C 被抢占 — vTaskSuspendAll 保护 I2C 时序

**现象：**

在 Phase 5 回归测试中发现，当 `Task_UI` 正在执行 `OLED_Update()`（~12ms 软件 I2C bit-banging）期间，
如果 `Task_Input`（优先级 3 > 2）被 TIM2 ISR 通知而解除阻塞，调度器会执行上下文切换，
导致 I2C 位带传输被中断。虽然软件 I2C 不受硬件中断影响（已关中断），
但**任务级抢占**会导致单帧总 I2C 时间超过 33ms 帧预算：

```
正常帧: [Render ~1ms][OLED_Update ~12ms] = ~13ms
被抢占帧: [Render ~1ms][OLED_Update ~3ms] → Task_Input 抢占执行 ← [OLED_Update ~9ms 续传] = ~13ms + Task_Input执行时间
```

被抢占本身不会破坏 I2C 时序（I2C 的状态在任务栈中保存），但会导致：
- 帧周期抖动（frame time jitter）
- 如果 Task_Input 执行时间较长，可能超过 33ms 帧预算

**技术分析：**

| 项目 | 说明 |
|------|------|
| 根因 | Task_UI (prio 2) 的 I2C 传输被 Task_Input (prio 3) 抢占 |
| 是否破坏 I2C 时序 | ❌ 不破坏（任务上下文保存 I2C 状态） |
| 是否影响帧率 | 🟡 可能 — 单帧执行时间增加 Task_Input 开销 |
| Debug 页测量影响 | 🟡 存在 — Render_Debug 本身调用 uxTaskGetStackHighWaterMark 等 API，这些 API 调用 `taskENTER_CRITICAL`，当 vTaskSuspendAll 已挂起调度器时，critical section 行为与正常情况不同 |

**解决方案：**

在 `OLED_Update()` 前后用 `vTaskSuspendAll()` / `xTaskResumeAll()` 挂起调度器，
确保 I2C 位带传输连续完成，不被任务切换中断：

```diff
  void Task_UI_RenderFrame(uint8_t key)
  {
      // ... dispatch render functions ...
+     vTaskSuspendAll();              /* 挂起调度器 → I2C 传输不可被抢占 */
      OLED_Update();                  /* ~12ms 连续 I2C 传输，无任务切换 */
+     xTaskResumeAll();               /* 恢复调度器，执行待处理的 PendSV 切换 */
  }
```

| 方案 | 优点 | 风险 |
|------|------|------|
| ✅ **vTaskSuspendAll** | 最简单，只禁任务切换不禁中断；ISR 仍可响应，I2C 时序完整 | 🟢 低——挂起时间仅 ~12ms |
| ❌ taskENTER_CRITICAL | 也会禁中断，软件 I2C 的 NOP 延时仍被中断影响 | 🟡 中断被禁可能影响按键采样 |
| ❌ 提高 Task_UI 优先级 | 与 Task_Input 同优先级（都是 3），按键响应延迟 | 🔴 牺牲按键响应速度 |

**为什么 vTaskSuspendAll 是正确选择：**

| 特性 | vTaskSuspendAll | taskENTER_CRITICAL |
|------|:---------------:|:------------------:|
| 禁止任务切换 | ✅ | ✅ |
| 禁止中断 | ❌ | ✅ |
| ISR 仍可响应按键 | ✅ | ❌ |
| TIM2 ISR 仍可更新 dino_tick | ✅ | ❌ |
| 适合 I2C 位带传输 | ✅ | ❌（过度杀伤） |

**修复状态：** ✅ **已修复**（2026-06-30）
- `menu.c:Task_UI_RenderFrame()`: `OLED_Update()` 被 `vTaskSuspendAll` / `xTaskResumeAll` 包裹
- I2C 传输期间无任务切换，帧周期稳定在 ~13ms（无按键时）
- ISR 不受影响，TIM2 的按键采样和恐龙游戏逻辑正常执行

---

### 问题 10：恐龙游戏碰撞后无法二次进入 — `Dino_GameOver_Countdown` 未重置 + 渲染顺序缺陷

**发现阶段：** Phase 5 前代码审查 — 游戏功能回归测试

**现象：**

恐龙游戏碰撞障碍物后显示 "Game Over" 界面并返回游戏选择页，但再次选择恐龙游戏时，第一帧就立即显示 "Game Over" 并被踢回选择页，无法正常重新开始游戏。

**涉及文件：**

- [dino.c:57-62](../SmartWatch_HAL/HAL/Core/Src/Hardware/dino.c#L57-L62) — `Game_Init()` 函数
- [dino.c:277-305](../SmartWatch_HAL/HAL/Core/Src/Hardware/dino.c#L277-L305) — `Dino_RenderFrame()` 函数

**根因分析（两个子问题）：**

**子问题 A：`Dino_GameOver_Countdown` 未在 `Game_Init()` 中重置（主因）**

`Dino_GameOver_Countdown` 是 `static` 局部变量，在 `Game_Init()` 中没有被重置为 0：

```c
// 修复前：
void Game_Init(void)
{
    Dino_GameActive = 1;
    Dino_Score = Dino_ScoreCount = ... = 0;  // 重置了所有变量，唯独漏了 Countdown
    Dino_JumpRequest = 0;
}
```

当用户第一次游戏碰撞后，`Dino_GameOver_Countdown` 被设为 30 并开始递减。如果用户在倒计时中途（例如 Countdown=5）通过其他方式退出游戏（或被倒计时归零后返回选择页），该变量可能残留非零值。二次进入游戏时 `Game_Init()` 虽重置了其他变量，但 `Dino_GameOver_Countdown` 仍然 > 0，导致 `Dino_RenderFrame()` 第一帧就进入 Game Over 倒计时分支：

```c
if(Dino_GameOver_Countdown > 0)   // ← 残留值 > 0，立即进入
{
    Dino_GameOver_Countdown--;
    // ... 显示 "Game Over" ...
    if(Dino_GameOver_Countdown == 0)
        return 1;                 // ← 返回游戏结束信号
}
return 0;
```

**子问题 B：渲染在碰撞检测的 else 分支中执行（次因）**

原代码结构为 `if(碰撞) { 设置倒计时 } else { 渲染所有对象 }`。这意味着：

1. 碰撞帧不渲染任何对象，`Barr` 和 `dino` 边界结构体不更新
2. 如果 `Game_Init()` 后全局 `Barr`/`dino` 仍持有上次游戏的残留边界值（例如上一局结束时恐龙和障碍物恰好重叠），存在误判碰撞的风险
3. 碰撞发生的瞬间玩家看不到恐龙和障碍物重叠的画面

**影响分析：**

| 维度 | 影响 |
|------|------|
| 可用性 | 🔴 **致命** — 游戏完全无法二次进入，等同于一次性游戏 |
| 用户体验 | 选择恐龙游戏 → 闪一下 "Game Over" → 立即退回选择页，无任何交互机会 |
| 频率 | 100% 复现 |

**解决方案：**

**修复 A：** 在 `Game_Init()` 中增加 `Dino_GameOver_Countdown = 0;`，确保每次重新进入游戏时倒计时从零开始。

**修复 B：** 将渲染逻辑提到碰撞检测之前，每帧都先渲染所有对象（同时更新边界结构体），再进行碰撞检测。消除 `if-else` 分支，简化控制流。

```diff
 void Game_Init(void)
 {
     Dino_GameActive = 1;
+    Dino_GameOver_Countdown = 0;
     Dino_Score = ... = 0;
     Dino_JumpRequest = 0;
 }

 uint8_t Dino_RenderFrame(void)
 {
     if(Dino_GameOver_Countdown > 0) { ... }

-    if(isColliding(&Barr, &dino))
-    {
-        Dino_GameOver_Countdown = 30;
-        Dino_GameActive = 0;
-    }
-    else
-    {
-        Show_Score();
-        Show_Ground();
-        Show_Barrier();
-        Show_Cloud();
-        Show_Dino();
-    }
+    /* 先更新所有对象位置，再进行碰撞检测 */
+    Show_Score();
+    Show_Ground();
+    Show_Barrier();
+    Show_Cloud();
+    Show_Dino();
+
+    if(isColliding(&Barr, &dino))
+    {
+        Dino_GameOver_Countdown = 30;
+        Dino_GameActive = 0;
+    }
+
     return 0;
 }
```

| 方案 | 优点 | 风险 |
|------|------|------|
| ✅ 修复 A + B 组合 | 根治重置问题 + 消除残留碰撞风险 + 控制流更简洁 | 🟢 低——改动仅 dino.c 一个文件 |

**验证方法：** 多次进入游戏 → 碰撞 → Game Over → 返回 → 再次进入游戏，确认每次都能正常开始新游戏。

**修复状态：** ✅ **已修复**（2026-07-01）
- `dino.c:Game_Init()`: 新增 `Dino_GameOver_Countdown = 0;`
- `dino.c:Dino_RenderFrame()`: 渲染前置到碰撞检测之前，消除 `else` 分支

**关联问题：** 本问题与 [问题 7](#问题-7show_gameover-中-delay_ms1000-阻塞-task_ui-长达-1-秒)（Game Over 阻塞 1 秒）和 [问题 8](#问题-8show_gameover-中-oldupdate-与-task_ui-重复调用导致单帧-i2c-翻倍)（重复 OLED_Update）同属恐龙游戏模块的 FreeRTOS 适配问题，三者共同完成了游戏结束流程的非阻塞改造。

---

## 附录：关键文件路径速查

| 文件 | 路径 | 备注 |
|------|------|------|
| main.c | `SmartWatch_HAL/HAL/Core/Src/main.c` | 入口 + 时钟配置 + FreeRTOS 初始化 |
| stm32f1xx_it.c | `SmartWatch_HAL/HAL/Core/Src/stm32f1xx_it.c` | TIM2 ISR + SysTick ISR (含 FreeRTOS tick) |
| stm32f1xx_it.h | `SmartWatch_HAL/HAL/Core/Inc/stm32f1xx_it.h` | 中断声明（SVC/PendSV 由 FreeRTOS 接管） |
| stm32f1xx_hal_conf.h | `SmartWatch_HAL/HAL/Core/Inc/stm32f1xx_hal_conf.h` | `USE_RTOS=0U`, `TICK_INT_PRIORITY=15U` |
| stm32f1xx_hal_msp.c | `SmartWatch_HAL/HAL/Core/Src/stm32f1xx_hal_msp.c` | 外设 MSP 初始化（含 PendSV/TIM2 优先级） |
| stm32f1xx_hal_def.h | `SmartWatch_HAL/HAL/Drivers/STM32F1xx_HAL_Driver/Inc/stm32f1xx_hal_def.h` | `__HAL_LOCK` 宏（2016 版不支持 RTOS） |
| FreeRTOSConfig.h | `SmartWatch_HAL/HAL/Core/Inc/FreeRTOSConfig.h` | FreeRTOS 内核参数配置 |
| freertos.c | `SmartWatch_HAL/HAL/Core/Src/freertos.c` | CubeMX 生成的 FreeRTOS 初始化 |
| HAL.ioc | `SmartWatch_HAL/HAL/HAL.ioc` | CubeMX 工程配置文件 |
| HAL.uvprojx | `SmartWatch_HAL/HAL/MDK-ARM/HAL.uvprojx` | Keil MDK 工程文件 |
| delay.c | `SmartWatch_HAL/HAL/Core/Src/delay.c` | DWT 微秒延时 |
| Key.c | `SmartWatch_HAL/HAL/Core/Src/Hardware/Key.c` | 按键驱动（临界区保护） |
| OLED.c | `SmartWatch_HAL/HAL/Core/Src/Hardware/OLED.c` | OLED 驱动（软件 I2C） |
| MyI2C.c | `SmartWatch_HAL/HAL/Core/Src/MyI2C.c` | 软件 I2C 协议 |
| menu.c | `SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c` | 页面函数 + 帧率控制 |
| menu.h | `SmartWatch_HAL/HAL/Core/Inc/Hardware/menu.h` | 帧率控制宏定义 |
| port.c | `SmartWatch_HAL/HAL/Middlewares/Third_Party/FreeRTOS/Source/portable/RVDS/ARM_CM3/port.c` | FreeRTOS Cortex-M3 移植层 |
| heap_4.c | `SmartWatch_HAL/HAL/Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c` | FreeRTOS 内存管理 |
