# Phase 1 · HAL 库迁移参考文档

> **创建日期**: 2026-05-26
> **分析工具**: OpenCode Sisyphus + Explore Agents
> **项目仓库**: https://github.com/CHSGY/My_SmartWatch_STM32_STL
> **目的**: 为 Phase 1 HAL 库迁移提供完整的技术参考

---

## 一、项目现状总览

| 维度 | 当前状态 |
|------|----------|
| MCU | STM32F103C8T6 (Cortex-M3, 72MHz, 64KB Flash, 20KB SRAM) |
| 固件库 | STM32 SPL V3.5.0（标准外设库，已停产） |
| 编译器 | ARMCC V5.06 update 7 (Keil MDK 5) |
| 架构 | 裸机超级循环 (bare-metal superloop) + `__WFI` 休眠 |
| 定时器 | TIM2 → 1ms 系统 tick（唯一外设中断） |
| I2C | 两套独立软件 I2C（bit-banging），无硬件 I2C |
| ADC | 阻塞轮询模式，无 DMA |
| RTC | 使用 STM32 内置 RTC + LSE 32.768kHz 晶振 |

---

## 二、项目架构

```
My_SmartWatch_STM32_STL/
├── User/               # 应用入口
│   ├── main.c          # TIM2_IRQHandler + main while(1)
│   ├── stm32f10x_it.c  # Cortex-M3 异常处理（模板）
│   └── stm32f10x_conf.h # SPL 库配置（包含全部外设头文件）
├── Hardware/           # 硬件驱动层
│   ├── OLED.c/h        # SSD1306 OLED 驱动（内含独立软件 I2C）
│   ├── MyI2C.c/h       # 软件 I2C 主设备（用于 MPU6050）
│   ├── MPU6050.c/h     # 6 轴传感器驱动
│   ├── Key.c/h         # 按键输入（消抖 + 长按检测）
│   ├── LED.c/h         # LED 控制
│   ├── AD.c/h          # ADC 电池电压监测
│   ├── menu.c/h        # 菜单系统 UI（含 7 个阻塞 while(1)）
│   ├── dino.c/h        # 恐龙跑酷游戏
│   └── SetTime.c/h     # RTC 时间设置界面
├── System/             # 系统模块
│   ├── Timer.c/h       # TIM2 定时器（1ms tick）
│   ├── MyRTC.c/h       # RTC 实时时钟
│   └── Delay.c/h       # 延时函数（直接操作 SysTick）
├── Library/            # STM32 SPL V3.5.0 完整库（46 个文件）
├── Start/              # 启动文件和系统配置
│   ├── startup_stm32f10x_md.s
│   ├── system_stm32f10x.c/h
│   ├── core_cm3.c/h
│   └── stm32f10x.h
└── Project.uvprojx     # Keil 工程文件
```

---

## 三、外设使用清单

### 3.1 实际使用的外设（仅 6 个）

| 外设 | 用途 | 涉及文件 | SPL 函数数量 | HAL 迁移难度 |
|------|------|----------|-------------|-------------|
| **GPIO** | 按键、LED、软件 I2C 引脚、电源控制 | MyI2C.c, OLED.c, Key.c, LED.c, AD.c, menu.c | ~15 个 | ⭐ 低 |
| **TIM2** | 1ms 系统 tick 定时器 | Timer.c, main.c | ~8 个 | ⭐⭐ 中低 |
| **NVIC** | 中断优先级配置 | Timer.c | ~3 个 | ⭐ 低 |
| **ADC1** | 电池电压采样（PA2, 通道 2） | AD.c | ~12 个 | ⭐⭐ 中 |
| **RTC** | 实时时钟 | MyRTC.c | ~5 个 | ⭐⭐⭐ 中高 |
| **PWR/BKP** | 备份域访问、RTC 首次配置标志 | MyRTC.c | ~4 个 | ⭐⭐ 中低 |

### 3.2 未使用的外设（Library 中存在但项目未用）

SPI, USART, I2C(硬件), DMA, CAN, DAC, EXTI, FSMC, SDIO, IWDG, WWDG, CEC, CRC

---

## 四、SPL 函数使用清单（按文件分类）

### 4.1 GPIO 操作（6 个源文件，最高频使用）

