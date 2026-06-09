# STM32 标准库 → HAL库 迁移分析报告

> **最后更新**: 2026-06-09 12:00 — 添加STD↔HAL函数接口对照表

## 📊 迁移进度概览

| 模块 | STD文件 | HAL状态 | 进度 |
|------|---------|---------|------|
| **硬件驱动层** | 11个文件 | 11个已迁移 | 100% |
| **系统层** | 3个文件 | 3个已迁移 | 100% |
| **应用层** | 1个文件 | 1个已迁移 | 100% |
| **总计** | 15个文件 | 15个已迁移 | **100%** |

---

## ✅ 已完成迁移

### 硬件驱动层 (Core/Src/Hardware/)

| 文件 | 说明 | 迁移状态 |
|------|------|----------|
| `AD.c/h` | ADC电池电压监测 | ✅ 完成 |
| `Key.c/h` | 三按键输入处理 | ✅ 完成 |
| `LED.c/h` | LED状态指示灯 | ✅ 完成 |
| `MPU6050.c/h` | 6轴传感器驱动 | ✅ 完成 |
| `OLED.c/h` | SSD1306 OLED显示驱动 | ✅ 完成 |
| `OLED_Data.c/h` | OLED字库数据 | ✅ 完成 |
| **`menu.c/h`** | **主菜单及功能模块** | **✅ 完成** |
| **`dino.c/h`** | **恐龙跳跃游戏** | **✅ 新迁移** |

### 系统层 (Core/Src/)

| 文件 | 说明 | 迁移状态 |
|------|------|----------|
| `delay.c/h` | 微秒/毫秒延时 (DWT实现) | ✅ 完成 |
| `MyRTC.c/h` | RTC实时时钟 | ✅ 完成 |
| `power.c/h` | 电源管理 | ✅ 完成 |

### 核心文件

| 文件 | 说明 | 迁移状态 |
|------|------|----------|
| `main.c` | 主程序入口 | ✅ 完成 |
| `stm32f1xx_it.c` | 中断处理 | ✅ 完成 |
| `system_stm32f1xx.c` | 系统初始化 | ✅ 完成 |

---

## ❌ 待迁移模块

### 1. 菜单系统 (`menu.c/h`) ✅ 已完成迁移

**原文件位置**: `STD/Hardware/menu.c` (1030行)

**迁移状态**: ✅ 已完成

**迁移内容**:
- 头文件包含：`stm32f10x.h` → `main.h`, `Delay.h` → `delay.h`
- 延时函数：`Delay_ms()` → `delay_ms()`
- 电源控制：GPIO直接操作 → `POWER_Shutdown()` 模块
- ADC初始化：移除 `AD_Init()`（CubeMX已完成）
- TIM2中断：添加 `StopClock_Tick()` 和 `dino_tick()` 回调
- 创建 stub 文件确保编译通过

---

### 2. 恐龙游戏 (`dino.c/h`) ✅ 已完成迁移

**原文件位置**: `STD/Hardware/dino.c` (273行)

**迁移状态**: ✅ 已完成

**迁移内容**:
- 头文件包含：`stm32f10x.h` → `main.h`, `Delay.h` → `delay.h`
- 延时函数：`Delay_s()` → `delay_ms(1000)`
- 创建 `dino.h` (99行) 和 `dino.c` (285行) 完整实现
- 删除 `dino_stub.c`
- 更新Keil项目文件：`dino_stub.c` → `dino.c`
- 添加 `--no-multibyte-chars` 编译选项（解决UTF-8中文编码问题）
- 修复 `menu.c` 编码问题（UTF-16 LE → UTF-8）

**依赖关系**: OLED, Key, Delay, Menu

---

### 3. 时间设置 (`SetTime.c/h`) ✅ 已完成迁移

**原文件位置**: `STD/Hardware/SetTime.c` (407行)

**迁移状态**: ✅ 已完成

