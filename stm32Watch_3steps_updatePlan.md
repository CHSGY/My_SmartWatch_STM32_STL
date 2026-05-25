# STM32 智能手表项目：三步优化迁移计划

> 项目仓库：https://github.com/CHSGY/My_SmartWatch_STM32_STL
> MCU：STM32F103C8T6 (Cortex-M3, 72MHz, 64KB Flash, 20KB SRAM)
> 当前状态：SPL (标准外设库) + 裸机超级循环 + Keil MDK (ARMCC)

---

## 推荐执行顺序

```
Phase 1: HAL 库迁移     ←──── 必须先做，地基
    ↓
Phase 2: FreeRTOS 引入  ←──── 依赖稳定的 HAL 层
    ↓
Phase 3: 工程规范化      ←──── 架构稳定后再重构
```

---

## Phase 1 · HAL 库迁移（⭐ 优先级最高，必须最先做）

### 为什么必须先做

- **FreeRTOS 依赖 HAL**：FreeRTOS 的 tick 源（`HAL_Delay` / `HAL_SYSTICK_Callback`）需要 HAL 的 Systick 配置，SPL 没有标准化的 Systick 管理。
- **工程优化依赖稳定驱动**：在 SPL 上用 CMake 重构目录，等 HAL 迁移时又要改一遍，白做。
- **风险最小**：一个外设一个外设地替换，每替换一个就验证一个，随时能回退。

### 具体任务

| 步骤 | 内容 | 输出 |
|---|---|---|
| 1.1 | CubeMX 生成 HAL 工程骨架 | `.ioc` 配置文件 + CubeMX 生成代码 |
| 1.2 | 验证 HAL_GPIO | Key、LED 驱动替换为 HAL 接口 |
| 1.3 | 验证 HAL_I2C | OLED / MPU6050 / DS1307 改用硬件 I2C（软件 I2C 保留为 fallback） |
| 1.4 | 验证 HAL_ADC | 电池检测改用 DMA 模式，消除阻塞等待 |
| 1.5 | 验证 HAL_TIM | 1ms 定时器中断改用 `HAL_TIM_Base_Start_IT` |
| 1.6 | 验证 HAL_RTC | DS1307 保持软件 I2C，或替换为 STM32 内置 RTC + 后备电池 |
| 1.7 | 功能回归测试 | 每个子功能（时钟、菜单、游戏、水平仪、表情包）全部跑通 |

### 当前项目的主要问题（Phase 1 解决方向）

| 当前问题 | HAL 方案 |
|---|---|
| SPL 已停产，无官方维护 | HAL 是 ST 当前标准库，持续更新 |
| ARMCC 编译器绑定 | HAL + GCC 兼容，可切换到 ARM GCC |
| 软件 I2C + 10µs 阻塞延迟 | HAL_I2C_Master_Transmit 中断/DMA 模式 |
| AD_GetValue() 阻塞等待 EOC | HAL_ADC_Start_DMA + 回调 |
| NVIC 分组随意设置 | HAL 统一初始化 NVIC |

---

## Phase 2 · FreeRTOS 引入（⭐ 等 Phase 1 稳定后再做）

### 为什么 Phase 1 之后做

- HAL 的 `HAL_IncTick()` 可以直接作为 FreeRTOS 的 tick hook。
- HAL 中断处理规范统一（`HAL_TIM_PeriodElapsedCallback` 等回调函数）。
- CubeMX 一键集成 FreeRTOS（勾选即可，生成即用）。

### 具体任务