| 文件 | SPL 函数调用 |
|------|------------|
| `MyI2C.c` | `RCC_APB2PeriphClockCmd`, `GPIO_Init`, `GPIO_WriteBit`(x2), `GPIO_ReadInputDataBit`, `GPIO_SetBits` |
| `OLED.c` | `RCC_APB2PeriphClockCmd`, `GPIO_Init`(x2), `GPIO_WriteBit`(x2) |
| `Key.c` | `RCC_APB2PeriphClockCmd`(x2), `GPIO_Init`(x2), `GPIO_ReadInputDataBit`(x7) |
| `LED.c` | `RCC_APB2PeriphClockCmd`, `GPIO_Init`(x2), `GPIO_SetBits`(x4), `GPIO_ResetBits`(x3), `GPIO_ReadOutputDataBit`(x2) |
| `AD.c` | `RCC_APB2PeriphClockCmd`(x2), `GPIO_Init` |
| `menu.c` | `GPIO_ReadOutputDataBit`(x1), `GPIO_ResetBits`(x3), `GPIO_SetBits`(x3) |

### 4.2 定时器操作

| 文件 | SPL 函数调用 |
|------|------------|
| `Timer.c` | `RCC_APB1PeriphClockCmd`, `TIM_InternalClockConfig`, `TIM_TimeBaseInit`, `TIM_ClearFlag`, `TIM_ITConfig`, `NVIC_PriorityGroupConfig`, `NVIC_Init`, `TIM_Cmd` |
| `main.c` | `TIM_GetITStatus`, `TIM_ClearITPendingBit` |

### 4.3 ADC 操作

| 文件 | SPL 函数调用 |
|------|------------|
| `AD.c` | `RCC_APB2PeriphClockCmd`(x2), `RCC_ADCCLKConfig`, `ADC_RegularChannelConfig`, `ADC_Init`, `ADC_Cmd`, `ADC_ResetCalibration`, `ADC_GetResetCalibrationStatus`, `ADC_StartCalibration`, `ADC_GetCalibrationStatus`, `ADC_SoftwareStartConvCmd`, `ADC_GetFlagStatus`, `ADC_GetConversionValue` |

### 4.4 RTC/PWR/BKP 操作

| 文件 | SPL 函数调用 |
|------|------------|
| `MyRTC.c` | `RCC_APB1PeriphClockCmd`(x2), `PWR_BackupAccessCmd`, `BKP_ReadBackupRegister`, `RCC_LSEConfig`, `RCC_GetFlagStatus`, `RCC_RTCCLKConfig`, `RCC_RTCCLKCmd`, `RTC_WaitForSynchro`, `RTC_WaitForLastTask`(x4), `RTC_SetPrescaler`, `BKP_WriteBackupRegister`, `RTC_SetCounter`, `RTC_GetCounter` |

---

## 五、直接寄存器操作分析

**在用户代码中**（排除 Library 库内部实现），唯一的直接寄存器操作出现在 `Delay.c`：

```c
// System/Delay.c - SysTick 寄存器直接操作
SysTick->LOAD = 72 * xus;              // 重装值
SysTick->VAL = 0x00;                   // 清空计数
SysTick->CTRL = 0x00000005;            // 启动定时器
while(!(SysTick->CTRL & 0x00010000));  // 等待计数到 0
SysTick->CTRL = 0x00000004;            // 关闭定时器
```

**在系统启动文件中**（`Start/system_stm32f10x.c`）有大量 RCC 寄存器直接操作，但这是 CMSIS 标准文件，HAL 迁移时会用对应的 HAL/CMSIS 文件替换。

**重要发现**：用户代码中**没有**对 GPIOA->ODR、TIM2->CR1、RTC->CNT 等外设寄存器的直接操作。所有外设访问均通过 SPL API 封装。

---

## 六、软件 I2C 实现分析

项目中存在**两套独立的软件 I2C 实现**：

### 6.1 MyI2C.c（用于 MPU6050 通信）

- **引脚**: PB10(SCL), PB11(SDA)
- **模式**: 开漏输出 `GPIO_Mode_Out_OD`
- **速度**: 50MHz
- **延时**: 每次操作后 `Delay_us(10)`
- **SPL 依赖**: `GPIO_WriteBit`, `GPIO_ReadInputDataBit`, `GPIO_Init`, `RCC_APB2PeriphClockCmd`, `GPIO_SetBits`

### 6.2 OLED.c 内部 I2C（用于 OLED 通信）

- **引脚**: PB8(SCL), PB9(SDA)
- **模式**: 开漏输出 `GPIO_Mode_Out_OD`
- **速度**: 50MHz
- **延时**: 无额外延时（注释掉）
- **特点**: 独立实现，不依赖 MyI2C.c，发送时忽略 ACK 应答
- **SPL 依赖**: `GPIO_WriteBit`, `GPIO_Init`, `RCC_APB2PeriphClockCmd`

