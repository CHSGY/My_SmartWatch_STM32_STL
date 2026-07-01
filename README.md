# STM32 Watch — 基于 FreeRTOS 的实时嵌入式智能手表

**在 20KB SRAM 的 ARM Cortex-M3 上运行 FreeRTOS 多任务系统，集 OLED 图形界面、6 轴传感器融合、体感游戏和智能电源管理于一体的全栈嵌入式项目。**

---

## 技术指标

| 指标 | 值 | 指标 | 值 |
|------|-----|------|-----|
| MCU | STM32F103C8T6 (Cortex-M3) | RTOS | FreeRTOS V10.3.1 |
| 主频 | 72MHz (HSE 8MHz × PLL9) | 用户任务 | 3 个 (Input / UI / Sensor) |
| Flash | 64KB | 堆配置 | 10KB (heap_4) |
| SRAM | 20KB | 帧率 | 30FPS (OLED) |
| 编译器 | ARMCC V5.06 | 传感器 | MPU6050 六轴 IMU |
| IDE | Keil MDK V5 + STM32CubeMX | 显示 | SSD1306 OLED 128×64 |

---

## 代码规模

| 层次 | 模块 | 代码量 |
|------|------|--------|
| **应用入口** | main.c / freertos.c / stm32f1xx_it.c | ~970 行 |
| **硬件驱动** | OLED (1510) / OLED_Data (1055) / menu (1078) / dino (305) / Key (232) / MPU6050 (153) / LED (105) / SetTime (65) / AD (25) | ~4,530 行 |
| **系统层** | MyRTC (73) / delay (52) / power (39) / MyI2C | ~200 行 |
| **头文件** | 所有模块 .h 文件 | ~1,490 行 |
| **用户代码总计** | 14 个 .c + 14 个 .h | **~7,200 行** |

> 完整工程（含 HAL 库 + FreeRTOS 内核源码）约 60 万行，其中自主编写的应用层代码约 7,200 行，体现了在成熟生态基础上进行高效应用开发的能力。

---

## 项目演进路径

本项目经历了完整的嵌入式软件演进路径：

```
Standard Peripheral Library (裸机)          HAL 库迁移              FreeRTOS 集成
        STD/                            SmartWatch_HAL/          SmartWatch_HAL/
   ┌─────────────┐                  ┌─────────────────┐      ┌─────────────────┐
   │ superloop   │  ────迁移────▶   │ CubeMX HAL 框架 │ ───▶ │ FreeRTOS V10.3.1│
   │ 单线程轮询  │                  │ STM32F1xx HAL   │      │ 3 任务抢占式调度 │
   │ 标准外设库  │                  │ 15 文件全量移植  │      │ 任务通知 + 互斥  │
   └─────────────┘                  └─────────────────┘      └─────────────────┘
```

整个迁移过程采用分阶段实施策略（5 个 Phase），系统化地解决了 **11 个移植与并发问题**，形成了一套在资源极度受限硬件（20KB SRAM）上运行 RTOS 的完整方法论。详细的移植记录和问题追踪见 [doc/](doc/) 目录。

> 当前活跃分支 `FreeRTOS_Version` 包含两个工程目录：`STD/`（标准外设库裸机版，保留作为基线参考）和 `SmartWatch_HAL/`（HAL + FreeRTOS 版本，当前主力开发）。

---