| 步骤 | 内容 |
|---|---|
| 2.1 | 在 `.ioc` 中启用 FreeRTOS（CMSIS_V2 封装），配置 heap 大小 |
| 2.2 | 创建任务任务： |
| | - `UI_Task`：显示刷新（OLED 操作） |
| | - `Sensor_Task`：MPU6050 数据采集 + 电池电压采样 |
| | - `Input_Task`：按键扫描 + 事件分发 |
| | - `Game_Task`：恐龙游戏循环 |
| 2.3 | 建立 Queue：`KeyEventQueue` 分发按键事件 |
| 2.4 | 从 ISR 中移除业务逻辑： |
| | - 键盘扫描移到 `Input_Task`（用 `vTaskDelay(20ms)` 轮询） |
| | - 游戏逻辑从 TIM2 ISR 移到 `Game_Task` |
| 2.5 | 使用 `xSemaphoreCreateBinary` 保护 OLED 显存 |
| 2.6 | Tickless 模式：电池供电场景启用 `configUSE_TICKLESS_IDLE` |

### 当前项目的主要问题（Phase 2 解决方向）

| 当前问题 | FreeRTOS 方案 |
|---|---|
| 超级循环（main while(1)）顺序轮询 | 多任务并发，独立调度 |
| 按键扫描在 1ms ISR 中执行 | Input_Task 每 20ms 轮询，ISR 只 `xSemaphoreGiveFromISR` |
| 游戏动画在 1ms ISR 中直接渲染 | 通过 Queue 发事件给 Game_Task |
| 电池采样 3000 次阻塞约 300ms | Sensor_Task 每 1s 采样一次，不影响其他任务 |
| `while(1)` 阻塞式菜单导航 | 事件驱动状态机，菜单切换立刻响应 |
| 无任务优先级管理 | 高优先级：Input（响应按键），中：UI，低：Sensor |

---

## Phase 3 · 工程规范化（⭐ 最后做，收益最大）

### 为什么最后做

- 模块划分依赖功能边界清晰（Phase 2 做完后，任务划分就是天然的模块边界）。
- CMake 构建需要确定的目录结构（HAL/FreeRTOS 确定后不会再大改）。
- 状态机替代超级循环在任务化后更自然。

### 具体任务

#### 3.1 目录结构重构

```
My_SmartWatch_STM32/
├── CMakeLists.txt                    # 顶层 CMake 构建
├── .github/
│   └── workflows/
│       └── build.yml                 # GitHub Actions CI
├── Drivers/
│   ├── CMSIS/
│   ├── STM32F1xx_HAL_Driver/         # HAL 库源码（CubeMX 独立出来）
│   └── BSP/
│       ├── OLED.c/h
│       ├── Key.c/h
│       ├── LED.c/h
│       ├── AD.c/h
│       └── MPU6050.c/h
├── Middlewares/
│   └── FreeRTOS/                     # FreeRTOS 源码（CMSIS_V2）
├── App/
│   ├── Tasks/
│   │   ├── ui_task.c/h
│   │   ├── input_task.c/h
│   │   ├── sensor_task.c/h
│   │   └── game_task.c/h
│   └── main.c
├── Components/
│   ├── GUI/
│   │   ├── menu.c/h                  # 菜单逻辑（状态机）
│   │   ├── SetTime.c/h
│   │   └── gradienter.c/h
│   ├── Game/
│   │   └── dino.c/h
│   └── Utils/
│       ├── Delay.c/h                 # 仅保留调试用阻塞延时
│       └── battery.c/h               # 电池电量计算
├── Docs/
│   ├── hardware_schematic.pdf
│   └── api_reference.md
└── Scripts/
    ├── flash.sh                      # 一键烧录脚本（OpenOCD）
    └── monitor.sh                    # 串口日志
```

#### 3.2 构建系统迁移

| 项目 | 当前 | 目标 |
|---|---|---|
| 编译器 | ARMCC (Keil MDK) | ARM GCC + Keil 双构建 |
| 构建系统 | `.uvprojx` 专属格式 | CMake（跨平台） |
| 烧录工具 | Keil UVision | OpenOCD / ST-Link CLI |
| CI | 无 | GitHub Actions：`make` 编译验证 |
| IDE | 仅 Keil | VS Code (Cortex-Debug) + Keil（可选） |

