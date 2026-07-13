# FreeRTOS 移植：编译器选择与 NVIC 冲突分析

> 文档日期：2026-06-16
> 分支：`FreeRTOS_Version`
> 目标工程：`SmartWatch_HAL/HAL/MDK-ARM/HAL.uvprojx`

---

## 一、当前工程基线

| 项目 | 值 |
|:---|:---|
| MCU | STM32F103C8 (Cortex-M3, 64KB Flash, 20KB SRAM) |
| 编译器 | **ARMCC V5.06** (build 528) |
| ARMCLANG V6 | 已显式禁用 (`uAC6=0`) |
| C标准 | C99 |
| 中文编码 | **GB2312**（源文件 GBK 编码 + `--no-multibyte-chars`） |
| HAL库 | STM32F1xx HAL Driver |
| CMSIS | CMSIS v5 |
| USE_RTOS | `0`（当前未启用） |
| NVIC 优先级分组 | **NVIC_PRIORITYGROUP_2** |
| 系统心跳 | **TIM2**（1ms周期，PSC=719, ARR=99, 72MHz→1KHz） |
| 延时实现 | **DWT CYCCNT**（已与 SysTick 解耦，FreeRTOS 兼容） |
| FreeRTOS 源码 | 尚未导入（仅 CMSIS RTOS2 头文件模板存在） |

---

## 二、编译器方案对比（最终结论）

### 方案A：ARMCC V5 + CMSIS-RTOS v1（✅ 推荐）

| 维度 | 评估 |
|:---|:---|
| 源文件编码 | GBK 不变，`--no-multibyte-chars` 不变 |
| 中文显示 | 零改动，OLED 字库 GB2312 索引保持不变 |
| 编译器迁移 | 无需迁移，无风险 |
| FreeRTOS API | CMSIS-RTOS v1（功能冻结但稳定） |
| 汇编启动文件 | `startup_stm32f103xb.s`（ARMCC 语法）不变 |
| HAL 兼容性 | ARMCC V5 原生支持，已验证通过 |
| 技术债务 | 编译器老旧，未来可能被 Keil 移除 |
| **总工作量** | **~2-3天**（仅 FreeRTOS 移植本身） |

### 方案B：ARMCLANG V6 + CMSIS-RTOS v2（⛔ 暂不推荐）

| 维度 | 评估 |
|:---|:---|
| 源文件编码 | GBK → **UTF-8**（全部含中文的源文件需转码） |
| 中文显示 | 需改 `OLED_CHARSET_GB2312` → `OLED_CHARSET_UTF8`，字库索引 `char Index[3]` → `char Index[5]`，所有字模条目的 GB2312 码改为 UTF-8 码 |
| 编译器迁移 | `--no-multibyte-chars` 不再需要，ARMCLANG 原生 UTF-8 |
| 汇编文件 | `startup_stm32f103xb.s` 需替换为 ARMCLANG 兼容版本（AREA→.section 等） |
| CMSIS 头文件 | 自动切换 `cmsis_armcc.h` → `cmsis_armclang.h` |
| HAL __weak | 已验证 CubeMX 最新 HAL 版本已修复 `__weak` 兼容性 |
| 风险 | 编码转换可能引入隐形乱码，调试困难 |
| **总工作量** | **~5-7天**（编码迁移 + FreeRTOS 移植） |

### 方案C：ARMCLANG V6 + 保留 GBK（⛔ 不推荐）

ARMCLANG 的 `-finput-charset=GBK -fexec-charset=GBK` 理论上可保留 GBK 字符串，但：
- ARMCLANG 对 `-fexec-charset` 支持不完整
- 与 CMSIS/ST HAL 库的 UTF-8 默认假设冲突
- 风险极高，不推荐

### 结论：选择方案A

**先用 ARMCC V5 + CMSIS-RTOS v1 把 FreeRTOS 跑起来。** 编码问题留待后续统一处理（可编写脚本批量转换）。

---

## 三、CMSIS-RTOS v1 + TIM2 中断冲突分析

### 3.1 核心问题：NVIC 优先级分组不兼容

这是 FreeRTOS 移植中最关键的隐藏问题。

```
当前状态：NVIC_PRIORITYGROUP_2  (2-bit preempt, 2-bit sub)
FreeRTOS要求：NVIC_PRIORITYGROUP_4  (4-bit preempt, 0-bit sub)
```

#### 为什么 GROUP_2 有问题？

Cortex-M3 实现了 **4 位优先级**（16 个级别，值域 0-15）。FreeRTOS 使用 `BASEPRI` 寄存器来屏蔽中断——进入临界区时，`BASEPRI` 被设置为 `configMAX_SYSCALL_INTERRUPT_PRIORITY`，任何优先级数值**大于等于**此值的中断被屏蔽。