**迁移内容**:
- 头文件包含：`stm32f10x.h` → `main.h`, `OLED.h`/`Key.h`/`menu.h` → `Hardware/`前缀
- API函数保持完全不变（OLED/Key/MyRTC接口与STD版本一致）
- 创建 `SetTime.h` (75行) 和 `SetTime.c` (418行) 完整实现
- 删除 `SetTime_stub.c`
- 更新Keil项目文件：`SetTime_stub.c` → `SetTime.c`
- 更新 `menu.h` 注释（移除stub标记）

---

### 4. 定时器系统 (`Timer.c/h`) ✅ 已完成

**原文件位置**: `STD/System/Timer.c`

**迁移状态**: ✅ 功能已覆盖

- HAL版本已在 `main.c` 中通过CubeMX生成 (`MX_TIM2_Init`)
- 中断处理已在 `stm32f1xx_it.c` 中实现
- 添加了 `StopClock_Tick()` 和 `dino_tick()` 回调

---

## 📁 文件对照表

| STD文件 | HAL对应 | 状态 |
|---------|---------|------|
| `Hardware/AD.c/h` | `Core/Src/Hardware/AD.c/h` | ✅ |
| `Hardware/Key.c/h` | `Core/Src/Hardware/Key.c/h` | ✅ |
| `Hardware/LED.c/h` | `Core/Src/Hardware/LED.c/h` | ✅ |
| `Hardware/MPU6050.c/h` | `Core/Src/Hardware/MPU6050.c/h` | ✅ |
| `Hardware/OLED.c/h` | `Core/Src/Hardware/OLED.c/h` | ✅ |
| `Hardware/OLED_Data.c/h` | `Core/Src/Hardware/OLED_Data.c/h` | ✅ |
| **`Hardware/menu.c/h`** | **`Core/Src/Hardware/menu.c/h`** | **✅ 新迁移** |
| `Hardware/dino.c/h` | `Core/Src/Hardware/dino.c/h` | ✅ 完成 |
| `Hardware/SetTime.c/h` | `Core/Src/Hardware/SetTime.c` / `Core/Inc/Hardware/SetTime.h` | ✅ 完成 |
| `Hardware/MyI2C.c/h` | `Core/Src/MyI2C.c/h` | ✅ |
| `System/Delay.c/h` | `Core/Src/delay.c/h` | ✅ |
| `System/MyRTC.c/h` | `Core/Src/MyRTC.c/h` | ✅ |
| `System/Timer.c/h` | `Core/Src/main.c` | ✅ |
| `User/main.c` | `Core/Src/main.c` | ✅ |

---

## 🔄 STD库与HAL库函数接口对照表

### 1. GPIO — 通用输入输出

| 功能 | STD 标准库 | HAL 库 |
|------|-----------|--------|
| 初始化GPIO引脚 | `GPIO_Init(GPIOx, &GPIO_InitStructure)` | `HAL_GPIO_Init(GPIOx, &GPIO_InitStruct)` |
| 设置引脚高电平 | `GPIO_SetBits(GPIOx, GPIO_Pin_x)` | `HAL_GPIO_WritePin(GPIOx, Pin, GPIO_PIN_SET)` |
| 设置引脚低电平 | `GPIO_ResetBits(GPIOx, GPIO_Pin_x)` | `HAL_GPIO_WritePin(GPIOx, Pin, GPIO_PIN_RESET)` |
| 写入引脚电平 | `GPIO_WriteBit(GPIOx, Pin, BitValue)` | `HAL_GPIO_WritePin(GPIOx, Pin, (GPIO_PinState)BitValue)` |
| 读取引脚电平 | `GPIO_ReadInputDataBit(GPIOx, Pin)` | `HAL_GPIO_ReadPin(GPIOx, Pin)` |
| 读取输出电平 | `GPIO_ReadOutputDataBit(GPIOx, Pin)` | `HAL_GPIO_ReadPin(GPIOx, Pin)`（统一接口） |
| 结构体类型 | `GPIO_InitTypeDef` | `GPIO_InitTypeDef`（名称兼容） |
| 引脚宏定义 | `GPIO_Pin_0` — `GPIO_Pin_15` | `GPIO_PIN_0` — `GPIO_PIN_15`（宏名不同） |
| 模式枚举 | `GPIO_Mode_Out_PP` / `GPIO_Mode_IPU` 等 | `GPIO_MODE_OUTPUT_PP` / `GPIO_MODE_INPUT` 等 |