**迁移影响**: 这两套软件 I2C 不需要改为硬件 I2C，只需将 GPIO SPL 调用替换为 HAL GPIO 调用即可。

---

## 七、中断配置详情

### 7.1 IRQHandler 清单

| 中断处理函数 | 定义位置 | 功能 |
|---|---|---|
| `TIM2_IRQHandler` | `main.c:46` | 1ms 定时中断，执行所有 tick 任务 |
| `NMI_Handler` | `stm32f10x_it.c:47` | 空函数 |
| `HardFault_Handler` | `stm32f10x_it.c:56` | 死循环 while(1) |
| `MemManage_Handler` | `stm32f10x_it.c:69` | 死循环 while(1) |
| `BusFault_Handler` | `stm32f10x_it.c:82` | 死循环 while(1) |
| `UsageFault_Handler` | `stm32f10x_it.c:95` | 死循环 while(1) |
| `SVC_Handler` | `stm32f10x_it.c:108` | 空函数（FreeRTOS 迁移需替换） |
| `PendSV_Handler` | `stm32f10x_it.c:126` | 空函数（FreeRTOS 迁移需替换） |
| `SysTick_Handler` | `stm32f10x_it.c:135` | 空函数（FreeRTOS 迁移需替换） |

**关键发现**: `Timer.c:55-62` 中的 `TIM2_IRQHandler` 已被注释掉，实际定义在 `main.c:46`。**无 EXTI 中断、无 USART 中断、无 DMA 中断。**

### 7.2 TIM2_IRQHandler 内部执行的全部任务

```c
// main.c:46-62 — 每 1ms 执行一次
void TIM2_IRQHandler(void) {
    if(TIM_GetITStatus(TIM2, TIM_IT_Update) == SET) {
        Key3_Tick();          // 按键 3 长按时间计数（GPIO 轮询）
        KeyTick();            // 按键消抖状态机（每 20ms 检测一次状态）
        if(start_timing_flag == 1)
            StopClock_Tick(); // 秒表 1 秒计时（1000 次 tick 后 sec++）
        dino_tick();          // 游戏物理更新（分数/地面/障碍物/云朵/跳跃）
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}
```

**ISR 耗时分析**: 估算总耗时 ~2-5us，占 1ms 周期的 0.2-0.5%，负载极轻。

### 7.3 NVIC 配置

```c
// Timer.c:36 — 全局唯一 NVIC 分组配置
NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
// 分组 2: 2 位抢占优先级(0~3) + 2 位响应优先级(0~3)

// Timer.c:42-48 — TIM2 中断优先级
NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;  // 抢占优先级 2
NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;         // 响应优先级 1
```

| 配置项 | 值 | 说明 |
|---|---|---|
| NVIC 分组 | Group_2 | 2bit 抢占 + 2bit 响应 |
| 使能的中断 | 仅 TIM2_IRQn | 全局唯一外设中断 |
| TIM2 抢占优先级 | 2 | 中等优先级 |
| TIM2 响应优先级 | 1 | |

---

## 八、主循环 while(1) 结构分析

### 8.1 main.c 主循环

```c
// main.c:75-93
while (1) {
    OLED_Clear();
    Battery_Show_UI();          // 电池电量显示（每 50 帧才做一次 ADC 采样）
    OLED_Update();
    ClockUI_Move_Flag = First_Page_Clock();  // ← 阻塞 while(1)，__WFI 等待
    if (ClockUI_Move_Flag == 1)
        Menu_Page();            // ← 阻塞 while(1)
    else if (ClockUI_Move_Flag == 2)
        SettingPage();          // ← 阻塞 while(1)
}
```

### 8.2 嵌套阻塞调用链

```
main while(1)
  └─ First_Page_Clock()       while(1) + __WFI  ← 阻塞等按键
       └─ Menu_Page()          while(1) + __WFI  ← 阻塞等按键
            ├─ StopClock()      while(1)          ← 无 __WFI，全速轮询
            ├─ flashlight_Func() while(1)         ← 无 __WFI，全速轮询
            ├─ MPU6050_Main()   while(1)          ← 无 __WFI，含 Delay_ms(5)
            ├─ Game()/Dino_game_Animation() while(1) ← 无 __WFI，全速刷新
            ├─ Emoji_Func()     while(1)          ← 无 __WFI，含 Delay_ms
            └─ Gradienter_Func() while(1) + __WFI ← 有 __WFI
```

