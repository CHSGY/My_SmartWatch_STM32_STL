# FreeRTOS 移植记录

> **评估日期：** 2026-06-14（可行性分析）
> **Phase 1 实施日期：** 2026-06-16
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
- [Phase 1 实施记录](#五phase-1-实施记录--cubemx-集成-freertos)
- [Phase 1 遇到的问题](#六phase-1-遇到的问题与解决方案)
- [结论](#七结论)

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

### 1.4 临界区保护模式已建立 ✅

**涉及文件：** [Key.c](../SmartWatch_HAL/HAL/Core/Src/Hardware/Key.c) — `Key_GetNum()`

问题十修复后，`Key_GetNum()` 已使用 `__disable_irq()` / `__enable_irq()` 保护读-改-写临界区：

```c
uint8_t Key_GetNum(void)
{
    uint8_t Temp;
    __disable_irq();
    if(Key_Num) {
        Temp = Key_Num;
        Key_Num = 0;
        __enable_irq();
        return Temp;
    }
    __enable_irq();
    return 0;
}
```

迁移到 FreeRTOS 时只需替换为：

```c
taskENTER_CRITICAL();
// ... 临界区代码 ...
taskEXIT_CRITICAL();
```

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
- 每个页面一个独立任务
- `vTaskDelayUntil()` 替代帧率控制
- 空闲任务钩子中执行 `__WFI()`

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

将每个页面函数改造为**独立任务 + 状态机**模型：

```c
// FreeRTOS 架构：每个页面是一个独立任务
void Task_Clock(void *pvParameters)
{
    TickType_t lastWakeTime = xTaskGetTickCount();

    while(1)
    {
        // 等待按键通知（非阻塞）
        uint32_t key;
        if(xTaskNotifyWait(0, ULONG_MAX, &key, pdMS_TO_TICKS(33)) == pdTRUE)
        {
            // 处理按键
        }

        // 帧率控制
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(FRAME_PERIOD_MS));

        // 绘制
        Show_Clock_UI();
        OLED_Update();
    }
}
```

| 项目 | 当前架构 | FreeRTOS 架构 |
|------|---------|--------------|
| 页面切换 | 函数嵌套调用 | 任务挂起/恢复 |
| 按键获取 | `Key_GetNum()` 轮询 | `xTaskNotifyWait()` 阻塞等待 |
| 帧率控制 | `HAL_GetTick()` 轮询 | `vTaskDelayUntil()` 精确调度 |
| 休眠 | `__WFI()` | 空闲任务自动 `WFI` |

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

**SRAM 使用预估：**

| 项目 | 预估用量 |
|------|---------|
| FreeRTOS 内核数据 (TCB、队列、定时器) | ~1-2 KB |
| Task_Clock 栈 (首页时钟) | ~512-1024 字节 |
| Task_Menu 栈 (菜单页面) | ~512-1024 字节 |
| Task_Stopwatch 栈 (秒表) | ~512 字节 |
| Task_Sensors 栈 (MPU6050) | ~512 字节 |
| Task_Dino 栈 (恐龙游戏) | ~1024 字节 |
| 全局变量 (现有 ~3-5KB) | ~3-5 KB |
| HAL 缓冲区 | ~1 KB |
| **总计预估** | **~8-15 KB** |

20KB SRAM 够用但**不宽裕**。关键策略：

| 策略 | 说明 |
|------|------|
| 精确配置栈大小 | 使用 `uxTaskGetStackHighWaterMark()` 调优 |
| 避免递归 | 所有函数禁止递归调用 |
| 减少大数组 | 检查是否有不必要的全局缓冲区 |
| OLED 显存 | 当前显存在 OLED 驱动 IC 内（外部），不占用 SRAM |

---

### 2.6 🟢 HAL_Delay() 需要替换

代码中可能存在 `HAL_Delay()` 调用，该函数基于 SysTick 轮询，在 FreeRTOS 下会导致：
1. SysTick 被 FreeRTOS 接管时 `HAL_Delay()` 失效
2. 轮询浪费 CPU 时间

**替换方案：** `HAL_Delay(ms)` → `vTaskDelay(pdMS_TO_TICKS(ms))`

**搜索命令：** 在工程中 grep 查找所有 `HAL_Delay(` 出现的位置。

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

### 4.1 架构变化

```
当前架构 (裸机 Super Loop):

main()
├── HAL_Init()
├── SystemClock_Config()
├── MX_GPIO_Init() / MX_ADC1_Init() / MX_TIM2_Init() / MX_RTC_Init()
├── Key_Init() / OLED_Init() / MPU6050_Init() / MyRTC_Init()
├── HAL_TIM_Base_Start_IT(&htim2)
├── while(1)
│   ├── Battery_Show_UI()
│   ├── First_Page_Clock()   ← while(1) 独占
│   ├── Menu_Page()          ← while(1) 独占
│   └── SettingPage()        ← while(1) 独占
│
TIM2_IRQHandler (1ms)
├── Key3_Tick()
├── KeyTick()
├── StopClock_Tick()
└── dino_tick()


FreeRTOS 架构:

main()
├── HAL_Init()
├── SystemClock_Config()
├── MX_GPIO_Init() / ... / MX_RTC_Init()
├── Key_Init() / OLED_Init() / MPU6050_Init() / MyRTC_Init()
├── 创建任务队列
│   ├── Task_Clock      (优先级 2)
│   ├── Task_Menu       (优先级 2)
│   ├── Task_Stopwatch  (优先级 1)
│   ├── Task_Dino       (优先级 1)
│   ├── Task_Sensors    (优先级 1)
│   └── Task_Input      (优先级 3) ← 按键处理
├── vTaskStartScheduler()
└── (永不返回)

TIM2_IRQHandler (1ms)
├── Key3_Tick()           ← 仍可在 ISR 中执行
├── KeyTick()
├── StopClock_Tick()
├── dino_tick()
└── vTaskNotifyGiveFromISR()  ← 通知 Task_Input

空闲任务
└── __WFI()              ← 自动休眠
```

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

#### Phase 2：改造按键驱动

| 任务 | 说明 | 预估时间 |
|------|------|---------|
| 创建 `Task_Input` | 负责按键处理，使用 `xTaskNotifyWait()` 阻塞 | 1 天 |
| 修改 `Key_GetNum()` | 替换 `__disable_irq()` → `taskENTER_CRITICAL()` |  |
| TIM2 ISR 中发送通知 | 使用 `vTaskNotifyGiveFromISR()` |  |

#### Phase 3：页面函数改造为独立任务

| 任务 | 说明 | 预估时间 |
|------|------|---------|
| 拆分 9 个页面函数 | 每个页面创建独立任务 | 2-3 天 |
| 实现页面切换机制 | 任务挂起/恢复 + 任务通知 |  |
| 移植帧率控制 | `vTaskDelayUntil()` 替代 `HAL_GetTick()` 轮询 |  |
| 移植 `__WFI()` | 空闲任务钩子中自动执行 |  |

#### Phase 4：I2C 阻塞优化

| 任务 | 说明 | 预估时间 |
|------|------|---------|
| 创建高优先级 I2C 任务 | 封装所有 OLED/MPU6050 I2C 操作 | 1 天 |
| 评估硬件 I2C 迁移 | 研究 STM32F103 I2C1/I2C2 外设 | (可选) |

#### Phase 5：回归测试与栈调优

| 任务 | 说明 | 预估时间 |
|------|------|---------|
| 栈使用量分析 | `uxTaskGetStackHighWaterMark()` | 1-2 天 |
| 功能回归测试 | 所有页面、按键、传感器、游戏 |  |
| 功耗对比测试 | 移植前后电流对比 |  |

### 4.3 总预估

| 指标 | 值 |
|------|-----|
| **总工作量** | **5-8 个工作日** |
| 新增/修改文件 | ~15-20 个 |
| 核心改动量 | ~300-500 行 C 代码 |
| 风险等级 | 中等 |

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
| **MAX_PRIORITIES** | `5` | 内存 | 5 个优先级够用（见 5.2.4 优先级分配），每个优先级增加一个就绪链表，少一级省 ~20 字节 RAM |
| **MINIMAL_STACK_SIZE** | `128` (512 bytes) | 内存 | 给 idle task + timer task 用，Cortex-M3 上下文 16 字 + 嵌套中断栈帧，余量充足 |
| **MAX_TASK_NAME_LEN** | `16` | 调试 | 够写 `Task_Stopwatch` 等名字 |
| **USE_16_BIT_TICKS** | `0` | 时间基准 | 32-bit tick，1ms tick → 溢出时间 49.7 天，远超手表使用场景 |
| **IDLE_SHOULD_YIELD** | `1` | 调度 | 有同优先级就绪任务时 idle task 主动让出 CPU |

#### 5.2.3 同步与通信

| 参数 | 值 | 理由 |
|:---|:---|:---|
| **USE_MUTEXES** | `1` | I2C/OLED 资源互斥保护（软件 I2C 不能同时被两个任务操作） |
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

**10KB 内存详细计算：**

| 项目 | 估算 | 累计 |
|:---|:---|:---|
| 空闲任务栈 | 512 bytes | 512 |
| 定时器任务栈 | 1024 bytes | 1536 |
| Task_Clock 栈 | 768 bytes | 2304 |
| Task_Menu 栈 | 768 bytes | 3072 |
| Task_Setting 栈 | 768 bytes | 3840 |
| Task_Stopwatch 栈 | 512 bytes | 4352 |
| Task_Sensors 栈 | 512 bytes | 4864 |
| Task_Dino 栈 | 1024 bytes | 5888 |
| Task_Input 栈 | 384 bytes | 6272 |
| Task_Flashlight 栈 | 384 bytes | 6656 |
| Task_Emoji 栈 | 512 bytes | 7168 |
| Task_Gradienter 栈 | 512 bytes | 7680 |
| Task_GameSelect 栈 | 384 bytes | 8064 |
| 13 个 TCB（~80B/个） | ~1040 bytes | 9104 |
| 队列/互斥量 | ~500 bytes | 9604 |
| **安全余量** | **~636 bytes** | **10240** |

> 20KB SRAM 总预算：10KB FreeRTOS heap + 1KB startup 栈 + 0.5KB startup heap + ~5KB 全局变量 ≈ 16.5KB/20KB，还有 3.5KB 系统余量。

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
| **Task_Input** (按键) | **3** | 实时响应按键，不能被任何 UI 任务阻塞 |
| **Task_Clock** (时钟首页) | **2** | 默认显示页面 |
| **Task_Menu** (菜单) | **2** | 与时钟同级，同一时刻只有一个活跃 |
| **Task_Setting** (设置) | **2** | 同上 |
| **Task_Stopwatch** (秒表) | **1** | 后台运行，不阻塞 UI |
| **Task_Dino** (恐龙游戏) | **1** | 游戏渲染，不阻塞 UI |
| **Task_Sensors** (MPU6050) | **1** | 传感器采样，不阻塞 UI |
| 其他页面任务 | **1** | 低优先级，不阻塞核心功能 |

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

## 七、结论

### 综合评估：✅ 可以移植，条件成熟

| 维度 | 评分 | 说明 |
|------|------|------|
| 硬件资源 | 🟡 够用 | 20KB RAM 够用但需精打细算，栈大小需精确配置 |
| 时钟系统 | 🟢 完全兼容 | 72MHz + DWT 延时方案确保零冲突 |
| 驱动兼容性 | 🟢 大部分无需修改 | OLED/MyI2C/RTC/ADC/GPIO 均与 OS 无关 |
| 架构适配 | 🟡 需要改造 | 页面 while(1) → 任务状态机是主要工作量 |
| I2C 阻塞 | 🟡 可接受 | 短期用专用任务 + 高优先级，中期迁移硬件 I2C |

### 移植的收益

1. **代码结构清晰** — 每个页面独立任务，职责单一
2. **实时响应** — 按键任务高优先级，消除当前轮询延迟
3. **功耗优化** — 空闲任务自动 `WFI`，比当前 `__WFI()` 模式更精确
4. **扩展性** — 未来添加 BLE、SPI Flash、心率传感器等只需新增任务
5. **调试便利** — FreeRTOS 的任务列表、栈监控等调试工具

### 移植的成本

1. **工作量** — 5-8 个工作日
2. **架构改动** — 页面函数需要重构为状态机
3. **回归风险** — 需全面测试所有功能
4. **Flash 占用** — FreeRTOS 内核约 6-8KB

### 建议

> **如果当前功能稳定且无新增需求，可以不急于移植。** 问题十五修复后，帧率控制 + `__WFI()` 已解决主要功耗问题（预估续航改善约 3 倍）。
>
> **如果计划添加多线程功能（如 BLE 通信、传感器数据后台采集），建议移植。** 推荐从 Phase 1 开始，快速验证 OS 启动，再逐步改造应用层。

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