**关键差异**：STD使用单独函数区分设置/复位，HAL统一通过`HAL_GPIO_WritePin`的第三个参数控制电平；输出读取在STD中为专用函数，HAL中与输入读取共用。

---

### 2. RCC — 复位与时钟控制

| 功能 | STD 标准库 | HAL 库 |
|------|-----------|--------|
| 外设时钟使能(APB2) | `RCC_APB2PeriphClockCmd(RCC_APB2Periph_x, ENABLE)` | `__HAL_RCC_GPIOx_CLK_ENABLE()` |
| 外设时钟使能(APB1) | `RCC_APB1PeriphClockCmd(RCC_APB1Periph_x, ENABLE)` | `__HAL_RCC_x_CLK_ENABLE()` |
| ADC时钟分频 | `RCC_ADCCLKConfig(RCC_PCLK2_Div6)` | `HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit)` |
| LSE晶振配置 | `RCC_LSEConfig(RCC_LSE_ON)` | `HAL_RCC_OscConfig(&RCC_OscInitStruct)` |
| RTC时钟选择 | `RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE)` | `HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit)` |
| RTC时钟使能 | `RCC_RTCCLKCmd(ENABLE)` | （CubeMX通过`HAL_RCCEx_PeriphCLKConfig`完成） |
| LSE就绪标志 | `RCC_GetFlagStatus(RCC_FLAG_LSERDY)` | （CubeMX生成的`HAL_RCC_OscConfig`内部处理） |
| 系统时钟配置 | 无（手动配置） | `HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_x)` |
| 时钟结构体 | 各功能独立宏定义 | `RCC_OscInitTypeDef`, `RCC_ClkInitTypeDef`, `RCC_PeriphCLKInitTypeDef` |

**关键差异**：STD中RCC操作为分散的函数调用，HAL中通过CubeMX生成统一的结构体配置流程；STD时钟使能函数被替换为`__HAL_RCC_xxx_CLK_ENABLE()`宏。

---

### 3. TIM — 定时器

| 功能 | STD 标准库 | HAL 库 |
|------|-----------|--------|
| 时基初始化 | `TIM_TimeBaseInit(TIMx, &TIM_TimeBaseInitStructure)` | `HAL_TIM_Base_Init(&htimx)` |
| 内部时钟选择 | `TIM_InternalClockConfig(TIMx)` | `HAL_TIM_ConfigClockSource(&htimx, &sClockSourceConfig)` |
| 清除更新标志 | `TIM_ClearFlag(TIMx, TIM_FLAG_Update)` | （HAL库内部处理） |
| 中断使能 | `TIM_ITConfig(TIMx, TIM_IT_Update, ENABLE)` | `HAL_TIM_Base_Start_IT(&htimx)` |
| 启动定时器 | `TIM_Cmd(TIMx, ENABLE)` | `HAL_TIM_Base_Start(&htimx)` / `HAL_TIM_Base_Start_IT(&htimx)` |
| 中断标志查询 | `TIM_GetITStatus(TIMx, TIM_IT_Update)` | 通过回调函数 `HAL_TIM_PeriodElapsedCallback()` |
| 清除中断标志 | `TIM_ClearITPendingBit(TIMx, TIM_IT_Update)` | HAL回调内部自动清除 |
| 主从同步配置 | 无 | `HAL_TIMEx_MasterConfigSynchronization(&htimx, &sMasterConfig)` |
| 中断处理方式 | 在`TIMx_IRQHandler`中手动调用`TIM_GetITStatus`判断 | `HAL_TIM_IRQHandler(&htimx)`统一分发到HAL回调 |
| 回调机制 | 无（直接写中断函数） | `HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)` |
| 句柄类型 | `TIM_TimeBaseInitTypeDef` 配置结构体 | `TIM_HandleTypeDef` 句柄结构体 |