### 8.3 menu.c 中所有 while(1) 阻塞函数汇总

| 函数 | 行号 | __WFI | 功能 |
|---|---|---|---|
| `First_Page_Clock()` | 113 | 有 | 时钟首页光标移动 |
| `SettingPage()` | 197 | 无 | 设置页面 |
| `Menu_Page()` | 364 | 有 | 菜单滑动选择 |
| `StopClock()` | 566 | 无 | 秒表功能 |
| `flashlight_Func()` | 689 | 无 | 手电筒 |
| `MPU6050_Main()` | 815 | 无 | 传感器数据展示 |
| `Game()` | 853 | 无 | 游戏入口选择 |
| `Emoji_Func()` | 973 | 无 | 表情动画 |
| `Gradienter_Func()` | 1015 | 有 | 水平仪 |
| `Dino_game_Animation()` | 254(dino.c) | 无 | 游戏主循环 |

---

## 九、ADC 采样方式分析

```c
// AD.c:52-56 — 阻塞轮询模式，无 DMA，无中断
uint16_t AD_GetValue(void) {
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);                // 软件触发
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET); // ← 阻塞等待 EOC
    return ADC_GetConversionValue(ADC1);
}
```

| 配置项 | 值 |
|---|---|
| 采样方式 | **软件触发 + 阻塞轮询** |
| DMA | **未使用** |
| ADC 中断 | **未使用** |
| 连续转换 | DISABLE（单次转换） |
| 扫描模式 | DISABLE（单通道） |
| ADC 时钟 | 72MHz/6 = 12MHz |
| 采样时间 | 55.5 cycles |
| 通道 | ADC_Channel_2 (PA2) |
| 单次采样耗时 | (55.5 + 12.5) / 12MHz ≈ 5.7us |

---

## 十、低功耗模式分析

**当前使用**: `__WFI()` (Wait For Interrupt) — ARM Cortex-M3 最低功耗等待模式

```c
// 使用 __WFI 的位置:
menu.c:172  — First_Page_Clock() 循环末尾
menu.c:475  — Menu_Page() 循环末尾
menu.c:1027 — Gradienter_Func() 循环末尾
```

**未使用的低功耗模式**: PWR_EnterSTOPMode(), PWR_EnterSTANDBYMode(), 无 PVD 配置

**PMOS 电源控制**: `First_Page_Clock()` 中按键 3 长按 (KeyNum==4) 可通过 PB12/PB13 控制 PMOS 实现硬件关机。

---

## 十一、引脚分配汇总

| 引脚 | 功能 | 涉及文件 | HAL 迁移 |
|------|------|----------|----------|
| PB8 | OLED SCL (软件 I2C) | OLED.c | 替换 GPIO 调用 |
| PB9 | OLED SDA (软件 I2C) | OLED.c | 替换 GPIO 调用 |
| PB10 | MPU6050 SCL (软件 I2C) | MyI2C.c | 替换 GPIO 调用 |
| PB11 | MPU6050 SDA (软件 I2C) | MyI2C.c | 替换 GPIO 调用 |
| PB1 | 按键 1（上） | Key.c | 替换 GPIO 调用 |
| PA6 | 按键 2（下） | Key.c | 替换 GPIO 调用 |
| PA4 | 按键 3（确认） | Key.c | 替换 GPIO 调用 |
| PA0 | LED1 | LED.c | 替换 GPIO 调用 |
| PA2 | ADC 电池电压 | AD.c | 替换 ADC 调用 |
| PB12 | LED2 / PMOS 电源控制 | LED.c, menu.c | 替换 GPIO 调用 |
| PB13 | PMOS 电源控制 | menu.c | 替换 GPIO 调用 |

---

## 十二、构建系统现状

| 项目 | 当前值 |
|------|--------|
| 工程文件 | `Project.uvprojx` (Keil MDK 5) |
| 编译器 | ARMCC V5.06 update 7 (build 960) |
| 预定义宏 | `USE_STDPERIPH_DRIVER` |
| 包含路径 | `.\Start;.\Library;.\User;.\System;.\Hardware` |
| 启动文件 | `startup_stm32f10x_md.s` (Medium Density) |
| Flash | 0x08000000, 64KB |
| SRAM | 0x20000000, 20KB |

---

## 十三、SPL → HAL 函数映射表

### 13.1 GPIO（最简单，6 个文件涉及）

