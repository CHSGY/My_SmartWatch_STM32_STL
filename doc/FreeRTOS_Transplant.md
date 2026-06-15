# FreeRTOS 移植可行性评估

> **评估日期：** 2026-06-14
> **目标 MCU：** STM32F103RBT6 (Cortex-M3)
> **当前工程：** SmartWatch_HAL (HAL 库版本)

---

## 目录

- [项目概览](#项目概览)
- [已具备的兼容条件](#一已具备的-freeertos-兼容条件)
- [需要解决的关键问题](#二需要解决的关键问题)
- [不需要修改的部分](#三不需要修改的部分)
- [推荐移植方案](#四推荐移植方案与工作量)
- [结论](#五结论)

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

CubeMX 生成的 HAL 库天然支持 FreeRTOS。关键配置项（第 133 行）：

```c
#define USE_RTOS                     0U    /* 改为 1U 即可适配 RTOS */
```

HAL 库提供完整的 `HAL_Delay()` 和 `HAL_GetTick()` 抽象层，迁移时只需在 SysTick 或 TIM 中断中增加 OS tick 调用。

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

### 2.4 🟡 HAL 的 SysTick 与 FreeRTOS 的 SysTick 冲突

**涉及文件：** [stm32f1xx_it.c:185-194](../SmartWatch_HAL/HAL/Core/Src/stm32f1xx_it.c#L185-L194)

当前 SysTick 仅用于 `HAL_IncTick()`：

```c
void SysTick_Handler(void)
{
    HAL_IncTick();
}
```

FreeRTOS 默认也使用 SysTick 作为系统 tick 源。需要解决冲突。

**可选方案对比：**

| 方案 | 描述 | 工作量 | 优点 | 缺点 |
|------|------|--------|------|------|
| **A（推荐）** | FreeRTOS 使用 TIM3 作 tick 源，SysTick 留给 HAL | 小 | HAL 兼容性最好，互不干扰 | 多占用一个 TIM 外设 |
| **B** | FreeRTOS 接管 SysTick，`SysTick_Handler()` 中同时调用 `HAL_IncTick()` 和 `xPortSysTickHandler()` | 很小 | 不占用额外外设 | 需确保 `HAL_IncTick()` 在 ISR 中安全 |
| **C** | 将 `HAL_IncTick()` 迁移到 TIM 中断中 | 中 | 最彻底的分离 | 需要修改 HAL 底层 |

**方案 A 实现：**
1. CubeMX 中配置 TIM3 为 1ms 周期
2. 在 `tim.c` 或 `stm32f1xx_it.c` 中添加 `xPortSysTickHandler()` 调用
3. 修改 `FreeRTOSConfig.h` 中 `configSYSTICK_CLOCK_HZ` 和 `configTICK_RATE_HZ`

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

#### Phase 1：CubeMX 集成 FreeRTOS 组件

| 任务 | 说明 | 预估时间 |
|------|------|---------|
| 在 CubeMX 中勾选 FreeRTOS | 使用 CMSIS_V1 或 CMSIS_V2 封装层 | 0.5 天 |
| 配置 TIM3 作为 FreeRTOS tick 源 | 避免与 HAL 的 SysTick 冲突 |  |
| 修改 `USE_RTOS` 为 `1U` | `stm32f1xx_hal_conf.h` 第 133 行 |  |
| 验证 OS 启动 | 创建一个简单任务，LED 闪烁 |  |

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

## 五、结论

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
| main.c | `SmartWatch_HAL/HAL/Core/Src/main.c` | 入口 + 时钟配置 |
| stm32f1xx_it.c | `SmartWatch_HAL/HAL/Core/Src/stm32f1xx_it.c` | TIM2 ISR + SysTick ISR |
| stm32f1xx_hal_conf.h | `SmartWatch_HAL/HAL/Core/Inc/stm32f1xx_hal_conf.h` | `USE_RTOS` 配置 |
| delay.c | `SmartWatch_HAL/HAL/Core/Src/delay.c` | DWT 微秒延时 |
| Key.c | `SmartWatch_HAL/HAL/Core/Src/Hardware/Key.c` | 按键驱动（临界区保护） |
| OLED.c | `SmartWatch_HAL/HAL/Core/Src/Hardware/OLED.c` | OLED 驱动（软件 I2C） |
| MyI2C.c | `SmartWatch_HAL/HAL/Core/Src/MyI2C.c` | 软件 I2C 协议 |
| menu.c | `SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c` | 页面函数 + 帧率控制 |
| menu.h | `SmartWatch_HAL/HAL/Core/Inc/Hardware/menu.h` | 帧率控制宏定义 |