标准 FreeRTOS Cortex-M3 配置：
```c
#define configPRIO_BITS         4
#define configKERNEL_INTERRUPT_PRIORITY    (15 << (8 - configPRIO_BITS))  // = 0xF0 = 240
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  (5 << (8 - configPRIO_BITS)) // = 0x50 = 80
```

在 GROUP_4 下：4 位全是抢占优先级，`BASEPRI=80` 表示屏蔽优先级 5-15 的中断，只允许 0-4 的中断打断 FreeRTOS 临界区。**语义正确。**

在 GROUP_2 下：只有高 2 位是抢占优先级（值域 0-3），低 2 位是子优先级。`BASEPRI=80`（0x50）的二进制是 `0101 0000`，高 2 位 = `01` = 抢占优先级 1。**这会屏蔽抢占优先级 1-3 的中断，只允许 0。** 与 GROUP_4 下的语义完全不同。

#### 当前 TIM2 优先级在 GROUP_2 下的问题

当前代码：
```c
// stm32f1xx_hal_msp.c:72
HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_2);

// stm32f1xx_hal_msp.c:210
HAL_NVIC_SetPriority(TIM2_IRQn, 2, 1);   // preempt=2, sub=1
```

在 GROUP_2 下，`TIM2_IRQn` 的原始优先级 = `(2 << 6) | (1 << 4)` = 0x90 = 144。

FreeRTOS 默认 `configMAX_SYSCALL_INTERRUPT_PRIORITY = 80`（0x50）。

在 GROUP_2 下，`BASEPRI=80`（0x50），高 2 位=1，所以优先级 1-3 的中断被屏蔽。TIM2 的抢占优先级=2，**会被 BASEPRI 屏蔽**。这意味着 TIM2 ISR 内不能调用 FreeRTOS API（如 `xQueueSendFromISR`），这是正确的——但如果你**想在 TIM2 ISR 内调用 FreeRTOS API**，就会静默失败或触发 assert。

### 3.2 解决方案：切换到 NVIC_PRIORITYGROUP_4

必须修改 `stm32f1xx_hal_msp.c` 第 72 行：

```c
// 修改前
HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_2);

// 修改后
HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
```

然后**所有** `HAL_NVIC_SetPriority()` 调用必须重新计算参数。当前只有一个：

```c
// 修改前（GROUP_2, preempt=2, sub=1）
HAL_NVIC_SetPriority(TIM2_IRQn, 2, 1);

// 修改后（GROUP_4, 只有 preempt 参数有效, sub 参数被忽略）
// 保持相同的行为：TIM2 优先级应低于 RTOS 临界区阈值(5)
// 即 TIM2 优先级 > 5，使 TIM2 在 BASEPRI 屏蔽范围内
HAL_NVIC_SetPriority(TIM2_IRQn, 6, 0);   // preempt=6, > configMAX_SYSCALL(5), 被 RTOS 临界区屏蔽
```

优先级分配建议（GROUP_4 下）：

| 中断 | 优先级 | 说明 |
|:---|:---|:---|
| SysTick（RTOS tick） | **15（最低）** | FreeRTOS 强制要求 |
| PendSV | **15（最低）** | FreeRTOS 强制要求 |
| SVC | **0（最高）** | FreeRTOS 强制要求 |
| TIM2（应用定时器） | **6** | 高于 RTOS 临界区阈值 5，被屏蔽 |
| 需要打断 RTOS 的紧急中断 | **0-4** | 可调用 FreeRTOS ISR API |

### 3.3 CubeMX 配置 CMSIS-RTOS v1 时 TIM2 会冲突吗？

**不会直接冲突，但需要正确配置。** 具体分析：

#### CubeMX 的行为

1. **RTOS tick 源**：CubeMX CMSIS-RTOS v1 默认使用 **SysTick** 作为 FreeRTOS 的 tick。它**不会**自动占用 TIM2。
2. **TIM2 保持独立**：TIM2 继续作为你的应用层 1ms 定时器（按键扫描、秒表计时、Dino 游戏 tick）。
3. **HAL_IncTick 处理**：CubeMX 启用 FreeRTOS 后，会将 `HAL_IncTick()` 从 `SysTick_Handler` 移到 FreeRTOS 的 `SysTick_Handler` 钩子中，或者使用 TIM 外设提供 HAL tick。

#### 需要注意的点

| 检查项 | 状态 | 操作 |
|:---|:---|:---|
| `SVC_Handler` | 当前为空 stub ✅ | FreeRTOS 会接管，CubeMX 会自动生成 |
| `PendSV_Handler` | 当前为空 stub ✅ | FreeRTOS 会接管 |
| `SysTick_Handler` | 当前调用 `HAL_IncTick()` ⚠️ | FreeRTOS 接管后需确认 HAL tick 来源 |
| `TIM2_IRQHandler` | 当前用于应用层 ✅ | 保持现有实现不变 |
| NVIC Priority Group | GROUP_2 ❌ | **必须改为 GROUP_4** |
| DWT 延时 | 已与 SysTick 解耦 ✅ | 无需修改 |