| SPL | HAL | 复杂度 |
|-----|-----|--------|
| `RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOx, ENABLE)` | `__HAL_RCC_GPIOx_CLK_ENABLE()` | ⭐ 简单宏替换 |
| `GPIO_Init(GPIOx, &GPIO_InitStructure)` | `HAL_GPIO_Init(GPIOx, &GPIO_InitStruct)` | ⭐ 结构体字段重命名 |
| `GPIO_WriteBit(GPIOx, GPIO_Pin_x, BitAction)` | `HAL_GPIO_WritePin(GPIOx, GPIO_Pin_x, PinState)` | ⭐ 简单替换 |
| `GPIO_ReadInputDataBit(GPIOx, GPIO_Pin_x)` | `HAL_GPIO_ReadPin(GPIOx, GPIO_Pin_x)` | ⭐ 简单替换 |
| `GPIO_SetBits(GPIOx, GPIO_Pin_x)` | `HAL_GPIO_WritePin(GPIOx, GPIO_Pin_x, GPIO_PIN_SET)` | ⭐ 简单替换 |
| `GPIO_ResetBits(GPIOx, GPIO_Pin_x)` | `HAL_GPIO_WritePin(GPIOx, GPIO_Pin_x, GPIO_PIN_RESET)` | ⭐ 简单替换 |

### 13.2 TIM/NVIC（中等难度）

| SPL | HAL | 复杂度 |
|-----|-----|--------|
| `TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure)` | `HAL_TIM_Base_Init(&htim2)` | ⭐⭐ 结构体需重映射 |
| `TIM_Cmd(TIM2, ENABLE)` | `HAL_TIM_Base_Start(&htim2)` | ⭐ 简单替换 |
| `TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE)` | `__HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE)` | ⭐ 简单宏替换 |
| `TIM_GetITStatus(TIM2, TIM_IT_Update)` | `__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE)` | ⭐ 简单宏替换 |
| `TIM_ClearITPendingBit(TIM2, TIM_IT_Update)` | `__HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE)` | ⭐ 简单宏替换 |
| `NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2)` | `HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4)` | ⭐ 简单替换 |
| `NVIC_Init(&NVIC_InitStructure)` | `HAL_NVIC_SetPriority()` + `HAL_NVIC_EnableIRQ()` | ⭐⭐ 需拆分 |

### 13.3 ADC（中等难度）

| SPL | HAL | 复杂度 |
|-----|-----|--------|
| `ADC_Init(ADC1, &ADC_InitStructure)` | `HAL_ADC_Init(&hadc1)` | ⭐⭐ 结构体字段不同 |
| `ADC_RegularChannelConfig(ADC1, ch, rank, time)` | `HAL_ADC_ConfigChannel(&hadc1, &sConfig)` | ⭐⭐ 配置方式不同 |
| `ADC_SoftwareStartConvCmd(ADC1, ENABLE)` + `while(!EOC)` | `HAL_ADC_Start(&hadc1)` + `HAL_ADC_PollForConversion()` | ⭐⭐ API 合并 |
| `ADC_ResetCalibration()` + 校准序列 | `HAL_ADCEx_Calibration_Start(&hadc1)` | ⭐ 简化 |
| `ADC_GetConversionValue(ADC1)` | `HAL_ADC_GetValue(&hadc1)` | ⭐ 简单替换 |

### 13.4 RTC（最复杂）

| SPL | HAL | 复杂度 |
|-----|-----|--------|
| `RTC_SetCounter(time_t)` | `HAL_RTC_SetTime(&hrtc, &sTime, FORMAT_BIN)` | ⭐⭐⭐ API 完全不同 |
| `RTC_GetCounter()` → `localtime()` | `HAL_RTC_GetTime()` + `HAL_RTC_GetDate()` | ⭐⭐⭐ 需重写 |
| `RTC_SetPrescaler(32767)` | 在 `HAL_RTC_Init()` 中配置 | ⭐⭐ 配置方式不同 |
| `RTC_WaitForSynchro()` / `RTC_WaitForLastTask()` | HAL 自动处理 | ⭐ 简化 |
| `PWR_BackupAccessCmd(ENABLE)` | `HAL_PWR_EnableBkUpAccess()` | ⭐ 简单替换 |
| `BKP_ReadBackupRegister()` | `HAL_RTCEx_BKUPRead()` | ⭐ 简单替换 |
| `BKP_WriteBackupRegister()` | `HAL_RTCEx_BKUPWrite()` | ⭐ 简单替换 |

---

## 十四、迁移可行性结论