## 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                      应用层 (Application)                        │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────────┐ │
│  │ 时钟首页 │  │ 菜单系统 │  │ 恐龙游戏 │  │ 秒表/水平仪/... │ │
│  │        12 个功能页面统一由 Task_UI 状态机调度                 │ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                    FreeRTOS 内核 (V10.3.1)                       │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐ │
│  │  Task_Input  │  │   Task_UI   │  │      Task_Sensor        │ │
│  │  优先级: 3   │  │  优先级: 2  │  │      优先级: 1          │ │
│  │  栈: 384B    │──│  栈: 1280B  │  │      栈: 512B           │ │
│  │  按键处理    │  │  UI 渲染    │  │      MPU6050 采样       │ │
│  │              │  │  独占OLED   │  │      独占MPU I2C        │ │
│  └──┬───────────┘  └──┬──────────┘  └────────┬────────────────┘ │
│     │Task Notify      │Task Notify            │Task Notify       │
│     │(按键值)         │(传感器启停)           │                  │
│     │                 │ 帧率: 30FPS          │ 周期: 5ms        │
│     │      Idle Hook: __WFI() 统一低功耗入口                    │
├─────────────────────────────────────────────────────────────────┤
│                    HAL 硬件抽象层                                │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────────────┐│
│  │ GPIO │ │ TIM2 │ │ ADC1 │ │ RTC  │ │ I2C  │ │  NVIC (PRIO4) ││
│  │按键LED│ │1msTick│ │电池  │ │时钟  │ │软件模拟│ │  中断管理    ││
│  └──────┘ └──┬───┘ └──────┘ └──────┘ └──────┘ └──────────────┘│
│              │ 中断 → Task_Input 通知                           │
├─────────────────────────────────────────────────────────────────┤
│                    硬件层 (Hardware)                             │
│  STM32F103C8T6 | SSD1306 OLED 128×64 | MPU6050 | 3×Button      │
└─────────────────────────────────────────────────────────────────┘
```

### 数据流与通信机制

| 通信路径 | 机制 | 说明 |
|----------|------|------|
| TIM2 ISR → Task_Input | `vTaskNotifyGiveFromISR()` | 零拷贝唤醒，按键事件通知 |
| Task_Input → Task_UI | `xTaskNotify()` | 按键值转发（覆盖写语义） |
| Task_UI → Task_Sensor | `xTaskNotify()` | 传感器启停命令（进入/离开传感器页） |
| OLED I2C 互斥 | `vTaskSuspendAll()` | I2C 传输期间禁止抢占，防止时序错乱 |
| 帧率控制 | `xTaskNotifyWait(33ms)` | 事件驱动唤醒 + 30FPS 超时驱动 |

---

## FreeRTOS 任务设计

### 任务划分策略

面对从裸机超级循环向 RTOS 迁移的核心问题：**如何将一个单线程事件循环拆分为多个协作任务？**

**划分原则：**

1. **时序独立性**：按键扫描（事件驱动）≠ UI 渲染（帧驱动）≠ 传感器采样（周期驱动）→ 天然 3 任务
2. **资源独占性**：OLED（PB10/PB11）和 MPU6050（PB10/PB11）各有一条独立软件 I2C 总线，任务独占各自总线 → 无需互斥锁
3. **SRAM 约束**：20KB SRAM 下每个任务栈必须精确计算 → 通过 `uxTaskGetStackHighWaterMark()` 实测优化

### 任务规格

| 任务 | 优先级 | 栈大小 | 周期/触发 | 通信方式 | 职责 |
|------|--------|--------|-----------|----------|------|
| **Task_Input** | 3 (最高) | 384B | 事件驱动 | TIM2 ISR → Task Notify | 按键消抖、长按关机、按键转发 |
| **Task_UI** | 2 | 1280B | 30FPS (33ms) | Task Notify + 超时 | 12 页面状态机、OLED 独占渲染 |
| **Task_Sensor** | 1 (最低) | 512B | 5ms 周期 | Task_UI 通知启停 | MPU6050 互补滤波、欧拉角计算 |
| Idle | 0 | 128B | — | 静态分配 | `__WFI()` 统一低功耗入口 |
| Timer | 2 | 256B | — | 静态分配 | FreeRTOS 软件定时器服务 |

### 优先级分析

```
优先级 3 (Task_Input) : 按键响应零延迟，长按关机在任何页面立即生效
优先级 2 (Task_UI)    : 30FPS 满足 OLED 视觉需求，可被按键抢占，不会饿死
优先级 1 (Task_Sensor) : 传感器数据仅在使用者查看时才有意义，后台填充全局变量
优先级 0 (Idle)       : 所有任务阻塞时自动进入 WFI 休眠
```

### 为什么用 Task Notification 而非队列？

| 维度 | Task Notification | Queue |
|------|-------------------|-------|
| 语义匹配 | ✅ 单值覆盖（新按键覆盖旧按键） | ❌ FIFO 缓冲（积压过期按键） |
| 速度 | ✅ 比队列快 ~50% | — |
| RAM 开销 | ✅ 0 额外字节（嵌入 TCB） | ❌ 每个队列 ≥ 76 字节 |
| 适用场景 | 单消费者、单值覆盖 | 多消费者、需要缓冲 |

> 在 20KB SRAM 约束下，3 个队列 ≈ 228 字节开销不可忽视。Task Notification 将通信结构体嵌入任务 TCB，零额外 RAM。

---

## 功能模块总览

| 页面 | 功能 | 技术要点 |
|------|------|----------|
| 🕐 **时钟首页** | 实时时间 + 日期 + 电池 | HAL_RTC 日历、ADC 过采样（16 次均值）、电池百分比映射 |
| 📋 **主菜单** | 7 项图标式菜单 + 滑动动画 | 帧动画（8px/帧滑入）、光标反显、图标索引 |
| ⚙️ **设置** | 系统设置入口 | 预留扩展接口 |
| ⏱️ **秒表** | 启/停/清零计时器 | TIM2 1ms tick 驱动、按钮反显选中态、居中布局 |
| 🔦 **手电筒** | LED 全亮照明 | GPIO 推挽输出、PWM 预留 |
| 📊 **传感器数据** | 加速度 + 陀螺仪 + 欧拉角 | 互补滤波（α=0.9）、5ms 采样、100ms 快速收敛期 |
| 🫧 **水平仪** | 气泡尺姿态显示 | 欧拉角 → 圆心偏移映射、圆形边界约束 |
| 🦖 **恐龙游戏** | 跑酷跳跃游戏 | 物理跳跃抛物线、障碍物随机生成、碰撞检测、分数累加 |
| 😊 **表情动画** | 眨眼表情循环 | 椭圆绘制（Bresenham 算法）、帧动画状态机 |
| 🕑 **时间设置** | RTC 日历设置 | 子状态机（年/月/日/时/分/秒）、BCD 格式转换 |
| 🔍 **Debug 页面** | 任务栈水位监控 | `uxTaskGetStackHighWaterMark()` 5 任务栈实时读取 |

---

## 关键技术决策

### 1. 编译器选型：ARMCC V5 vs ARMCLANG V6

| 维度 | ARMCC V5 (✅ 选用) | ARMCLANG V6 (暂缓) |
|------|-------------------|---------------------|
| 中文编码 | GB2312 不变，零改动 | 全量转 UTF-8，风险高 |
| 移植工期 | 2-3 天 | 5-7 天 |
| 编译结果 | 0 Error, 0 Warning | 待验证 |
| 技术债务 | 编译器已停止维护 | 未来迁移方向 |

**决策**：先用 ARMCC V5 + CMSIS-RTOS v1 把 FreeRTOS 跑起来，编码迁移留待后续统一处理。完整分析见 [FreeRTOS_Compiler_Migration_Analysis.md](doc/FreeRTOS_Compiler_Migration_Analysis.md)。

### 2. 静态内存分配：应对 20KB SRAM 极限

Idle Task 和 Timer Task 使用静态分配（`StaticTask_t` + `StackType_t` 数组），用户任务使用 heap_4 动态分配。通过 Debug 页面持续监控栈水位，确保所有任务栈余量 > 30%。

### 3. OLED I2C 时序保护

软件 I2C（bit-banging）对时序极度敏感。高优先级任务抢占会导致 SCL 脉冲被拉长，从设备误判为 STOP 条件。**方案**：`OLED_Update()` 中调用 `vTaskSuspendAll()` 挂起调度器，I2C 传输完成后 `xTaskResumeAll()` 恢复。彻底消除 I2C 花屏和 MPU6050 数据异常。

### 4. DWT 微秒延时：与 FreeRTOS SysTick 解耦

裸机版使用 SysTick 做延时，FreeRTOS 需要 SysTick 作为系统 tick 源。**方案**：迁移到 DWT CYCCNT（Cortex-M3 调试单元周期计数器），硬件级精确延时，不占用 SysTick，可在 ISR 中调用，用完即关零额外功耗。

### 5. 分阶段移植策略

| Phase | 内容 | 成果 |
|-------|------|------|
| Phase 1 | CubeMX 生成 FreeRTOS 骨架 | 编译通过，调度器正常启动 |
| Phase 2 | 创建 Task_Input | ISR → Task 通知通信验证 |
| Phase 3 | 9 页面重构为 Task_UI 状态机 | 统一渲染架构，消除独占 while(1) |
| Phase 4 | 创建 Task_Sensor | MPU6050 后台采样 + Sleep/Wake 功耗管理 |
| Phase 5 | Debug 页面 + 栈监控 | 5 任务栈水位可视化，系统稳定性验证 |

每个阶段独立可测试，11 个移植问题逐一记录和修复，形成可复用的方法论文档。

---

## 功耗管理

在电池供电的可穿戴场景下，功耗是关键指标。本项目采用多层功耗优化：

| 层级 | 策略 | 实现 |
|------|------|------|
| **系统级** | Idle Hook 统一休眠 | `vApplicationIdleHook()` 中调用 `__WFI()`，所有任务阻塞时 MCU 进入睡眠 |
| **外设级** | MPU6050 按需启停 | 进入传感器页 → `MPU6050_Wake()`；离开 → `MPU6050_Sleep()` |
| **刷新率** | 30FPS 帧率限制 | Task_UI 33ms 超时唤醒，比无限制刷新降低约 3 倍 OLED 刷新功耗 |
| **ADC 采样** | 间歇采样 | 电池 ADC 每 50 帧（~1.65s）采样一次，其余帧使用缓存值 |
| **硬件关机** | PMOS 电源控制 | 长按 Key3 → `POWER_Shutdown()` 通过 PB13 关闭 MCU 电源 PMOS |

> 对比裸机版：原 9 个页面各自调用 `__WFI()`，FreeRTOS 版统一在 Idle Hook 中处理，代码更简洁且休眠覆盖更完整。

---

## 硬件配置与引脚分配

| 外设 | 引脚 | 说明 |
|------|------|------|
| OLED SCL | PB10 | 软件 I2C 时钟线（开漏输出） |
| OLED SDA | PB11 | 软件 I2C 数据线（开漏输出） |
| MPU6050 SCL | PB10 | 独立软件 I2C 时钟线 |
| MPU6050 SDA | PB11 | 独立软件 I2C 数据线 |
| Key1 (上) | PB1 | 上拉输入，20ms 消抖 |
| Key2 (下) | PA6 | 上拉输入，20ms 消抖 |
| Key3 (确认) | PA4 | 上拉输入，长按 ≥1s = 关机 |
| LED1 | PA0 | 推挽输出，手电筒 |
| LED2 | PB5 | 推挽输出，状态指示 |
| LED3 | PB6 | 推挽输出，状态指示 |
| ADC 电池 | PA2 | ADC1 CH2，12-bit 采样 |
| POWER_CTRL | PB13 | MCU 电源 PMOS 控制 |
| ADC_CTRL | PB12 | ADC/高压 PMOS 控制 |

---

## 编译与调试

### 开发环境

| 工具 | 版本/说明 |
|------|----------|
| IDE | Keil MDK V5 (uVision) |
| 编译器 | ARMCC V5.06 build 528 |
| 代码生成 | STM32CubeMX 6.x |
| 调试器 | ST-Link V2 (SWD) |
| 版本控制 | Git (`FreeRTOS_Version` 分支) |

### 编译步骤

```bash
1. STM32CubeMX 打开 SmartWatch_HAL/HAL/HAL.ioc → Generate Code
2. Keil MDK 打开 SmartWatch_HAL/HAL/MDK-ARM/HAL.uvprojx
3. 确认 Target: STM32F103C8, Compiler: ARMCC V5
4. F7 编译 → 0 Error, 0 Warning
5. F8 下载 → ST-Link 烧录
```

### 运行时调试

- **Debug 页面**：实时显示 Task_Input / Task_UI / Task_Sensor / Idle / Timer 五个任务的栈剩余水位（word 为单位）
- **栈溢出检测**：`configCHECK_FOR_STACK_OVERFLOW = 2`，溢出时触发 `vApplicationStackOverflowHook()`
- **断言保护**：`configASSERT()` 在参数非法时关中断死循环，方便 JTAG 定位

---

## 项目目录结构

```
SmartWatch_HAL/HAL/
├── Core/
│   ├── Inc/                            # 头文件
│   │   ├── main.h                      # HAL 外设句柄声明
│   │   ├── FreeRTOSConfig.h            # 内核配置（优先级/堆/钩子/静态分配）
│   │   ├── stm32f1xx_hal_conf.h        # HAL 模块裁剪
│   │   ├── stm32f1xx_it.h              # 中断服务函数声明
│   │   ├── delay.h / MyRTC.h / MyI2C.h / power.h
│   │   └── Hardware/                   # 硬件驱动头文件
│   │       ├── OLED.h / OLED_Data.h    # OLED 驱动 + GB2312 字库
│   │       ├── MPU6050.h / MPU6050_Reg.h  # 6 轴传感器 + 寄存器定义
│   │       ├── menu.h                  # 页面枚举 + 全局接口声明
│   │       ├── dino.h / Key.h / LED.h / AD.h / SetTime.h
│   └── Src/                            # 源文件
│       ├── main.c                      # HAL 初始化 + 任务创建 + 启动调度器
│       ├── freertos.c                  # 3 个任务实现 + Idle/Timer Hook
│       ├── stm32f1xx_it.c              # TIM2 ISR（按键通知 + 游戏 tick）
│       ├── stm32f1xx_hal_msp.c         # HAL 外设 MSP 初始化
│       ├── system_stm32f1xx.c          # 系统时钟配置
│       ├── delay.c / MyRTC.c / MyI2C.c / power.c
│       └── Hardware/                   # 硬件驱动源文件
│           ├── OLED.c / OLED_Data.c    # 显示驱动 (1510 + 1055 行)
│           ├── menu.c                  # 12 页面状态机 + UI 渲染 (1078 行)
│           ├── dino.c                  # 恐龙游戏物理引擎 (305 行)
│           ├── Key.c                   # 按键消抖 + 长按检测 (232 行)
│           ├── MPU6050.c               # 传感器初始化 + 欧拉角 (153 行)
│           ├── LED.c / AD.c / SetTime.c
├── Drivers/                            # STM32 HAL 库 + CMSIS
├── Middlewares/Third_Party/FreeRTOS/    # FreeRTOS V10.3.1 内核源码
├── HAL.ioc                             # CubeMX 工程文件
└── MDK-ARM/HAL.uvprojx                 # Keil 工程文件