**关键差异**：HAL使用面向对象的句柄机制，初始化参数通过CubeMX生成；中断处理从手动查询标志改为HAL回调模式；`TIM_ITConfig` + `TIM_Cmd` 合并为 `HAL_TIM_Base_Start_IT`。

---

### 4. ADC — 模数转换器

| 功能 | STD 标准库 | HAL 库 |
|------|-----------|--------|
| ADC初始化 | `ADC_Init(ADCx, &ADC_InitStructure)` | `HAL_ADC_Init(&hadcx)` |
| 规则通道配置 | `ADC_RegularChannelConfig(ADCx, Channel, Rank, SampleTime)` | `HAL_ADC_ConfigChannel(&hadcx, &sConfig)` |
| ADC使能 | `ADC_Cmd(ADCx, ENABLE)` | （`HAL_ADC_Init`内部使能） |
| 校准复位 | `ADC_ResetCalibration(ADCx)` | （CubeMX自动处理） |
| 校准复位状态 | `ADC_GetResetCalibrationStatus(ADCx)` | （CubeMX自动处理） |
| 启动校准 | `ADC_StartCalibration(ADCx)` | （CubeMX自动处理） |
| 校准状态 | `ADC_GetCalibrationStatus(ADCx)` | （CubeMX自动处理） |
| 软件触发转换 | `ADC_SoftwareStartConvCmd(ADCx, ENABLE)` | `HAL_ADC_Start(&hadcx)` |
| 等待转换完成 | `while(!ADC_GetFlagStatus(ADCx, ADC_FLAG_EOC))` | `HAL_ADC_PollForConversion(&hadcx, Timeout)` |
| 读取转换值 | `ADC_GetConversionValue(ADCx)` | `HAL_ADC_GetValue(&hadcx)` |
| 句柄/结构体 | `ADC_InitTypeDef` | `ADC_HandleTypeDef` |

**关键差异**：HAL的校准流程由CubeMX生成的代码自动处理，应用层只需调用`HAL_ADC_Start`/`PollForConversion`/`GetValue`三步；轮询方式由用户自旋等待改为超时机制。

---

### 5. RTC — 实时时钟

| 功能 | STD 标准库 | HAL 库 |
|------|-----------|--------|
| RTC初始化 | （直接配置寄存器） | `HAL_RTC_Init(&hrtc)` |
| 等待同步 | `RTC_WaitForSynchro()` | （HAL内部处理） |
| 等待上次操作完成 | `RTC_WaitForLastTask()` | （HAL内部处理） |
| 设置预分频 | `RTC_SetPrescaler(Div)` | （`HAL_RTC_Init`通过`hrtc.Init.AsynchPrediv`配置） |
| 设置计数器 | `RTC_SetCounter(value)` | `HAL_RTC_SetTime(&hrtc, &sTime, Format)` |
| 读取计数器 | `RTC_GetCounter()` | `HAL_RTC_GetTime(&hrtc, &sTime, Format)` |
| 设置日期 | 无（STD RTC只有时间计数器） | `HAL_RTC_SetDate(&hrtc, &sDate, Format)` |
| 读取日期 | 无 | `HAL_RTC_GetDate(&hrtc, &sDate, Format)` |
| 时间格式 | 纯二进制计数器 | BCD或二进制（通过Format参数选择） |
| 句柄类型 | 无 | `RTC_HandleTypeDef` |