### 总体评估：✅ 完全可行，难度中等偏低

| 评估维度 | 结论 |
|----------|------|
| **技术可行性** | ✅ 完全可行。无硬件限制，HAL 完全支持 STM32F103C8 |
| **工作量** | 中等偏低。约 15 个 .c 文件需修改，主要是函数名替换 |
| **风险** | 低。可逐模块替换验证，随时回退 |
| **最大障碍** | RTC 模块 API 差异较大，需重写时间读写逻辑 |
| **预估时间** | 2-3 周（业余时间），符合计划 |

### ✅ 有利因素

1. **无直接寄存器操作**（用户代码中）— 所有外设访问均通过 SPL API，映射关系清晰
2. **仅 1 个外设中断**（TIM2）— 中断迁移工作量极小
3. **无 DMA、无硬件 I2C/SPI/USART** — 避免了最复杂的外设迁移
4. **GPIO 操作占主导**（6 个源文件）— GPIO 是最简单的迁移对象
5. **代码量适中** — 用户代码约 15 个 .c 文件，~3000 行
6. **软件 I2C 可保留 bit-banging 逻辑** — 只需替换底层 GPIO 调用

### ⚠️ 挑战因素

1. **RTC API 差异大** — SPL 用 `RTC_SetCounter(seconds)`，HAL 用 `HAL_RTC_SetTime(RTC_TimeTypeDef)`
2. **两套独立软件 I2C** — MyI2C.c (PB10/PB11) 和 OLED.c 内部 (PB8/PB9)
3. **TIM2_IRQHandler 在 main.c 中** — HAL 迁移后需移到 stm32f10x_it.c 或专用文件
4. **ADC 校准序列不同** — SPL 手动控制，HAL 自动处理
5. **SysTick 直接寄存器操作** — Delay.c 中 `SysTick->LOAD/VAL/CTRL`
6. **7 个阻塞式 while(1) 页面函数** — 不影响 HAL 迁移，但影响后续 FreeRTOS

---

## 十五、建议迁移顺序

```
Step 1: CubeMX 生成 HAL 工程骨架（.ioc + HAL 库 + 启动文件）
    ↓
Step 2: GPIO 迁移（Key, LED, MyI2C, OLED GPIO 层）← 最简单，验证工具链
    ↓
Step 3: TIM2/NVIC 迁移 ← 系统 tick 基础
    ↓
Step 4: ADC 迁移 ← 独立模块，阻塞→DMA 可选
    ↓
Step 5: RTC 迁移 ← 最复杂，需重写 MyRTC.c
    ↓
Step 6: 功能回归测试（时钟、菜单、游戏、水平仪、表情包）
```

---

## 十六、关键建议

1. **先做 GPIO**：GPIO 迁移最简单，可以快速验证 CubeMX 生成的 HAL 工程是否正常工作
2. **软件 I2C 保留 bit-banging**：只需替换底层 `GPIO_WriteBit` → `HAL_GPIO_WritePin`，逻辑不变
3. **SysTick 可保留直接操作**：`Delay.c` 中的 SysTick 寄存器操作精度足够，不必改用 `HAL_Delay`
4. **RTC 先写原型**：建议先单独写一个 HAL RTC 初始化 + 读写示例，验证通过后再集成
5. **TIM2_IRQHandler 移到专用文件**：HAL 迁移时顺便整理中断处理函数位置

---

## 十七、FreeRTOS 引入的前置条件（Phase 2 参考）

| 问题 | 位置 | 影响 |
|---|---|---|
| TIM2_IRQHandler 在 main.c 中定义 | main.c:46 | FreeRTOS SysTick 冲突，需删除或移到专用文件 |
| SVC_Handler / PendSV_Handler 空实现 | stm32f10x_it.c:108,126 | FreeRTOS 需要替换这两个 handler |
| SysTick_Handler 空实现 | stm32f10x_it.c:135 | FreeRTOS 需要使用 SysTick |
| 7 个 while(1) 阻塞式页面函数 | menu.c 各处 | 必须改为状态机才能让出 CPU 给其他任务 |
| Key_GetNum() 共享变量无保护 | Key.c:42 | 多任务下需要临界区保护 |
| ADC 阻塞轮询 | AD.c:55 | 应改为 DMA 或中断方式 |

---

*文档生成日期：2026-05-26*
*分析工具：OpenCode Sisyphus + Explore Agents*
*基于项目仓库 https://github.com/CHSGY/My_SmartWatch_STM32_STL 的代码审查*