STD/                                    # 基线参考：标准外设库裸机版
├── Hardware/                           # 硬件驱动（OLED/Key/MPU6050/...）
├── System/                             # 系统模块（Timer/RTC/Delay）
├── Library/                            # STM32 标准外设库
├── Start/                              # 启动文件
├── User/                               # 用户入口（main.c）
└── Project.uvprojx                     # Keil 工程文件
```

---

## 面试能力举证

本项目展示了以下嵌入式软件工程师核心能力：

| 能力维度 | 项目体现 |
|----------|----------|
| **RTOS 系统设计** | 从裸机 superloop 到 FreeRTOS 3 任务架构的完整迁移，包含任务划分、优先级分析、栈大小计算、IPC 选型 |
| **硬件驱动开发** | 软件 I2C（bit-banging）、SSD1306 OLED 驱动、MPU6050 六轴传感器驱动、ADC 电池监测 |
| **传感器算法** | 互补滤波（加速度 + 陀螺仪融合）、欧拉角姿态解算、物理碰撞检测 |
| **资源约束优化** | 20KB SRAM 下的静态/动态分配权衡、栈水位监控、Task Notification 替代队列节省 RAM |
| **并发与同步** | 临界区保护、vTaskSuspendAll 保护 I2C 时序、ISR → Task 通知机制、竞态条件修复 |
| **功耗管理** | 多层级低功耗策略（WFI 休眠、MPU6050 Sleep/Wake、30FPS 帧率控制、间歇 ADC 采样、PMOS 硬件关机） |
| **工程规范** | CubeMX 代码生成、分阶段移植、11 个问题的系统化追踪与修复、4 篇方法论文档 |
| **编译器与工具链** | ARMCC V5 vs ARMCLANG V6 选型分析、NVIC 优先级分组、DWT 与 SysTick 解耦 |

---

## 参考文档

| 文档 | 内容 |
|------|------|
| [FreeRTOS_Transplant.md](doc/FreeRTOS_Transplant.md) | FreeRTOS 移植全记录（5 Phase + 11 问题修复 + 技能总结） |
| [FreeRTOS_Compiler_Migration_Analysis.md](doc/FreeRTOS_Compiler_Migration_Analysis.md) | 编译器选型分析（ARMCC V5 vs ARMCLANG V6 + NVIC 冲突） |
| [MIGRATION_REPORT.md](doc/MIGRATION_REPORT.md) | STD → HAL 迁移详细报告 |
| [HAL_Porting_Issues.md](doc/HAL_Porting_Issues.md) | HAL 移植问题追踪 |

---

## 许可证

MIT License