**关键差异**：HAL RTC提供日期和时间分离的API，支持BCD/BIN格式选择；STD使用简单的32位计数器，日期需软件计算；STD的手动同步等待操作在HAL中由库内部自动处理。

---

### 6. BKP — 备份寄存器

| 功能 | STD 标准库 | HAL 库 |
|------|-----------|--------|
| 读取备份寄存器 | `BKP_ReadBackupRegister(BKP_DRx)` | `HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DRx)` |
| 写入备份寄存器 | `BKP_WriteBackupRegister(BKP_DRx, Data)` | `HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DRx, Data)` |
| 所属模块 | 独立BKP外设 | RTC扩展功能（`HAL_RTCEx`） |

**关键差异**：HAL中BKP操作归入RTC扩展模块，需要传入RTC句柄；STD中为独立外设函数。

---

### 7. PWR — 电源控制

| 功能 | STD 标准库 | HAL 库 |
|------|-----------|--------|
| 备份域访问使能 | `PWR_BackupAccessCmd(ENABLE)` | （CubeMX自动配置） |

**关键差异**：HAL项目中备份域访问由CubeMX在RCC初始化中自动完成，无需手动调用。

---

### 8. NVIC — 嵌套向量中断控制器

| 功能 | STD 标准库 | HAL 库 |
|------|-----------|--------|
| 优先级分组 | `NVIC_PriorityGroupConfig(NVIC_PriorityGroup_x)` | `HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_x)`（CubeMX生成） |
| 中断通道初始化 | `NVIC_Init(&NVIC_InitStructure)` | `HAL_NVIC_SetPriority(IRQn, Preempt, Sub)` + `HAL_NVIC_EnableIRQ(IRQn)` |
| 中断使能 | `NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE` | `HAL_NVIC_EnableIRQ(IRQn)` |
| 优先级设置 | `NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority` | `HAL_NVIC_SetPriority(IRQn, PreemptPriority, SubPriority)` |

**关键差异**：HAL将NVIC配置拆分为独立的优先级设置和使能函数，中断配置由CubeMX生成在`HAL_MspInit`中；STD通过一个`NVIC_Init`函数完成全部配置。

---

### 9. 延时与系统时钟

| 功能 | STD 标准库 | HAL 库 |
|------|-----------|--------|
| 微秒延时 | `Delay_us(x)`（SysTick实现） | `delay_us(x)`（DWT实现，STM32F1特有） |
| 毫秒延时 | `Delay_ms(x)`（封装Delay_us） | `delay_ms(x)`（封装delay_us） |
| 秒级延时 | `Delay_s(x)`（封装Delay_ms） | `delay_ms(x*1000)`（直接调用） |
| HAL系统滴答 | 无 | `HAL_IncTick()`（在SysTick_Handler中调用） |
| 等待中断 | `__WFI()`（CMSIS） | `__WFI()`（CMSIS，保持不变） |
| 关中断 | `__disable_irq()`（CMSIS） | `__disable_irq()`（CMSIS，保持不变） |

**关键差异**：STD使用SysTick实现微秒级延时（读取`SysTick->LOAD/VAL/CTRL`寄存器），HAL改用DWT（Data Watchpoint and Trace）的CYCCNT计数器实现更高精度延时；SysTick保留给`HAL_IncTick()`使用。

---

### 10. 用户自定义函数接口对照