#### 3.3 状态机重构（替代 `while(1)` 阻塞菜单）

当前每个页面导航都是 `while(1) + Key_GetNum()` 阻塞循环：

```
First_Page_Clock()  → while(1) { Key_GetNum(); ... }
SettingPage()       → while(1) { Key_GetNum(); ... }
Menu_Page()         → while(1) { Key_GetNum(); ... }
```

目标：统一状态机：

```
typedef enum {
    STATE_CLOCK,
    STATE_SETTING,
    STATE_MENU,
    STATE_MPU6050,
    STATE_GAME,
    STATE_GRADIENTER,
    STATE_EMOJI,
} WatchState_t;

WatchState_t current_state, next_state;
// 每个 tick（Input_Task 发出事件后）只执行一次状态转移 + 渲染
```

#### 3.4 中断与 DMA 优化

| 当前 | 优化后 |
|---|---|
| 软件 I2C 位带 + 10µs 阻塞延迟 | 硬件 I2C 中断模式（OLED 刷新 1ms 内完成） |
| ADC 阻塞等待 3000 次采样 | ADC DMA 循环采样，CPU 零等待 |
| OLED 整屏 1024 字节 SPI/I2C 同步发送 | DMA 传输，CPU 继续绘制 |
| 无低功耗 | FreeRTOS tickless + STOP 模式（待机时 µA 级） |

#### 3.5 内存优化

| 当前问题 | 优化方案 |
|---|---|
| OLED 显存全局数组 128×8=1024 字节（5% RAM） | 保持不变（合理），但用 Semaphore 保护 |
| 多处无约束的全局变量 | 封装为模块内部 static + getter/setter |
| 游戏帧缓冲区可能重复申请 | 静态预分配，复用缓冲区 |
| FreeRTOS heap 大小未知 | 先配置 4KB，根据实际使用调整 |

#### 3.6 文档与 CI

| 项目 | 内容 |
|---|---|
| API 文档 | Doxygen 注释生成 |
| 硬件文档 | PDF 原理图 + 引脚映射表 |
| 构建 CI | GitHub Actions：ARM GCC 编译 + 固件大小检查 |
| 版本规范 | Semantic Versioning + CHANGELOG.md |

---

## 常见错误顺序分析

| 错误顺序 | 问题 |
|---|---|
| **FreeRTOS → HAL** | SPL 上没有 `HAL_SYSTICK_Callback`，FreeRTOS tick 要么用 TIM 单独做，要么等 HAL 迁移时重写 OS 配置，多改一次。 |
| **工程优化 → HAL** | 目录结构和 CMakeLists.txt 在 HAL 迁移时引脚/文件名全变，白做。 |
| **三者同时做** | 出问题了不知道是 HAL 的 bug、RTOS 的配置问题还是重构引入的 regression，调试地狱。 |

---

## 建议 Timeline（一个人业余时间）

| 阶段 | 预估时间 | 里程碑产出 |
|---|---|---|
| Phase 1: HAL 迁移 | 2-3 周 | 全部外设在 HAL 上跑通，回归测试通过 |
| Phase 2: FreeRTOS | 1-2 周 | 手表在 RTOS 上运行，多任务正常调度 |
| Phase 3: 工程规范化 | 1-2 周 | CMake + GCC 构建、目录重构、状态机、文档 CI |
| **总计** | **4-7 周** | 三个可工作的里程碑节点 |

> **核心原则：每一步独立可验证，每阶段结束后都是一个可工作的版本。**
> 先换 HAL（地基）→ 再上 FreeRTOS（骨架）→ 最后工程重构（装修）。
> 不出大锅，随时可以暂停。

---

*分析日期：2025年*
*作者：AtomCode（基于项目仓库 https://github.com/CHSGY/My_SmartWatch_STM32_STL 的代码审查）*