### 3.4 HAL Tick 迁移：TIM2 替代 SysTick

启用 FreeRTOS 后，`SysTick_Handler` 被 FreeRTOS 接管。`HAL_IncTick()` 不能继续放在 SysTick 中断中。你有两个选择：

**选择1：TIM2 双重职责（推荐）**

TIM2 已经是 1ms 周期，在 `TIM2_IRQHandler` 中添加 `HAL_IncTick()`：

```c
void TIM2_IRQHandler(void)
{
    HAL_IncTick();         // ← 新增：替代 SysTick 为 HAL 提供时基
    Key3_Tick();
    KeyTick();
    StopClock_Tick();
    dino_tick();
    HAL_TIM_IRQHandler(&htim2);
}
```

同时在 `stm32f1xx_hal_conf.h` 中无需修改（HAL tick 默认使用 SysTick，但 `HAL_IncTick` 可在任何 ISR 中调用）。需在 `main.h` 或 FreeRTOSConfig.h 中重新定义 `HAL_GetTick()` 如果使用了 `HAL_GetTick()`。

**选择2：保持 SysTick 双用途（不推荐）**

让 SysTick 同时服务于 FreeRTOS 和 HAL，需要确保 FreeRTOS tick 速率与 HAL tick 速率一致。会增加耦合。

### 3.5 最终 NVIC 配置汇总（迁移后）

```c
// stm32f1xx_hal_msp.c → HAL_MspInit()
HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);  // 必须改为 GROUP_4

// HAL_TIM_Base_MspInit() → TIM2
HAL_NVIC_SetPriority(TIM2_IRQn, 6, 0);  // 高于 RTOS 临界区阈值，被 BASEPRI 屏蔽

// FreeRTOSConfig.h（CubeMX 自动生成，需确认）
#define configPRIO_BITS                      4
#define configKERNEL_INTERRUPT_PRIORITY      (15 << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5  << (8 - configPRIO_BITS))
```

---

## 四、移植步骤概要（方案A）

| 阶段 | 内容 | 预计时间 |
|:---|:---|:---|
| 1 | CubeMX 启用 CMSIS-RTOS v1，生成 FreeRTOS 代码 | 30分钟 |
| 2 | 修改 NVIC Priority Group 为 4，调整 TIM2 优先级 | 15分钟 |
| 3 | HAL tick 迁移（TIM2_IRQHandler 中添加 HAL_IncTick） | 15分钟 |
| 4 | 将 9 个页面函数改造为 FreeRTOS 任务 | 1天 |
| 5 | FreeRTOSConfig.h 调优（栈大小、tick 频率、SRAM 预算） | 1小时 |
| 6 | 编译调试，解决警告和错误 | 半天 |
| 7 | 实际运行测试（功耗、稳定性、中文显示验证） | 半天 |

---

## 五、关键风险清单

| 风险 | 等级 | 缓解措施 |
|:---|:---|:---|
| SRAM 不足（20KB 总量，FreeRTOS 需 8-15KB） | 🔴 高 | 精简任务栈，使用 `uxTaskGetStackHighWaterMark` 监控 |
| NVIC GROUP_2→4 迁移遗漏 | 🟡 中 | 逐项检查所有 `HAL_NVIC_SetPriority` 调用 |
| HAL 超时函数依赖 SysTick | 🟡 中 | TIM2 提供 HAL tick 后需验证 `HAL_GetTick` 准确性 |
| TIM2 ISR 中调用 FreeRTOS API 被 BASEPRI 拦截 | 🟢 低 | TIM2 优先级 6 > configMAX(5)，不会调用 FreeRTOS API |
| 中文 GB2312 显示不受影响 | 🟢 低 | 编译器不变，编码不变 |

---

## 六、结论

1. **编译器**：坚持 ARMCC V5，不切换到 ARMCLANG V6。中文 GB2312 编码保持不变，零迁移风险。
2. **CMSIS-RTOS 版本**：使用 CMSIS-RTOS v1（与 ARMCC V5 最佳兼容）。
3. **TIM2 与 FreeRTOS 不冲突**：TIM2 作为应用层定时器保持独立，FreeRTOS 使用 SysTick。但**必须**将 NVIC Priority Group 从 2 改为 4，并重新计算所有中断优先级。
4. **HAL Tick**：建议在 TIM2_IRQHandler 中添加 `HAL_IncTick()` 以替代 SysTick 的 HAL 时基功能。
5. **DWT 延时**：当前实现已完美兼容 FreeRTOS，无需修改。