| 模块 | STD 版本 | HAL 版本 | 变更说明 |
|------|---------|----------|---------|
| 延时 | `void Delay_ms(uint32_t)` | `void delay_ms(uint32_t)` | 首字母小写，内部实现从SysTick改为DWT |
| 延时 | `void Delay_us(uint32_t)` | `void delay_us(uint32_t)` | 首字母小写，内部实现从SysTick改为DWT |
| 延时 | `void Delay_s(uint32_t)` | 改用 `delay_ms(x*1000)` | 独立函数取消 |
| 定时器 | `void Timer_Init(void)` | 由CubeMX生成`MX_TIM2_Init()` | 手动初始化改为CubeMX生成 |
| 电源 | 无独立模块（GPIO直接操作） | `void POWER_Shutdown(void)` | 封装为独立模块 |
| 电源 | 无 | `uint8_t POWER_IsRunning(void)` | 新增状态查询 |
| ADC | `void AD_Init(void)` | 由CubeMX生成`MX_ADC1_Init()` | 手动初始化改为CubeMX生成 |
| ADC | `uint16_t AD_GetValue(void)` | `uint16_t AD_GetValue(void)` | 函数签名不变，内部调用HAL API |
| Key | `void Key_Init(void)` | `void Key_Init(void)` | 函数签名不变（仅变量初始化） |
| Key | `uint8_t Key_GetNum(void)` | `uint8_t Key_GetNum(void)` | 函数签名不变 |
| Key | `void KeyTick(void)` | `void KeyTick(void)` | 函数签名不变 |
| Key | `void Key3_Tick(void)` | `void Key3_Tick(void)` | 函数签名不变 |
| LED | `void LEDx_ON/OFF/Turn(void)` | `void LEDx_ON/OFF/Turn(void)` | 函数签名不变，内部调用`HAL_GPIO_WritePin` |
| MyI2C | `void MyI2C_W_SCL(uint8_t)` | 新签名：`void MyI2C_W_SCL(GPIO_TypeDef*, uint16_t, uint8_t)` | 增加GPIO端口和引脚参数 |

---

### 11. 中断处理方式对比

| 中断源 | STD 处理方式 | HAL 处理方式 |
|--------|-------------|-------------|
| TIM2更新中断 | 直接在`TIM2_IRQHandler`中判断`TIM_GetITStatus`并手动清除标志 | 调用`HAL_TIM_IRQHandler(&htim2)`统一分发到`HAL_TIM_PeriodElapsedCallback`回调 |
| SysTick | 未使用（Delay模块手动读取SysTick寄存器） | 调用`HAL_IncTick()`为HAL提供系统时钟基准 |
| 应用层定时回调 | 无标准机制 | `StopClock_Tick()`和`dino_tick()`在`HAL_TIM_PeriodElapsedCallback`中调用 |

---

### 12. 初始化方式对比

| 模块 | STD 方式 | HAL 方式 |
|------|---------|---------|
| GPIO | 应用中手动调用`GPIO_Init` + `RCC_APB2PeriphClockCmd` | CubeMX生成`MX_GPIO_Init()`，应用层直接使用 |
| ADC | 应用中手动`AD_Init()`（含GPIO/RCC/ADC配置） | CubeMX生成`MX_ADC1_Init()`，应用层仅调用`AD_GetValue` |
| TIM | 应用中手动`Timer_Init()`（含RCC/TIM/NVIC配置） | CubeMX生成`MX_TIM2_Init()` + `HAL_TIM_Base_Start_IT` |
| RCC | 分散在各模块的`RCC_xxxClockCmd`调用 | CubeMX生成`SystemClock_Config()`集中管理 |
| RTC | 应用中手动`MyRTC_Init()`（含LSE/RTC/BKP配置） | CubeMX生成`MX_RTC_Init()`，应用层仅处理首次启动逻辑 |

**初始化总结**：STD的初始化代码分散在各驱动模块中，开发者需手动配置时钟使能和引脚；HAL通过CubeMX图形化配置并自动生成初始化代码，统一在`main.c`中调用，大幅减少手动配置工作。

---

### 总览：STD → HAL 函数接口映射统计

| 外设 | STD函数数 | HAL函数数 | 映射覆盖率 |
|------|----------|----------|-----------|
| GPIO | 6 | 3 | 100%（功能压缩） |
| RCC | 7 | 6 + 宏 | 100% |
| TIM | 7 | 5 | 100%（功能合并） |
| ADC | 10 | 5 | 100%（校准自动化） |
| RTC | 5 | 7 | 100%（功能扩展） |
| BKP | 2 | 2 | 100% |
| PWR | 1 | 0 | 100%（CubeMX自动处理） |
| NVIC | 2 | 3 | 100% |
| **总计** | **40** | **31** | **100%** |

所有STD标准库函数调用已全部迁移到对应的HAL库接口，无遗留。

---

## 🔧 迁移计划

### 阶段一：菜单系统 ✅ 已完成

**完成时间**: 2026-06-07

**已完成任务**:
1. ✅ 迁移 `menu.c/h`
   - 修改头文件包含
   - 修改延时函数调用
   - 适配电源控制模块
   - 集成到TIM2中断处理
   - 更新Keil项目文件

2. ✅ 更新 `stm32f1xx_it.c`
   - 添加 `StopClock_Tick()` 回调
   - 添加 `dino_tick()` 回调

3. ✅ 更新 `main.c`
   - 添加菜单系统集成
   - 添加MPU6050初始化

---

### 阶段二：游戏模块 ✅ 已完成

**完成时间**: 2026-06-08

**已完成任务**:
1. ✅ 迁移 `dino.c/h`
   - 修改头文件包含
   - 修改延时函数调用
   - 删除 `dino_stub.c`
   - 编译通过
2. ✅ 修复 `menu.c` 编码问题（UTF-16 LE → UTF-8）
3. ✅ 添加 `--no-multibyte-chars` 编译选项

---

### 阶段三：时间设置模块 ✅ 已完成

**完成时间**: 2026-06-08

**已完成任务**:
1. ✅ 迁移 `SetTime.c/h`
   - 修改头文件包含：`stm32f10x.h` → `main.h`，添加 `Hardware/` 前缀
   - API保持兼容（无需修改函数逻辑）
   - 删除 `SetTime_stub.c`
   - 更新Keil项目文件：`SetTime_stub.c` → `SetTime.c`
2. ✅ 创建 `Core/Inc/Hardware/SetTime.h` 完整头文件
3. ✅ 更新 `menu.h` 注释（移除stub标记）
4. ✅ 编译检查通过

---

### 阶段四：集成测试 (预计1小时)

**任务**:
1. 编译测试
2. 功能验证
   - 时钟显示
   - 电池监测
   - 菜单导航
   - 时间设置
   - 恐龙游戏
   - 按键响应
3. 修复编译错误
4. 性能优化

---

## ⚠️ 迁移注意事项

### 1. 头文件修改
```c
// STD
#include "stm32f10x.h"

// HAL
#include "main.h"
```

### 2. 延时函数
```c
// STD
Delay_ms(100);
Delay_us(500);

// HAL
delay_ms(100);
delay_us(500);
```

### 3. GPIO操作
```c
// STD
GPIO_SetBits(GPIOA, GPIO_Pin_0);
GPIO_ResetBits(GPIOA, GPIO_Pin_0);
GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1);

// HAL
HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin);
```

### 4. ADC操作
```c
// STD
ADC_SoftwareStartConvCmd(ADC1, ENABLE);
while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
uint16_t value = ADC_GetConversionValue(ADC1);

// HAL
HAL_ADC_Start(&hadc1);
HAL_ADC_PollForConversion(&hadc1, 100);
uint16_t value = HAL_ADC_GetValue(&hadc1);
```

### 5. 中断处理
```c
// STD
void TIM2_IRQHandler(void) {
    if(TIM_GetITStatus(TIM2, TIM_IT_Update) == SET) {
        // 处理
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}

// HAL (使用回调)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if(htim->Instance == TIM2) {
        // 处理
    }
}
```

---

## 📈 迁移优势

1. **代码生成**: CubeMX自动生成初始化代码
2. **跨平台**: HAL库支持STM32全系列
3. **标准化**: 统一的API接口
4. **低功耗**: 内置低功耗管理
5. **RTOS支持**: 更好的FreeRTOS集成

---

## 🎯 下一步行动

1. ✅ 已完成: 迁移 `menu.c/h` (核心应用)
2. ✅ 已完成: 迁移 `dino.c/h` (游戏模块)
3. ✅ 已完成: 迁移 `SetTime.c/h` (时间设置)
4. **进行中**: 集成测试与优化

---

## 📝 迁移日志

### 2026-06-07 菜单系统迁移

**修改文件**:
- 新增: `Core/Src/Hardware/menu.c` (1030行)
- 新增: `Core/Src/Hardware/menu.h` (232行)
- 新增: `Core/Src/Hardware/SetTime_stub.c` (占位)
- 新增: `Core/Src/Hardware/dino_stub.c` (占位)
- 修改: `Core/Src/main.c` (添加菜单集成)
- 修改: `Core/Src/stm32f1xx_it.c` (添加中断回调)
- 修改: `MDK-ARM/HAL.uvprojx` (添加源文件)

**主要变更**:
1. 头文件: `stm32f10x.h` → `main.h`, `Delay.h` → `delay.h`
2. 延时函数: `Delay_ms()` → `delay_ms()`
3. 电源控制: GPIO直接操作 → `POWER_Shutdown()`
4. ADC: 移除 `AD_Init()`（CubeMX已完成）
5. TIM2中断: 添加 `StopClock_Tick()` 和 `dino_tick()` 回调

**验证状态**: 
- 文件结构完整 ✓
- Keil项目配置正确 ✓
- 编译通过 ✓

---

### 2026-06-08 恐龙游戏迁移

**修改文件**:
- 新增: `Core/Inc/Hardware/dino.h` (99行)
- 新增: `Core/Src/Hardware/dino.c` (285行)
- 修改: `Core/Inc/Hardware/menu.h` (删除3个dino stub声明)
- 修改: `Core/Src/Hardware/menu.c` (UTF-16 LE→UTF-8编码修复, 添加dino.h包含)
- 修改: `MDK-ARM/HAL.uvprojx` (替换源文件, 添加编译选项)
- 修改: `MDK-ARM/HAL.uvoptx` (同步Keil配置)
- 删除: `Core/Src/Hardware/dino_stub.c`

**主要变更**:
1. dino模块从STD完整移植到HAL
2. 头文件: `stm32f10x.h` → `main.h`, `Delay.h` → `delay.h`
3. 延时函数: `Delay_s()` → `delay_ms(1000)`
4. `--no-multibyte-chars` 编译选项解决UTF-8中文编码
5. menu.c编码修复（根因：原HAL文件为UTF-16 LE编码）

**验证状态**:
- 文件结构完整 ✓
- Keil项目配置正确 ✓
- 编译通过 ✓

---

---

### 2026-06-08 时间设置模块迁移

**修改文件**:
- 新增: `Core/Inc/Hardware/SetTime.h` (75行)
- 新增: `Core/Src/Hardware/SetTime.c` (418行，从STD完整移植)
- 修改: `Core/Inc/Hardware/menu.h` (移除SetTime stub注释)
- 修改: `MDK-ARM/HAL.uvprojx` (替换SetTime_stub.c → SetTime.c)
- 删除: `Core/Src/Hardware/SetTime_stub.c`

**主要变更**:
1. 头文件: `stm32f10x.h` → `main.h`, `OLED.h`/`Key.h`/`menu.h` → `Hardware/`前缀
2. 函数逻辑保持完全不变（OLED/Key/MyRTC API接口兼容）
3. 添加 `#include "Hardware/SetTime.h"` 自包含头文件
4. Keil项目文件更新，include path已包含`Core/Inc/Hardware`

**验证状态**:
- 文件结构完整 ✓
- 头文件与函数声明一致 ✓
- 项目文件配置正确 ✓

**报告生成时间**: 2026-06-08 17:00  
**分析工具**: Sisyphus AI Agent
