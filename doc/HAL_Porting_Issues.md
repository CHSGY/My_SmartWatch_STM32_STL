# HAL 库移植问题记录

本文档记录了将 STM32 智能手表项目从标准外设库 (STD) 迁移到 HAL 库过程中遇到的问题及解决方案。

---

## 问题一：OLED_Data.c 编译报错 — missing closing quote

### 报错现象

编译 `OLED_Data.c` 时出现大量 `missing closing quote` 错误，集中在 `OLED_CF16x16[]` 中文字符数组区域（约第 720~780 行），共 30 个错误：

```
..\Core\Src\Hardware\OLED_Data.c(722): error:  #8: missing closing quote
    "锛?",
..\Core\Src\Hardware\OLED_Data.c(726): error:  #8: missing closing quote
    "銆?",
..\Core\Src\Hardware\OLED_Data.c(730): error:  #8: missing closing quote
    "浣?",
...
..\Core\Src\Hardware\OLED_Data.c: 0 warnings, 30 errors
```

其余文件（`OLED.c`、`LED.c`、`Key.c`、`main.c` 等）均编译正常。

### 原因分析

**根本原因：源文件编码与 Keil 编译器预期的编码不匹配。**

- `OLED_Data.c` 中的中文字符（如 `"，"`, `"。"`, `"你"` 等）以 **UTF-8** 编码存储，每个中文字符占 3 个字节。
- Keil ARMCC 5.06 在中文 Windows 系统上默认按系统 locale（**GBK / CP936**）读取源文件。
- 当 UTF-8 的 3 字节序列被当作 GBK 的 2 字节序列解析时，产生非法的"半个字符"，导致字符串字面量的闭合引号 `"` 无法被正确识别。

**示例对比：**

| 字符 | UTF-8 编码 | GBK 编码 | 错误解析结果 |
|------|-----------|---------|-------------|
| `，` | `EF BC 8C` | `A3 AC` | `EF BC` → "锛"，`8C` → 未闭合 |
| `。` | `E3 80 82` | `A1 A3` | `E3 80` → "銆"，`82` → 未闭合 |
| `你` | `E4 BD A0` | `C4 E3` | `E4 BD` → "浣"，`A0` → 未闭合 |

### 解决方案

**需要同时修改两处：**

#### 1. 添加编译器标志 `--no-multibyte-chars`

在 Keil 工程文件 `HAL.uvprojx` 中，找到 C 编译器的 `MiscControls` 选项，添加 `--no-multibyte-chars` 标志：

```xml
<!-- MDK-ARM/HAL.uvprojx -->
<VariousControls>
  <MiscControls>--no-multibyte-chars</MiscControls>
  <Define>USE_HAL_DRIVER,STM32F103xB</Define>
  ...
</VariousControls>
```

> **注意：** 该工程文件中存在两处 `<MiscControls>` 节点，第一处（约第 340 行）是 C 编译器选项，第二处（约第 358 行）是汇编器选项。只需修改 C 编译器的那一处。

#### 2. 将 OLED_Data.c 转换为 GBK 编码

使用 PowerShell 将文件从 UTF-8 转换为 GBK 编码：

```powershell
$path = "SmartWatch_HAL\HAL\Core\Src\Hardware\OLED_Data.c"
$content = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
$gbk = [System.Text.Encoding]::GetEncoding("GBK")
[System.IO.File]::WriteAllText($path, $content, $gbk)
```

> **注意：** STD 工程的原始文件保持 UTF-8 编码不变，仅对 HAL 工程的副本进行转换。

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `SmartWatch_HAL/HAL/MDK-ARM/HAL.uvprojx` | C 编译器 MiscControls 添加 `--no-multibyte-chars` |
| `SmartWatch_HAL/HAL/Core/Src/Hardware/OLED_Data.c` | 文件编码从 UTF-8 转为 GBK |

---

## OLED 驱动移植改动总览

除上述编码问题外，OLED 驱动本身的移植改动量极小，遵循已有的 LED/Key 迁移模式：

### 文件规划

| 操作 | 文件 | 改动说明 |
|------|------|---------|
| 新建 | `Core/Inc/Hardware/OLED.h` | 直接复制，零改动 |
| 新建 | `Core/Inc/Hardware/OLED_Data.h` | 直接复制，零改动 |
| 新建 | `Core/Src/Hardware/OLED_Data.c` | 复制后转换编码（UTF-8 → GBK） |
| 新建 | `Core/Src/Hardware/OLED.c` | 复制后微调 4 处（见下方） |
| 修改 | `Core/Src/main.c` | 添加 `#include` 和 `OLED_Init()` |
| 修改 | `MDK-ARM/HAL.uvprojx` | 添加源文件到工程、添加编译器标志 |

### OLED.c 的 4 处改动

```c
// 1. 头文件替换
#include "stm32f10x.h"    →  #include "main.h"
#include "OLED.h"          →  #include "Hardware/OLED.h"

// 2. SCL 引脚操作（GPIO_WriteBit → HAL_GPIO_WritePin）
GPIO_WriteBit(GPIOB, GPIO_Pin_8, (BitAction)BitValue);
→
HAL_GPIO_WritePin(OLED_SCL_GPIO_Port, OLED_SCL_Pin, (GPIO_PinState)BitValue);

// 3. SDA 引脚操作
GPIO_WriteBit(GPIOB, GPIO_Pin_9, (BitAction)BitValue);
→
HAL_GPIO_WritePin(OLED_SDA_GPIO_Port, OLED_SDA_Pin, (GPIO_PinState)BitValue);

// 4. GPIO 初始化函数简化（移除 STD 库 GPIO 配置代码）
//    CubeMX 的 MX_GPIO_Init() 已完成 PB8/PB9 的开漏输出配置
//    仅保留上电延时和引脚释放
```

---

## 问题二：MPU6050 驱动移植 — 软件 I2C 与微秒延时

### 背景

MPU6050 六轴传感器驱动依赖两个外部模块：

| 依赖模块 | STD 工程文件 | HAL 工程现状 |
|---------|-------------|-------------|
| 软件 I2C 总线 | `MyI2C.c` / `MyI2C.h` | **不存在**，无共享 I2C 模块 |
| 微秒延时 | `Delay.c` / `Delay.h` | **不存在**，HAL 工程无延时模块 |

STD 工程的 `MPU6050.c` 通过 `MyI2C_Start()`、`MyI2C_Stop()`、`MyI2C_SendByte()`、`MyI2C_ReceiveByte()` 进行 I2C 通信，通过 `Delay_us(2)` 实现写入后的短暂延时。

### 原因分析

**问题 1：无软件 I2C 模块**

HAL 工程中，OLED 驱动（`OLED.c`）内置了自有的软件 I2C 实现（操作 PB8/PB9），但未提取为公共模块。MPU6050 使用独立的引脚（PB10/PB11），需要独立的 I2C 总线驱动。

**问题 2：无微秒延时函数**

STD 工程的 `Delay.c` 基于 SysTick 轮询实现，存在以下问题：
- 硬编码 72MHz 时钟频率，与 HAL 工程的 8MHz HSI 不兼容
- 直接操作 SysTick 寄存器，与未来 FreeRTOS 的 SysTick 调度器冲突（FreeRTOS 需要 SysTick 中断驱动任务调度）
- 即使采用"单次轮询不开中断"模式，仍存在被 FreeRTOS 上下文切换中途抢占导致计时不准的风险

### 解决方案

#### 1. 软件 I2C：新建独立 MyI2C 模块

从标准库 `MyI2C.c` 移植，改为**参数化设计**：所有函数通过参数传入 GPIO 端口和引脚，支持多组 I2C 总线共用同一套驱动代码。

**设计思路：**

标准库的 `MyI2C` 引脚硬编码（PB10/PB11），只能驱动一组 I2C 总线。HAL 工程中 OLED（PB8/PB9）和 MPU6050（PB10/PB11）使用不同引脚，需要支持多总线。因此将引脚参数化：

```c
// 标准库：引脚硬编码
void MyI2C_Start(void);

// HAL 库：引脚参数化
void MyI2C_Start(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin,
                 GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin);
```

**调用宏简化：** 每个使用方在 `.c` 文件中定义调用宏，隐藏引脚参数：

```c
// MPU6050.c 中
#define MPU_I2C_Start()       MyI2C_Start(MPU_SCL_GPIO_Port, MPU_SCL_Pin, MPU_SDA_GPIO_Port, MPU_SDA_Pin)
#define MPU_I2C_SendByte(Byte) MyI2C_SendByte(MPU_SCL_GPIO_Port, MPU_SCL_Pin, MPU_SDA_GPIO_Port, MPU_SDA_Pin, Byte)
// ...其余类似

// 调用时无需关心引脚参数
MPU_I2C_Start();
MPU_I2C_SendByte(0xD0);
```

**改动对照表：**

| 标准库 (STD) | HAL 库 |
|-------------|--------|
| `#include "stm32f10x.h"` | `#include "main.h"` |
| `#include "Delay.h"` | `#include "delay.h"` |
| `GPIO_WriteBit(GPIOB, GPIO_Pin_x, ...)` | `HAL_GPIO_WritePin(GPIOx, GPIO_Pin, ...)` |
| `GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_x)` | `HAL_GPIO_ReadPin(GPIOx, GPIO_Pin)` |
| `Delay_us(10)` (引脚层延时) | 删除（8MHz 时钟下无需额外延时） |
| `MyI2C_Init()` (含 GPIO 配置) | 删除（GPIO 由 CubeMX 配置，MyI2C 不负责初始化） |

#### 2. 微秒延时：新建 DWT 延时模块

**方案选型对比：**

| 方案 | 原理 | FreeRTOS 兼容 | 功耗 | 精度 | 结论 |
|------|------|-------------|------|------|------|
| SysTick 轮询 | 忙等 COUNTFLAG | ⚠️ 冲突风险 | 低 | 一般 | ❌ 不采用 |
| 忙等空转 | 循环计数 | ✅ | 低 | ❌ 差（受编译优化影响） | ❌ 不采用 |
| TIM6 定时器 | 基本定时器 | ✅ | 低 | ✅ 好 | ⚠️ 可选但占用外设 |
| **DWT CYCCNT** | **内核周期计数器** | **✅** | **用完即关** | **✅ 周期级精确** | **✅ 采用** |

**最终方案：DWT CYCCNT（用完即关模式）**

```c
void delay_us(uint32_t us)
{
    /* 开启 DWT */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 忙等 */
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks);

    /* 关闭 DWT，不增加静态功耗 */
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
}
```

**关键设计决策：**

| 决策项 | 选择 | 原因 |
|--------|------|------|
| DWT vs SysTick | DWT | 与 FreeRTOS 零冲突，不占用 SysTick |
| 常开 vs 用完即关 | 用完即关 | 手表电池供电场景，空闲时零额外功耗 |
| 硬编码时钟 vs SystemCoreClock | SystemCoreClock | 自动适配时钟频率，未来改 PLL 无需修改代码 |
| 内嵌 vs 独立模块 | 独立模块 (`delay.c`) | OLED 和 MyI2C 模块均可复用 |

**DWT CYCCNT 常见疑问解答：**

| 疑问 | 解答 |
|------|------|
| 是否需要 ST-Link 连接？ | 否。`TRCENA` 是普通内存映射寄存器，CPU 正常运行时即可访问 |
| 是否影响 ST-Link 烧录调试？ | 否。SWD 接口和 DWT 是 CoreSight 调试架构的独立模块 |
| 独立电池供电能否使用？ | 能。软件设置 `TRCENA=1` 即可，与调试器连接无关 |

### 涉及文件

| 操作 | 文件 | 说明 |
|------|------|------|
| 新建 | `Core/Inc/delay.h` | DWT 延时模块头文件 |
| 新建 | `Core/Src/delay.c` | DWT 延时模块实现 |
| 新建 | `Core/Inc/MyI2C.h` | 独立软件 I2C 模块头文件（参数化，支持多总线） |
| 新建 | `Core/Src/MyI2C.c` | 独立软件 I2C 模块实现 |
| 新建 | `Core/Inc/Hardware/MPU6050_Reg.h` | 原样复制，零改动 |
| 新建 | `Core/Inc/Hardware/MPU6050.h` | 调整 include 路径 |
| 新建 | `Core/Src/Hardware/MPU6050.c` | 主要改动文件：调用独立 MyI2C + delay 模块 |
| 修改 | `Core/Src/main.c` | 添加 `#include` 和 `MPU6050_Init()`（待后续） |
| 修改 | `MDK-ARM/HAL.uvprojx` | 添加源文件到工程（待后续） |

### 遗留待办

- [ ] `main.c` 中添加 `MPU6050_Init()` 调用
- [ ] `HAL.uvprojx` 中添加新文件到工程分组
- [ ] 串口或 OLED 输出验证 MPU6050 通信是否正常（`MPU6050_GetID()` 应返回 `0x68`）

---

## 问题三：RTC 驱动移植 — 编译器版本与 HAL API 变化

### 背景

STD 工程的 `MyRTC` 模块使用 LSE（32.768kHz 外部晶振）作为 RTC 时钟源，通过秒计数器模式（`RTC_SetCounter` / `RTC_GetCounter`）存储时间，使用 C 标准库的 `mktime` / `localtime` 进行时间转换。

HAL 库的 RTC API 有重大变化：
- 时间存储从**秒计数器**改为 **Time + Date 结构体**分离模式
- 使用 `RTC_TimeTypeDef`（时分秒）和 `RTC_DateTypeDef`（年月日星期）
- 备份寄存器 API 从 `BKP_ReadBackupRegister` 改为 `HAL_RTCEx_BKUPRead`

### 报错现象

编译时报错：

```
../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c(787): error: unknown type name '__weak'
__weak void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
^
../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c(200): error: unknown type name '__weak'
__weak void HAL_MspInit(void)
^
...（75 errors total）
```

### 原因分析

**问题 1：编译器版本不兼容**

- Keil 默认选择了 **ARM Compiler V6.7 (ARMCLANG/Clang)** 编译器
- `__weak` 是 ARM Compiler 5 (ARMCC) 的特有关键字，用于定义弱符号函数
- ARMCLANG 基于 Clang/LLVM，不支持 `__weak` 语法，应使用 `__attribute__((weak))`
- STM32 HAL 库官方针对 ARMCC V5 设计，与 V6 存在语法差异

**问题 2：HAL 库 RTC API 变化**

| 功能 | 标准库 (STD) | HAL 库 |
|------|-------------|--------|
| 时间存储 | 秒计数器 `RTC_SetCounter` / `RTC_GetCounter` | `HAL_RTC_SetTime` + `HAL_RTC_SetDate` |
| 时间结构 | `time_t` + `struct tm` | `RTC_TimeTypeDef` + `RTC_DateTypeDef` |
| 备份寄存器 | `BKP_ReadBackupRegister` / `BKP_WriteBackupRegister` | `HAL_RTCEx_BKUPRead` / `HAL_RTCEx_BKUPWrite` |
| 预分频器 | `RTC_SetPrescaler(32768-1)` | `RTC_InitTypeDef.AsynchPrediv = 32767` |
| 时钟配置 | 手动代码配置 LSE | **CubeMX 配置（推荐）** |

**问题 3：MX_RTC_Init() 会覆盖 RTC 保持的时间**

CubeMX 生成的 `MX_RTC_Init()` 每次复位都会设置默认时间（0:0:0），会覆盖掉 RTC 硬件电路由电池保持的时间。需要使用备份寄存器判断是否首次配置。

### 解决方案

#### 1. 编译器版本切换

在 Keil 工程设置中切换到 ARM Compiler 5：

1. 点击工程设置（Options for Target）
2. 选择 **Target** 标签页
3. 在 **ARM Compiler** 下拉框中选择 **"Use default compiler version 5"**

> **原因：** STM32 HAL 库官方使用 ARMCC V5 语法，切换编译器是最简单可靠的解决方案。

#### 2. CubeMX 配置 RTC

HAL 库推荐通过 CubeMX 配置硬件资源，而非手动编写初始化代码：

**CubeMX 配置项：**

| 配置项 | 设置值 | 说明 |
|--------|--------|------|
| LSE 时钟源 | RCC_LSE_ON | 外部 32.768kHz 晶振 |
| RTC 时钟选择 | RCC_RTCCLKSOURCE_LSE | RTC 使用 LSE |
| 预分频器 | AsynchPrediv = 32767 | 32768-1，得到 1Hz |
| RTC Calendar | 启用 | 生成 Time/Date 初始化代码 |

**CubeMX 自动生成的代码：**

- `HAL_RTC_MspInit()`：使能 PWR/BKP 时钟、备份域访问、RTC 时钟
- `MX_RTC_Init()`：配置预分频器、设置默认时间
- `RTC_HandleTypeDef hrtc`：全局 RTC 句柄

#### 3. MyRTC 模块移植设计

**保持接口兼容：**

```c
// MyRTC.h（接口不变）
extern int16_t MyRTC_Time[6];  // 年、月、日、时、分、秒
void MyRTC_Init(void);
void MyRTC_SetTime(void);
void MyRTC_ReadTime(void);
```

**内部实现变化：**

```c
// MyRTC.c
void MyRTC_SetTime(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    
    // 从 MyRTC_Time 数组填充结构体
    sTime.Hours = MyRTC_Time[3];
    sTime.Minutes = MyRTC_Time[4];
    sTime.Seconds = MyRTC_Time[5];
    sDate.Year = MyRTC_Time[0] - 2000;  // HAL: 0-99
    sDate.Month = MyRTC_Time[1];
    sDate.Date = MyRTC_Time[2];
    
    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}
```

#### 4. 备份寄存器逻辑调整

**修改 `MX_RTC_Init()`（USER CODE BEGIN Check_RTC_BKUP 区域）：**

```c
/* USER CODE BEGIN Check_RTC_BKUP */
if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) == 0xA5A5)
{
    return;  // 非首次配置，跳过默认时间设置
}
/* USER CODE END Check_RTC_BKUP */
```

**`MyRTC_Init()` 完成首次配置：**

```c
void MyRTC_Init(void)
{
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != 0xA5A5)
    {
        MyRTC_SetTime();  // 设置默认时间
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0xA5A5);
    }
}
```

**执行流程：**

| 场景 | MX_RTC_Init() | MyRTC_Init() | 结果 |
|------|---------------|--------------|------|
| 首次上电 | BKUP != 0xA5A5 → 设置 0:0:0 | BKUP != 0xA5A5 → 设置默认时间 → 写入 0xA5A5 | RTC = MyRTC_Time 默认值 |
| 非首次上电 | BKUP == 0xA5A5 → return 跳过 | BKUP == 0xA5A5 → 不操作 | RTC 保持上次时间 |

### 涉及文件

| 操作 | 文件 | 说明 |
|------|------|------|
| 新建 | `Core/Inc/MyRTC.h` | RTC 驱动头文件（接口兼容） |
| 新建 | `Core/Src/MyRTC.c` | RTC 驱动实现（HAL API） |
| 修改 | `Core/Src/main.c` | USER CODE 区域添加 MyRTC.h 和 MyRTC_Init() |
| 修改 | `Core/Src/main.c` | USER CODE BEGIN Check_RTC_BKUP 添加备份寄存器检查 |
| 配置 | `HAL.ioc` | CubeMX 配置 LSE + RTC |
| 生成 | `Core/Src/stm32f1xx_hal_msp.c` | CubeMX 生成 HAL_RTC_MspInit() |
| 配置 | `MDK-ARM/HAL.uvprojx` | 编译器切换为 ARM Compiler 5 |

### 关键 API 对照表

| 标准库 | HAL 库 | 说明 |
|--------|--------|------|
| `BKP_ReadBackupRegister(BKP_DR1)` | `HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1)` | 读取备份寄存器 |
| `BKP_WriteBackupRegister(BKP_DR1, val)` | `HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, val)` | 写入备份寄存器 |
| `RTC_SetCounter(cnt)` | `HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN)` | 设置时间 |
| `RTC_GetCounter()` | `HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN)` | 获取时间 |
| `RTC_SetPrescaler(32767)` | `RTC_InitTypeDef.AsynchPrediv = 32767` | 预分频器（CubeMX 配置） |
| `PWR_BackupAccessCmd(ENABLE)` | `HAL_PWR_EnableBkUpAccess()` | 备份域访问（CubeMX 生成） |

### 备注

- HAL 库的 RTC 使用 Time + Date 结构体，不再支持秒计数器模式
- 年份范围：HAL 为 0-99（对应 2000-2099），需在 MyRTC.c 中做 +2000/-2000 转换
- CubeMX 配置 LSE 后，即使主电源掉电，RTC 仍可由备用电池供电保持走时

---

## 问题四：menu.c 编译报错 — missing closing quote（编码问题复发）

### 报错现象

编译 `menu.c` 时出现 `missing closing quote` 错误，集中在秒表页面 `Show_StopClock_UI()` 函数中的中文字符串字面量（第 475~476 行），共 3 个错误：

```
..\Core\Src\Hardware\menu.c(475): error:  #8: missing closing quote
    OLED_ShowString(
TOPCLK_BTN_START_X, 
TOPCLK_BTN_Y, "寮€濮?", OLED_8X16);
..\Core\Src\Hardware\menu.c(476): error:  #165: too few arguments in function call
    OLED_ShowString(STOPCLK_BTN_STOP_X, STOPCLK_BTN_Y, "鍋滄", OLED_8X16);
..\Core\Src\Hardware\menu.c(476): error:  #18: expected a ")"
    OLED_ShowString(STOPCLK_BTN_STOP_X, STOPCLK_BTN_Y, "鍋滄", OLED_8X16);
..\Core\Src\Hardware\menu.c: 0 warnings, 30 errors
```

> **与问题一的区别：** 问题一是 `OLED_Data.c`（30 个错误），本次是 `menu.c`（3 个错误）。根因相同，但涉及不同的文件和修复步骤。

### 原因分析

**根本原因与问题一相同：源文件编码与 Keil 编译器预期的编码不匹配。**

问题一修复时，仅对 `OLED_Data.c` 进行了编码转换，但 `menu.c` 仍保持 UTF-8 编码。当 ARMCC V5.06 以 GBK 编码读取 `menu.c` 时，UTF-8 的中文字节被错误解析。

**为什么部分中文不报错？**

`menu.c` 中有多处中文字符串（如第 97 行 `"菜单"`、第 98 行 `"设置"`），但编译器仅在第 475~476 行报错。原因是某些 UTF-8 字节序列恰好构成有效的 GBK 双字节对，虽然显示为乱码，但不会破坏字符串解析：

| 汉字 | UTF-8 字节 | GBK 解析 | 编译结果 |
|------|-----------|---------|---------|
| `菜单` | `E8 8F 9C E5 8D 95` | `E88F` + `9CE5` + `8D95`（均为有效 GBK 双字节） | ✅ 乱码但不报错 |
| `开始` | `E5 BC 80 E5 A7 8B` | `E5BC` + `80`（单字节 €）+ `E5A7` + `8B`（悬挂） | ❌ 引号断裂 |
| `停止` | `E5 81 9C E6 AD A2` | `E581` + `9CE6` + `ADA2`（部分有效） | ❌ 参数数量错乱 |

**关键区别：** `菜单` 的 UTF-8 字节恰好两两配对形成有效 GBK，而 `开始` 的第二个字节 `80` 在 GBK 中是单字节字符（€ 符号），打断了双字节解析流程。

### 解决方案

由于问题一已添加 `--no-multibyte-chars` 编译器标志，本次只需将 `menu.c` 从 UTF-8 转换为 GBK 编码。

#### 使用 PowerShell 转换编码

```powershell
$path = "SmartWatch_HAL\HAL\Core\Src\Hardware\menu.c"
$content = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
$gbk = [System.Text.Encoding]::GetEncoding("GBK")
[System.IO.File]::WriteAllText($path, $content, $gbk)
```

#### 验证转换结果

转换后可检查关键行的字节是否匹配 GBK 编码：

```powershell
$bytes = [System.IO.File]::ReadAllBytes($path)
$gbk = [System.Text.Encoding]::GetEncoding("GBK")

# "开始" 应为 BF AA CA BC
# "停止" 应为 CD A3 D6 B9
# "清除" 应为 C7 E5 B3 FD
# "菜单" 应为 B2 CB B5 A5
# "设置" 应为 C9 E8 D6 C3

Write-Output "'开始' GBK: $(($gbk.GetBytes('开始') | ForEach-Object { '{0:X2}' -f $_ }) -join ' ')"
```

#### 编译验证

转换后重新编译，确认 0 Error(s)：

```
Build target 'HAL'
compiling menu.c...
linking...
"HAL\HAL.axf" - 0 Error(s), 0 Warning(s).
```

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c` | 文件编码从 UTF-8 转为 GBK |

### 经验总结

**HAL 工程中所有包含中文字符串字面量的 `.c` 文件都必须使用 GBK 编码。** 在问题一和问题四之后，需排查所有 UTF-8 源文件：

| 文件 | 编码 | 中文字符串 | 状态 |
|------|------|-----------|------|
| `OLED_Data.c` | GBK | 是（中文字符数组） | ✅ 已修复（问题一） |
| `menu.c` | GBK | 是（UI 文本） | ✅ 已修复（问题四→问题五补充） |
| `SetTime.c` | GBK | 是（UI 文本） | ✅ 已修复（问题五） |
| `OLED.c` | GBK | 仅注释含中文（无字符串） | ✅ 已统一（问题五编码统一） |
| `OLED_Data.h` | GBK | 仅注释含中文 | ✅ 已统一（问题五编码统一） |
| `dino.c` | GBK | 仅注释含中文 | ✅ 已统一（问题五编码统一） |
| 其他 `.c/.h` 文件 | UTF-8 | 否 | ✅ 无需处理 |

> **注意：** 注释中的中文不影响编译，因为编译器在词法分析前会剥离注释。只有字符串字面量（双引号内的内容）需要正确的编码。
>
> **2026-06-10 编码统一：** 问题五修复完成后，将工程中所有含中文的源文件统一为 GBK 编码，并配置 `.vscode/settings.json` 的 `files.autoGuessEncoding: true` + `files.encoding: gbk`，确保 Keil IDE 和 VSCode 均能正确显示中文注释。Markdown 文档保持 UTF-8 编码（通过 `[markdown].files.encoding: utf8` 覆盖）。

---

## 问题五：汉字显示异常 — 字符集宏定义与文件编码双重不匹配

### 现象

首页时钟界面"菜单"和"设置"汉字显示异常（显示为默认的方框问号图形），设置项目中所有汉字均显示不正常。

### 报错信息

编译阶段无报错（0 Error, 0 Warning），问题出现在运行时。屏幕上所有汉字均被替换为字库末尾的默认图形（方框内问号）。

### 原因分析

**根本原因有两层：`OLED_Data.h` 字符集宏定义错误 + `menu.c`/`SetTime.c` 文件编码未转换。**

#### 第一轮排查：宏定义不匹配

| 事项 | 问题一修复后的状态 | 应有状态 |
|------|-------------------|---------|
| `OLED_Data.c` 文件编码 | GB2312（GBK） ✅ | — |
| `OLED_Data.h` 宏定义 | `OLED_CHARSET_UTF8` ❌ | `OLED_CHARSET_GB2312` |
| `menu.c` 文件编码 | **仍为 UTF-8** ❌ | GBK |

修改宏定义（`OLED_CHARSET_UTF8` → `OLED_CHARSET_GB2312`）后问题仍然存在，深入排查发现 **问题四记录的 `menu.c` 编码转换并未真正执行**，且 `SetTime.c` 同样未转换。

#### 第二轮排查：源文件编码仍为 UTF-8

实际文件编码状态：

| 文件 | 记录状态（问题四文档） | 实际状态 | 包含中文字符串 |
|------|----------------------|---------|-------------|
| `menu.c` | GBK ✅ | **UTF-8 (with BOM)** ❌ | 是："菜单"、"设置"、"开始"、"停止"、"清除"、"日期时间设置" |
| `SetTime.c` | 未提及 | **UTF-8** ❌ | 是："年"、"月"、"日"、"时"、"分"、"秒" |
| `OLED_Data.c` | GBK | ISO-8859 (GBK raw bytes) ✅ | 是（字库索引） |
| `OLED.c` | UTF-8 | UTF-8 ✅ | 否（仅注释含中文） |

#### 匹配失败机制

以 `menu.c` 中 `"菜单"` 为例，完整的数据流：

```
menu.c 源文件（UTF-8）:  "菜单" = E8 8F 9C E5 8D 95  (6 bytes)
        │
        ▼ ARMCC 5 + --no-multibyte-chars
        │ 编译器按单字节序列原样存储，不做多字节转换
        ▼
运行时内存中的字符串:    E8 8F 9C E5 8D 95  (UTF-8 原样)
        │
        ▼ OLED_ShowString() GB2312 模式
        │ bit7=1 → 取2字节: E8 8F, 9C E5, 8D 95
        │ SingleChar = {E8, 8F, 00}, {9C, E5, 00}, {8D, 95, 00}
        │
        ▼ strcmp 匹配字库
字库索引（GB2312）:      B2 CB (菜), B5 A5 (单)
解析结果（UTF-8乱码）:   E8 8F, 9C E5, 8D 95
                         ↑ strcmp ≠ 0，永远无法匹配
        │
        ▼ 遍历到字库末尾 → pIndex 指向默认图形
        显示：方框内问号 □?
```

#### OLED_Data.h 宏的全局影响

宏 `OLED_CHARSET_xxx` 在编译期同时影响两个地方：

| 影响点 | `OLED_CHARSET_UTF8` | `OLED_CHARSET_GB2312` |
|--------|---------------------|-----------------------|
| `ChineseCell_t.Index` 大小 | `char[5]` | `char[3]` |
| `OLED_ShowString()` 解析逻辑 | 按 UTF-8 首字节前缀解析（1~4字节） | 按 bit7 判断（0→ASCII, 1→2字节汉字） |

**`--no-multibyte-chars` 的作用：** 此编译器标志告诉 ARMCC 5 将源文件中的所有多字节字符当作单字节序列处理。这意味着：

- UTF-8 源文件中的中文字符串 **不会被编译器转码**
- 字符串字面量在二进制中保持 UTF-8 的原始字节
- 运行时 `OLED_ShowString()` 接收到的是 UTF-8 字节流

### 影响范围

所有调用 `OLED_ShowString()` 或 `OLED_Printf()` 显示汉字的源文件均受影响：

| 文件 | 涉及汉字 | 调用方式 |
|------|---------|---------|
| `menu.c` | "菜单"、"设置"、"日期时间设置"、"开始"、"停止"、"清除" | `OLED_ShowString()`, `OLED_Printf()` |
| `SetTime.c` | "年"、"月"、"日"、"时"、"分"、"秒" | `OLED_Printf()` |

### 解决方案

**需要同时修改三处：**

#### 1. 修改 OLED_Data.h 宏定义

```c
// 修改前
#define OLED_CHARSET_UTF8           //定义字符集为UTF8
//#define OLED_CHARSET_GB2312       //定义字符集为GB2312

// 修改后
//#define OLED_CHARSET_UTF8         //定义字符集为UTF8
#define OLED_CHARSET_GB2312         //定义字符集为GB2312
```

#### 2. 将 menu.c 转换为 GBK 编码

```powershell
$path = "SmartWatch_HAL\HAL\Core\Src\Hardware\menu.c"
$content = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
$gbk = [System.Text.Encoding]::GetEncoding("GBK")
[System.IO.File]::WriteAllText($path, $content, $gbk)
```

#### 3. 将 SetTime.c 转换为 GBK 编码

```powershell
$path = "SmartWatch_HAL\HAL\Core\Src\Hardware\SetTime.c"
$content = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
$gbk = [System.Text.Encoding]::GetEncoding("GBK")
[System.IO.File]::WriteAllText($path, $content, $gbk)
```

#### 修复原理

修复后数据流：

```
menu.c 源文件（GBK）:    "菜单" = B2 CB B5 A5  (4 bytes)
        │
        ▼ ARMCC 5 + --no-multibyte-chars
        │ 按单字节序列原样存储
        ▼
运行时内存中的字符串:    B2 CB B5 A5  (GBK 原样)
        │
        ▼ OLED_ShowString() GB2312 模式
        │ bit7=1 → 取2字节: B2 CB, B5 A5
        │ SingleChar = {B2, CB, 00}, {B5, A5, 00}
        │
        ▼ strcmp 匹配字库
字库索引（GB2312）:      B2 CB (菜) ✓, B5 A5 (单) ✓
                         strcmp = 0，匹配成功！
        │
        ▼ 显示字模数据
        显示：正常的"菜单"汉字
```

#### 字库完整性验证

修复前已验证字库 `OLED_CF16x16[]` 包含所有需要显示的汉字：

| 字符 | GB2312 编码 | 字库索引位置 |
|------|------------|------------|
| 菜 | B2 CB | [6] |
| 单 | B5 A5 | [7] |
| 设 | C9 E8 | [8] |
| 置 | D6 C3 | [9] |
| 日 | C8 D5 | [10], [16]（重复） |
| 期 | C6 DA | [11] |
| 时 | CA B1 | [12], [17]（重复） |
| 间 | BC E4 | [13] |
| 年 | C4 EA | [14] |
| 月 | D4 C2 | [15] |
| 分 | B7 D6 | [18] |
| 秒 | C3 EB | [19] |
| 开 | BF AA | [20] |
| 始 | CA BC | [21] |
| 停 | CD A3 | [22] |
| 止 | D6 B9 | [23] |
| 清 | C7 E5 | [24] |
| 除 | B3 FD | [25] |

> 字库数据完整，所有需要显示的汉字均有对应字模。问题纯粹是编码不匹配导致的 `strcmp` 匹配失败。

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `SmartWatch_HAL/HAL/Core/Inc/Hardware/OLED_Data.h` | 第 8~9 行：注释 `OLED_CHARSET_UTF8`，启用 `OLED_CHARSET_GB2312` |
| `SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c` | 文件编码从 UTF-8 (with BOM) 转为 GBK |
| `SmartWatch_HAL/HAL/Core/Src/Hardware/SetTime.c` | 文件编码从 UTF-8 转为 GBK |

### 与问题一、问题四的关系

四个问题构成完整的编码修复链条：

```
问题一：OLED_Data.c 编译报错
  ├─ 根因：UTF-8 文件被 ARMCC 按 GBK 解析
  ├─ 修复：转换 OLED_Data.c 编码 + 添加 --no-multibyte-chars
  └─ 遗留①：未同步修改 OLED_Data.h 宏定义  ← 埋下问题五 Part A

问题四：menu.c 编译报错
  ├─ 根因：同问题一，menu.c 仍是 UTF-8
  ├─ 修复（文档记录）：转换 menu.c 编码为 GBK
  ├─ 实际状态：转换未执行，文件仍为 UTF-8 ← 埋下问题五 Part B
  └─ 遗漏：SetTime.c 同样包含中文字符串但未被发现

问题五：汉字显示异常（本问题）
  ├─ 根因 Part A：OLED_CHARSET_UTF8 宏与 GBK 编码文件不匹配
  │   └─ 修复：切换宏定义为 OLED_CHARSET_GB2312
  ├─ 根因 Part B：menu.c + SetTime.c 仍为 UTF-8 编码
  │   └─ 修复：转换两个文件为 GBK 编码
  └─ 结果：汉字正常显示 ✅
```

### 经验总结

1. **编码修复是系统工程，必须闭环验证。** 问题一修复了 `OLED_Data.c`，问题四"记录"了 `menu.c` 的修复但实际上未执行。编码问题不能仅凭文档记录判断"已修复"，必须用工具验证文件的实际字节。

2. **`--no-multibyte-chars` 是把双刃剑。** 它解决了 UTF-8 源文件在 GBK locale 编译器下的词法解析问题，但也意味着字符串字面量的最终二进制编码**完全取决于源文件的物理编码**。如果源文件是 UTF-8，字符串就是 UTF-8；如果源文件是 GBK，字符串就是 GBK。

3. **排查方法：** 使用 `file` 命令（Linux/macOS）或检查 BOM 字节来判断文件编码，使用 `xxd` 验证关键中文字符串的字节是否与字库索引一致。

| 检查项 | 问题一修复后 | 问题四文档记录 | 问题五修复后 |
|--------|------------|-------------|------------|
| `OLED_Data.c` 编译通过 | ✅ | ✅ | ✅ |
| `menu.c` 编译通过 | ❌ | ✅ | ✅ |
| `SetTime.c` 编译通过 | ✅ | ✅ | ✅ |
| `OLED_Data.h` 宏定义 | ❌ UTF8 | ❌ UTF8 | ✅ GB2312 |
| `menu.c` 文件编码 | UTF-8 ❌ | GBK（文档记录，实际未改）❌ | GBK ✅ |
| `SetTime.c` 文件编码 | UTF-8 ❌ | UTF-8 ❌ | GBK ✅ |
| `strcmp` 匹配结果 | ❌ 永远失败 | ❌ 永远失败 | ✅ 正常匹配 |
| 汉字显示效果 | ❌ 方框问号 | ❌ 方框问号 | ✅ 正常显示 |


---

## 问题六：菜单滑动动画缓慢 — 三重优化 + 时钟频率配置缺失

### 现象

用户反馈手表菜单滑动动画"像在看慢动作"。菜单从按下按键到滑动完成需要约 **1 秒**，动画帧率明显低于正常水平。

### 调查过程

#### 1. 代码审查发现的 4 个问题

**问题 A：`Menu_Animation()` 每帧被重复调用，且无条件调用导致 APP 返回可能黑屏**

`Menu_Page()` 主循环中直接调用了 `Menu_Animation()`，紧接着的条件分支中 `Set_Selection()` 内部再次调用了 `Menu_Animation()`，导致每帧实际执行了两次完整的清屏 + 绘制 + I2C 传输。

同时，无条件调用虽然兜底了无按键场景，但当 `Direct_Flag = 0`（APP 返回后）时不进任何分支，如果简单移除就会黑屏。

**问题 B：`OLED_Update()` 全屏刷新，软件 I2C 传输量大**

`Menu_Animation()` 使用 `OLED_Update()` 传输全部 1024 字节（8 页 × 128 字节）到 OLED。软件 I2C (bit-banged) 下每字节需约 26 次 GPIO 操作，一次全屏刷新耗时显著。

**问题 C：`MENU_SLIDE_STEP = 4`，帧数过多**

菜单图标间距 `MENU_ICON_SPACING = 48`，步长仅 `4 px/帧`，完整滑动需要 48/4 = **12 帧**。

**问题 D：系统时钟仅运行在 8MHz HSI（关键根因）**

通过分析 `main.c` 的 `SystemClock_Config()` 发现：

```c
// main.c（修改前）
RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;   // PLL 被禁用！
// ...
RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;  // 系统时钟 = HSI = 8MHz
```

CubeMX 默认生成的时钟配置未使能 PLL，系统实际运行在 HSI 8MHz，仅为预期 72MHz 的 **1/9**。

#### 2. 时钟频率对各项性能的影响

| 指标 | 72MHz（预期） | 8MHz（实际） | 降幅 |
|------|-------------|-------------|------|
| CPU 性能 | 100% | **11%** | **9×** |
| 软件 I2C 传输 1024 字节 | ~3 ms | **~27 ms** | **9×** |
| `OLED_Clear()` 清显存 | ~0.02 ms | **~0.18 ms** | **9×** |
| TIM2 中断周期 | **1 ms** | **9 ms** | **9×** |
| 按键扫描响应 | ~1 ms | **~9 ms** | **9×** |
| `__WFI()` 唤醒周期 | ~1 ms | **~9 ms** | **9×** |

TIM2 定时器参数（`Prescaler = 719`, `Period = 99`）是为 72MHz 设计的：

```
72MHz:  72,000,000 / (719+1) / (99+1) = 1000 Hz = 1ms
8MHz:    8,000,000 / (719+1) / (99+1) =  111 Hz = 9ms
```

### 动画性能对比

#### 修复前（8MHz HSI，双重调用，步长 4）

| 阶段 | 单帧耗时 | 帧数 | 总耗时 |
|------|---------|------|--------|
| `Menu_Animation()` × 2 次 | ~54 ms × 2 | — | ~108 ms |
| × 12 帧完整滑动 (step=4) | — | 12 | **~1,296 ms** |

#### 优化后（72MHz PLL，单次调用，步长 8 + 局部刷新）

| 阶段 | 单帧耗时 | 帧数 | 总耗时 |
|------|---------|------|--------|
| `Menu_Animation()` × 1 次 | ~6 ms | — | ~6 ms |
| × 6 帧完整滑动 (step=8) | — | 6 | **~36 ms** |

**性能提升：约 36 倍**

#### 各优化项贡献分解

| 优化项 | 优化前 | 优化后 | 单项提速 | 累积提速 |
|--------|-------|-------|---------|---------|
| 修复 A：去重（else 兜底） | 2 次/帧 | 1 次/帧 | **2×** | 2× |
| 修复 D：时钟 8→72MHz | 8 MHz | 72 MHz | **9×** | 18× |
| 修复 C：步长 4→8 | 12 帧 | 6 帧 | **2×** | 36× |
| 修复 B：局部刷新 | 1024 字节 | 768 字节 | **1.3×** | ~47× |

> **说明：** 局部刷新的加速效果在 72MHz 下更显著，因为此时 I2C 传输占比更大。8MHz 下 CPU 操作（清显存、绘图）占主导，I2C 传输时间被 CPU 慢速掩盖。

### 解决方案

#### 修复 A：重复调用改为 else 兜底方案（防黑屏）

直接移除 `Menu_Animation()` 会导致从小应用返回菜单时出现黑屏（`Direct_Flag = 0` 时 `Set_Selection()` 不执行，菜单不会被绘制）。

改为 else 兜底方案：有按键时通过 `Set_Selection()` 触发带动画的绘制，无按键时直接调用 `Menu_Animation()` 仅重绘不动画。

```c
// Menu_Page() 中（修改前）
Menu_Animation();       // 无条件调用（会导致双次调用）
if(MenuFlag==1) {
    if(Direct_Flag == 1)      { Set_Selection(move_stateFlag,1,0); }
    else if(Direct_Flag == 2) { Set_Selection(move_stateFlag,0,0); }
} else {
    if(Direct_Flag == 1)      { Set_Selection(move_stateFlag,MenuFlag,MenuFlag-1); }
    else if(Direct_Flag == 2) { Set_Selection(move_stateFlag,MenuFlag-2,MenuFlag-1); }
}

// 修改后：else 兜底，三种情况各调用一次 Menu_Animation()
if(MenuFlag==1) {
    if(Direct_Flag == 1)      { Set_Selection(move_stateFlag,1,0); }
    else if(Direct_Flag == 2) { Set_Selection(move_stateFlag,0,0); }
    else                      { Menu_Animation(); }  // 无按键，仅重绘
} else {
    if(Direct_Flag == 1)      { Set_Selection(move_stateFlag,MenuFlag,MenuFlag-1); }
    else if(Direct_Flag == 2) { Set_Selection(move_stateFlag,MenuFlag-2,MenuFlag-1); }
    else                      { Menu_Animation(); }  // 无按键，仅重绘
}
```

| `Direct_Flag` | 执行路径 | 效果 |
|---|---|---|
| 1（上键） | `Set_Selection()` → `Menu_Animation()` | 动画 1 次 |
| 2（下键） | `Set_Selection()` → `Menu_Animation()` | 动画 1 次 |
| 0（无按键/APP返回） | `Menu_Animation()` | 仅重绘，无动画 |

三种情况各只调用一次 `Menu_Animation()`，避免重复 I2C 传输的同时确保从小应用返回不会黑屏。

#### 修复 B：使用局部刷新代替全屏刷新

```c
// Menu_Animation() 中（修改前）
OLED_Clear();                     // 清全屏
// ...绘制 5 个图标...
OLED_Update();                    // 全屏 I2C 发送 1024 字节

// 修改后
OLED_ClearArea(0, MENU_FRAME_Y - 2, 128, 48);  // 只清菜单区域
// ...绘制 5 个图标（不变）...
OLED_UpdateArea(0, MENU_FRAME_Y - 2, 128, 48); // 只发送 768 字节
```

同理修改 `MenuToFunction_Animation()`：

```c
// 修改前
OLED_Clear();                     // 清全屏
// ...绘图...
OLED_Update();                    // 全屏发送

// 修改后
OLED_ClearArea(0, MENU_ICON_Y, 128, 48);     // 只清图标区域
// ...绘图（不变）...
OLED_UpdateArea(0, MENU_ICON_Y, 128, 48);    // 只发送图标区域
```

**局部刷新的区域选择依据：**

```
Y 坐标      页     内容
─────────────────────────────────
 0 ~  7    页0    空白（未使用）
 8 ~ 15    页1    选择框上缘 (Frame Y=10~15)
16 ~ 23    页2    图标区域 (Icon Y=16~47)
24 ~ 31    页3    ┐
32 ~ 39    页4    │ 菜单图标
40 ~ 47    页5    ┘
48 ~ 55    页6    选择框下缘
56 ~ 63    页7    空白（未使用）
```

菜单动画实际涉及 Y=8~55 共 6 页（768 字节），页 0 和页 7 始终为空白，无需刷新。

#### 修复 C：增大滑动步长

```c
// menu.h（修改前）
#define MENU_SLIDE_STEP         4       /* 菜单滑动步长(px/帧) */

// 修改后
#define MENU_SLIDE_STEP         8       /* 菜单滑动步长(px/帧) */
```

步长从 4 增加到 8，帧数从 12 减少到 6，动画时间减半。

#### 修复 D：通过 CubeMX 配置 PLL 使系统时钟恢复 72MHz

```c
// main.c（修改前）
RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;

// 修改后（CubeMX 自动生成）
RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
// FLASH_LATENCY 从 0 改为 2
```

### 效果对比总结

| 场景 | 动画时长 | 相对速度 |
|------|---------|---------|
| 🐢 修复前（8MHz + 双重调用 + 步长4） | **~1,296 ms** | 1× |
| 🚶 仅修复时钟（72MHz） | ~144 ms | 9× |
| 🏃 修复 A + B + C（8MHz） | ~90 ms | 14× |
| 🚀 **三项全优化 + 时钟修复** | **~36 ms** | **36×** |

### CubeMX 时钟树配置

#### 硬件时钟拓扑

```
                        ┌──────────┐
                        │   HSE    │
                        │  8 MHz   │
                        └────┬─────┘
                             │
                             ▼
                        ┌──────────┐     ┌─────────────────┐
                        │ PLLSRC   │────▶│  PLLMUL         │
                        │ (选择HSE) │     │  ×9 (8×9=72MHz) │
                        └──────────┘     └────────┬────────┘
                                                  │
                                                  ▼
              ┌─────────────────────────────────────────────┐
              │                SYSCLK                       │
              │         选择 PLLCLK  =  72 MHz              │
              └────┬──────────────┬───────────────┬─────────┘
                   │              │               │
                   ▼              ▼               ▼
            ┌──────────┐  ┌──────────┐  ┌──────────────┐
            │  AHB     │  │  APB1   │  │  APB2        │
            │ Prescaler│  │Prescaler│  │ Prescaler    │
            │  /1      │  │  /2     │  │  /1          │
            │  72 MHz  │  │  36 MHz │  │  72 MHz      │
            └──────────┘  └──────────┘  └──────────────┘
```

#### 配置参数表

| 时钟路径 | 配置项 | 设置值 | 说明 |
|---------|--------|--------|------|
| HSE | 使能 | ✅ 勾选 Crystal/Ceramic Resonator | 使用外部 8MHz 晶振 |
| LSE | 使能 | ✅ 勾选 | 32.768kHz，RTC 用（已配好） |
| PLL Source | PLL Source MUX | **HSE** | 选择 HSE 作为 PLL 输入 |
| PLL Mul | PLL Multiplier | **×9** | 8MHz × 9 = **72MHz** |
| SYSCLK | System Clock MUX | **PLLCLK** | 系统时钟选择 PLL 输出 |
| AHB Prescaler | — | **/1** | HCLK = 72MHz |
| APB1 Prescaler | — | **/2** | APB1 = 36MHz（TIM 挂在此总线） |
| APB2 Prescaler | — | **/1** | APB2 = 72MHz（ADC/GPIO 挂在此总线） |

#### CubeMX 操作步骤

1. 打开 `HAL.ioc` 文件（CubeMX 项目配置）
2. 进入 **Pinout & Configuration → Clock Configuration** 标签页
3. 在 **HSE** 旁的下拉框选择 **Crystal/Ceramic Resonator**
4. 在 **PLL Source** 的 MUX 中选择 **HSE**
5. 在 **PLL Mul** 下拉选择 **×9**
6. 在 **System Clock MUX** 中选择 **PLLCLK**
7. 调整 **APB1 Prescaler** 为 **/2**（让 APB1 = 36MHz）
8. 确认右下角显示 **72 MHz**（HCLK）无红色警告
9. 点击 **Generate Code** 重新生成代码

#### 配置前后时序验证

```
时钟恢复后：
HCLK  = 72 MHz  (AHB)
APB1  = 36 MHz  → TIM2 时钟 = 36 MHz × 2(倍频) = 72 MHz
APB2  = 72 MHz  → GPIO / ADC 时钟 = 72 MHz

TIM2 中断周期（参数不变）：
72MHz / (719+1) / (99+1) = 1000 Hz = 1ms  ✅ 恢复正确
```

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c` | `Menu_Animation()`：`OLED_Clear()` → `OLED_ClearArea()` |
| `SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c` | `Menu_Animation()`：`OLED_Update()` → `OLED_UpdateArea()` |
| `SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c` | `MenuToFunction_Animation()`：`OLED_Clear()` → `OLED_ClearArea()` |
| `SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c` | `MenuToFunction_Animation()`：`OLED_Update()` → `OLED_UpdateArea()` |
| `SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c` | `Menu_Page()`：无条件调用改为 else 兜底（`Direct_Flag`=0 时仅重绘，防黑屏） |
| `SmartWatch_HAL/HAL/Core/Inc/Hardware/menu.h` | `MENU_SLIDE_STEP` 从 4 改为 8 |
| `SmartWatch_HAL/HAL/Core/Src/main.c` | PLL 配置：HSI → HSE+PLL，SYSCLK = 72MHz |
| `SmartWatch_HAL/HAL/HAL.ioc` | CubeMX 时钟配置同步更新 |
| `SmartWatch_HAL/HAL/Drivers/` | HAL 驱动文件由 CubeMX 自动重生成 |
| `SmartWatch_HAL/HAL/MDK-ARM/HAL.uvprojx` | Keil 工程路径清理 |

### 遗留待办

- [x] 通过 CubeMX 配置 PLL 时钟，使系统时钟恢复 72MHz ✅
- [x] 配置完成后验证 TIM2 中断周期是否恢复为 1ms
- [x] 整体功能回归测试：菜单滑动、秒表计时、按键响应

---

## 问题七：SetTime.c — 分钟设置函数使用错误的数组索引

### 现象

在设置页面选择"设置分钟"，UI 高亮了分钟行（显示"分:XX"），但按键操作实际修改的是**秒**的值，分钟值永远无法被修改。

### 原因分析

`Set_Min()` 函数内部调用 `ChangeRTC_Time()` 时使用了错误的数组索引：

```c
// SetTime.c Set_Min() — 当前代码（错误）
int Set_Min(void)
{
    while(1)
    {
        KeyNum = Key_GetNum();
        if(KeyNum == 1)     // Key1: add Min
        {
            ChangeRTC_Time(5, 1);   // ← 索引 5 = MyRTC_Time[5] = 秒！
            ...
        }
        else if(KeyNum == 2)    // Key2: minus Min
        {
            ChangeRTC_Time(5, 0);   // ← 索引 5 = MyRTC_Time[5] = 秒！
            ...
        }
        ...
    }
}
```

`MyRTC_Time` 数组索引定义：

| 索引 | 含义 | 对应函数 |
|------|------|---------|
| 0 | 年 | `Set_Year()` |
| 1 | 月 | `Set_Month()` |
| 2 | 日 | `Set_Day()` |
| 3 | 时 | `Set_Hour()` |
| **4** | **分** | **`Set_Min()`** ← 应该操作此索引 |
| 5 | 秒 | `Set_Sec()` |

`Set_Min()` 应该使用索引 `4`（分），实际使用了索引 `5`（秒），导致 `Set_Min()` 和 `Set_Sec()` 都操作同一个 `MyRTC_Time[5]`。

此外，`Set_Min()` 的 UI 高亮位置也与分钟 UI 不匹配——`Show_SetTime_UI()` 中分钟的 Y 坐标为 16（"分:XX"），但 `Set_Min()` 中 `OLED_ReverseArea(24, 32, 16, 16)` 反显的是 Y=32 的区域（秒的位置），这导致高亮光标出现在秒行而非分钟行。

### 解决方案

#### 1. 修正数组索引

```c
// SetTime.c Set_Min() — 修改后
int Set_Min(void)
{
    while(1)
    {
        KeyNum = Key_GetNum();
        if(KeyNum == 1)     // Key1: add Min
        {
            ChangeRTC_Time(4, 1);   // ← 修正：索引 4 = 分
            if(MyRTC_Time[4] >= 60)
            {
                MyRTC_Time[4] = 0;
                MyRTC_SetTime();
            }
        }
        else if(KeyNum == 2)    // Key2: minus Min
        {
            ChangeRTC_Time(4, 0);   // ← 修正：索引 4 = 分
            if(MyRTC_Time[4] < 0)
            {
                MyRTC_Time[4] = 59;
                MyRTC_SetTime();
            }
        }
        ...
    }
}
```

#### 2. 修正 UI 高亮位置

```c
// 修改前
OLED_ReverseArea(24, 32, 16, 16);   // 反显 Y=32（秒行）

// 修改后
OLED_ReverseArea(24, 16, 16, 16);   // 反显 Y=16（分行）
```

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `SmartWatch_HAL/HAL/Core/Src/Hardware/SetTime.c` | `Set_Min()`：`ChangeRTC_Time` 索引从 `5` 改为 `4`，添加边界检查，修正 UI 高亮 Y 坐标 |

---

## 问题八：SetTime.c — Set_Hour() 小时边界值错误

### 现象

设置小时时，可以将小时值调到 **24**（屏幕显示"时:24"），这是无效的小时值。同时向下调小时时，最小值回绕到 24 而非 23。

### 原因分析

`Set_Hour()` 的上边界检查和下边界回绕值均有 off-by-one 错误：

```c
// SetTime.c Set_Hour() — 当前代码（错误）
if(MyRTC_Time[3] >= 25)     // 允许 MyRTC_Time[3] = 24
{
    MyRTC_Time[3] = 0;
}
...
if(MyRTC_Time[3] < 0)
{
    MyRTC_Time[3] = 24;     // 回绕值应为 23
}
```

小时的有效范围是 0~23：
- 上边界检查应为 `>= 24`（当值为 24 时回绕到 0），而非 `>= 25`
- 下边界回绕值应为 `23`（最小值 -1 应回绕到最大值），而非 `24`

### 解决方案

```c
// SetTime.c Set_Hour() — 修改后
ChangeRTC_Time(3, 1);
if(MyRTC_Time[3] >= 24)     // ← 修正：>= 24
{
    MyRTC_Time[3] = 0;
    MyRTC_SetTime();
}

// ...

ChangeRTC_Time(3, 0);
if(MyRTC_Time[3] < 0)
{
    MyRTC_Time[3] = 23;     // ← 修正：回绕到 23
    MyRTC_SetTime();
}
```

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `SmartWatch_HAL/HAL/Core/Src/Hardware/SetTime.c` | `Set_Hour()`：上边界 `>= 25` → `>= 24`，下边界回绕 `24` → `23` |

---

## 问题九：menu.c — Battery_ShowUI() 先显示后边界限制导致低电量乱码

### 现象

当电池电压很低时（ADC 值 < 3276），电量百分比在屏幕上显示为乱码（一个巨大的无符号数字），而非 "0%"。

### 原因分析

`Battery_Show_UI()` 函数中，`OLED_ShowNum()` 在 `Battery_Capacity` 的边界检查**之前**被调用：

```c
// menu.c Battery_Show_UI() — 当前代码（错误顺序）
Battery_Capacity = (AD_Value - BATTERY_ADC_EMPTY) * 100
                 / (BATTERY_ADC_MAX - BATTERY_ADC_EMPTY);

OLED_ShowNum(82, 4, Battery_Capacity, 3, OLED_6X8);  // 先显示！
OLED_ShowChar(100, 4, '%', OLED_6X8);

if(Battery_Capacity < 0)     // 后检查！
{
    Battery_Capacity = 0;
}
```

当 `AD_Value < 3276` 时，`Battery_Capacity`（`int8_t`，范围 -128~127）为负数。`OLED_ShowNum()` 的第三个参数类型是 `uint32_t`，负数被符号扩展为一个巨大的无符号数（如 -1 → 0xFFFFFFFF = 4294967295），导致 `OLED_ShowNum` 尝试显示一个超大数字，屏幕上出现乱码。

**数据流示例（AD_Value = 3000）：**

```
Battery_Capacity = (3000 - 3276) * 100 / (4092 - 3276)
                 = (-276) * 100 / 816
                 = -33  (int8_t)

OLED_ShowNum(82, 4, (uint32_t)(-33), 3, OLED_6X8)
  → Number = 0xFFFFFFDF = 4294967263
  → OLED 显示 "4294967263" → 屏幕乱码！
```

### 解决方案

将边界检查移到显示之前：

```c
// menu.c Battery_Show_UI() — 修改后
Battery_Capacity = (AD_Value - BATTERY_ADC_EMPTY) * 100
                 / (BATTERY_ADC_MAX - BATTERY_ADC_EMPTY);

// 先做边界限制
if(Battery_Capacity < 0)
{
    Battery_Capacity = 0;
}
if(Battery_Capacity >= 100)
{
    Battery_Capacity = 100;
}

// 再显示
OLED_ShowNum(82, 4, Battery_Capacity, 3, OLED_6X8);
OLED_ShowChar(100, 4, '%', OLED_6X8);
```

同时优化后续的电量图标绘制逻辑（原来在 `>= 100` 和 `>= 10` 分支中有冗余的 `OLED_ShowNum` 调用，修正边界后这些调用可以被统一）。

### 修复实施（2026-06-11）

采用 Python 字节级替换方式修改（因为 `menu.c` 为 GBK 编码，中文注释的 GBK 字节序列无法通过普通文本编辑器精确匹配）。

**修改内容：**

```c
// menu.c Battery_Show_UI() — 修改后
Battery_Capacity = (AD_Value - BATTERY_ADC_EMPTY) * 100
                 / (BATTERY_ADC_MAX - BATTERY_ADC_EMPTY);

// 先做边界限制（移到 OLED_ShowNum 之前）
if(Battery_Capacity < 0)
{
    Battery_Capacity = 0;
}

if(Battery_Capacity >= 100)
{
    Battery_Capacity = 100;
}

// 再显示（此时 Battery_Capacity 已确保在 [0, 100] 范围内）
OLED_ShowNum(82, 4, Battery_Capacity, 3, OLED_6X8);
OLED_ShowChar(100, 4, '%', OLED_6X8);

// 电量图标绘制（去掉冗余的 OLED_ShowNum 调用和冗余条件）
if(Battery_Capacity >= 100)
{
    OLED_ShowImage(...);
}
else if(Battery_Capacity >= 10)  // 去掉 && Battery_Capacity < 100（已 clamp）
{
    ...
}
else  // 个位数电量不显示
{
    ...
}
```

**修复要点：**

| 修改项 | 修改前 | 修改后 |
|--------|--------|--------|
| 边界检查位置 | `OLED_ShowNum()` **之后** | `OLED_ShowNum()` **之前** |
| `>= 100` 分支冗余 `OLED_ShowNum` | 有（重复调用） | 移除 |
| `>= 10` 分支条件 | `>= 10 && < 100` | `>= 10`（上界已在前面 clamp） |

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `SmartWatch_HAL/HAL/Core/Src/Hardware/menu.c` | `Battery_Show_UI()`：边界检查移到 `OLED_ShowNum()` 之前，移除冗余代码 |

---

## 问题十：Key.c — Key_GetNum() 存在 ISR-主循环竞态条件

### 现象

偶尔按键无响应，按键事件似乎被"吞掉"了。问题在快速连续按键时更容易复现。

### 原因分析

`Key_Num` 是一个共享变量——在 TIM2 ISR 中由 `KeyTick()` 写入，在主循环中由 `Key_GetNum()` 读取并清零。`Key_GetNum()` 的读-改-写操作不是原子的：

```c
// Key.c Key_GetNum() — 当前代码（存在竞态窗口）
uint8_t Key_GetNum(void)
{
    uint8_t Temp;
    if(Key_Num)
    {
        Temp = Key_Num;
        // ← ⚠️ 竞态窗口：如果 ISR 在此处触发并设置 Key_Num=3
        Key_Num = 0;      // 新设置的按键值 3 被清零丢失！
        return Temp;
    }
    else
    {
        return 0;
    }
}
```

**竞态时序：**

```
时间 →
主循环:  Temp=Key_Num(=0)  │              │ Key_Num=0
        ───────────────────┤              ├─────────────
ISR:                       │ KeyTick()    │
                           │ Key_Num = 3  │
                           │              │
结果: Key_Num 被 ISR 设为 3，然后被主循环清零 → 按键事件 3 丢失
```

在 Cortex-M3 上，`Key_Num` 是 `uint8_t`，其读写是单条 LDRB/STRB 指令，但整个 `if(Key_Num) { Temp=Key_Num; Key_Num=0; }` 序列不是原子的。

### 解决方案

在读取-清零操作期间短暂禁用中断：

```c
// Key.c Key_GetNum() — 修改后
uint8_t Key_GetNum(void)
{
    uint8_t Temp;
    __disable_irq();          // 临界区开始
    if(Key_Num)
    {
        Temp = Key_Num;
        Key_Num = 0;
        __enable_irq();       // 临界区结束
        return Temp;
    }
    __enable_irq();           // 临界区结束
    return 0;
}
```

> **注意：** `__disable_irq()` 会屏蔽所有中断（包括 TIM2 1ms 定时器），关中断时间极短（仅 2~3 条指令），不会影响系统实时性。

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `SmartWatch_HAL/HAL/Core/Src/Hardware/Key.c` | `Key_GetNum()`：添加 `__disable_irq()` / `__enable_irq()` 保护临界区 |

---

## 问题十一：dino.c — isColliding() 函数名与行为不符，阻塞延时冻结游戏循环

### 现象

游戏结束时屏幕冻结约 1 秒，期间所有按键无响应。从代码审查角度，`isColliding()` 的函数命名暗示纯查询操作，但实际内部有严重的副作用。

### 原因分析

`isColliding()` 函数违反了单一职责原则：

```c
// dino.c isColliding() — 当前代码
uint8_t isColliding(struct Object_Position* a, struct Object_Position* b)
{
    if((a->minX < b->maxX) && ...)   // 碰撞检测（正确）
    {
        OLED_Clear();                        // 副作用 1: 清屏
        OLED_ShowString(28,24,"Game Over",OLED_8X16);  // 副作用 2: 显示
        OLED_Update();                       // 副作用 3: 刷新
        delay_ms(1000);                      // 副作用 4: 阻塞 1 秒！
        OLED_Clear();                        // 副作用 5: 清屏
        OLED_Update();                       // 副作用 6: 刷新
        return 1;
    }
    return 0;
}
```

问题：
1. **函数名误导：** 名为 `isColliding`（"是否碰撞"），但内部执行了 UI 渲染和延时
2. **阻塞延时：** `delay_ms(1000)` 在游戏主循环中阻塞 1 秒，冻结所有游戏逻辑（障碍物移动、分数更新等）
3. **`dino_tick()` 仍在 ISR 中运行：** 即使 `isColliding` 阻塞了主循环，TIM2 ISR 中的 `dino_tick()` 仍在更新分数和障碍物位置，导致 Game Over 后游戏状态在后台继续变化

### 解决方案

将碰撞检测和 Game Over 显示分离：

```c
// dino.c — 修改后

// 纯碰撞检测函数（无副作用）
uint8_t isColliding(struct Object_Position* a, struct Object_Position* b)
{
    return (a->minX < b->maxX) && (a->maxX > b->minX)
        && (a->minY < b->maxY) && (a->maxY > b->minY);
}

// Game Over 显示函数（独立）
void Show_GameOver(void)
{
    OLED_Clear();
    OLED_ShowString(28, 24, "Game Over", OLED_8X16);
    OLED_Update();
    delay_ms(1000);
    OLED_Clear();
    OLED_Update();
}

// Dino_game_Animation() 调用处修改
uint8_t Dino_game_Animation(void)
{
    while(1)
    {
        OLED_Clear();
        Show_Score();
        Show_Ground();
        Show_Barrier();
        Show_Cloud();
        Show_Dino();
        OLED_Update();

        if(isColliding(&Barr, &dino))
        {
            Show_GameOver();
            return 0;
        }
    }
}
```

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `SmartWatch_HAL/HAL/Core/Src/Hardware/dino.c` | 拆分 `isColliding()`：纯检测逻辑保留，Game Over 显示逻辑提取为独立函数 |
| `SmartWatch_HAL/HAL/Core/Inc/Hardware/dino.h` | 新增 `Show_GameOver()` 声明（可选，如仅在 dino.c 内部使用可不暴露） |

---

## 问题十二：dino.c — 恐龙碰撞边界 maxX 计算错误

### 现象

目前无可见现象（因为 `DINO_X_POS = 0` 巧合正确），但如果未来调整恐龙 X 坐标，碰撞检测会出错。

### 原因分析

`Show_Dino()` 中恐龙的碰撞边界计算错误：

```c
// dino.c Show_Dino() — 当前代码
dino.minX = DINO_X_POS;           // 0，正确
dino.maxX = DINO_WIDTH;           // 16，错误！应为 DINO_X_POS + DINO_WIDTH
dino.minY = DINO_GROUND_Y - Dino_JumpPos;
dino.maxY = DINO_GROUND_Y_END - Dino_JumpPos;
```

因为 `DINO_X_POS = 0`，`DINO_WIDTH = 16`，`DINO_X_POS + DINO_WIDTH = 0 + 16 = 16`，与当前值巧合一致。但这是脆弱的——如果未来将恐龙移到屏幕中央（如 `DINO_X_POS = 10`），`dino.maxX = 16` 而实际应为 `26`，碰撞检测将提前误判。

### 解决方案

```c
// dino.c Show_Dino() — 修改后
dino.minX = DINO_X_POS;
dino.maxX = DINO_X_POS + DINO_WIDTH;     // ← 修正
dino.minY = DINO_GROUND_Y - Dino_JumpPos;
dino.maxY = DINO_GROUND_Y_END - Dino_JumpPos;
```

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `SmartWatch_HAL/HAL/Core/Src/Hardware/dino.c` | `Show_Dino()`：`dino.maxX = DINO_WIDTH` → `DINO_X_POS + DINO_WIDTH` |

---

## 问题十三：dino.c — Dino_JumpCount 静态初始化为 1 而非 0

### 现象

目前无可见现象（`Game_Init()` 中会重置为 0），但静态初始值不正确，代码可读性差。

### 原因分析

```c
// dino.c — 当前代码
uint16_t Dino_JumpCount = 1;    // 应为 0
```

虽在 `Game_Init()` 中会被重置为 0：
```c
void Game_Init(void)
{
    Dino_Score = ... = Dino_JumpCount = 0;
}
```

但如果任何代码路径跳过 `Game_Init()` 直接进入游戏，首次跳跃的 `sin()` 计算会偏差 1ms 的相位：

```c
Dino_JumpPos = DINO_JUMP_HEIGHT * sin((float)(Pi * Dino_JumpCount / DINO_JUMP_DURATION));
// Dino_JumpCount=1 → sin(Pi*1/1000) ≈ sin(0.00314) ≈ 0.00314
// 正确值应为 sin(0) = 0，恐龙应从地面开始跳跃
```

### 解决方案

```c
// dino.c — 修改后
uint16_t Dino_JumpCount = 0;
```

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `SmartWatch_HAL/HAL/Core/Src/Hardware/dino.c` | `Dino_JumpCount` 初始值从 `1` 改为 `0` |

---

## 问题十四：OLED.c / MyI2C.c — 软件 I2C 无延时可导致通信不稳

### 现象

OLED 显示可能偶尔出现花屏或闪烁；MPU6050 数据读取可能偶尔出错。问题在特定温度或电压条件下更容易复现。

### 原因分析

软件模拟 I2C 在 SDA 建立和 SCL 翻转之间没有任何延时。代码中注释也提到需要延时但被省略：

```c
// OLED.c OLED_W_SCL() — 当前代码
void OLED_W_SCL(uint8_t BitValue)
{
    HAL_GPIO_WritePin(OLED_SCL_GPIO_Port, OLED_SCL_Pin, (GPIO_PinState)BitValue);
    /*如果芯片速度过快，可以在此处添加延时，以避免超过I2C通信的最高速度*/
    //...   ← 延时被省略
}
```

在 72MHz 主频下：
- `HAL_GPIO_WritePin()` 执行时间约 50~100ns
- 软件 I2C 的 SCL 频率可达 **数 MHz**
- SSD1306 OLED 最大 I2C 频率：**400kHz**（快速模式）
- MPU6050 最大 I2C 频率：**400kHz**

**时序分析：**

```
标准 I2C (400kHz):
SCL: ──┐     ┌──┐  ┌──     SCL 高电平 ≥ 0.6μs
       │     │  │  │
       └─────┘  └──┘
SDA: ──────┐  ┌──────       SDA 在 SCL 低电平期间变化
           │  │
           └──┘
            ↑
        data setup ≥ 100ns

72MHz 无延时软件 I2C:
SCL: ──┐ ┌──┐ ┌──             SCL 高电平 ≈ 100ns（远小于 0.6μs）
       │ │  │ │
       └─┘  └─┘
SDA: ────┐┌──────             SDA setup ≈ 0ns（违反时序）
         ││
         └┘
```

**影响范围：**

| 外设 | I2C 引脚 | 驱动文件 | 风险 |
|------|---------|---------|------|
| OLED | PB8(SCL) / PB9(SDA) | `OLED.c` | 花屏、闪烁、偶尔不刷新 |
| MPU6050 | PB10(SCL) / PB11(SDA) | `MyI2C.c`（通过 MPU6050.c 调用） | 数据读取错误、传感器初始化失败 |

### 解决方案

在 SCL 翻转和 SDA 变化之间添加微秒级延时。由于 `delay_us()` 每次调用都会开启/关闭 DWT（有开销），对于 I2C 时序中的短延时，使用简单的 NOP 循环更合适：

```c
// MyI2C.c — 添加 I2C 延时宏
#define I2C_DELAY()  delay_us(2)   // 约 2μs，确保 SCL 周期 ≥ 5μs（200kHz）

// MyI2C_SendByte() — 修改后
void MyI2C_SendByte(...)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        MyI2C_W_SDA(SDA_GPIOx, SDA_Pin, !!(Byte & (0x80 >> i)));
        I2C_DELAY();                               // ← 添加：SDA 建立时间
        MyI2C_W_SCL(SCL_GPIOx, SCL_Pin, 1);
        I2C_DELAY();                               // ← 添加：SCL 高电平保持
        MyI2C_W_SCL(SCL_GPIOx, SCL_Pin, 0);
        I2C_DELAY();                               // ← 添加：SCL 低电平保持
    }
}
```

同样在 OLED 的 `OLED_I2C_SendByte()` 中也需要添加延时。但需注意：

> ⚠️ **性能影响：** 添加 I2C 延时后，OLED 全屏刷新（1024 字节 × 约 26 次 GPIO 操作 × 每次加 2μs 延时）的耗时将从 ~3ms 增加到约 **~50ms**。这与菜单滑动动画优化（问题六）的目标存在矛盾，需要权衡。

**推荐方案：仅在 MyI2C 模块统一添加延时**（OLED 的 I2C 函数是独立实现的，也需要同步修改）。可以将 I2C 延时作为可配置参数：

```c
// MyI2C.h — 添加延时配置宏
#define MYI2C_DELAY_US          1       // I2C 半周期延时(μs)，1μs → ~500kHz

// MyI2C.c
static void MyI2C_Delay(void)
{
#if MYI2C_DELAY_US > 0
    delay_us(MYI2C_DELAY_US);
#endif
}
```

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `SmartWatch_HAL/HAL/Core/Src/MyI2C.c` | 在 SCL/SDA 翻转之间添加 `delay_us()` 调用 |
| `SmartWatch_HAL/HAL/Core/Src/Hardware/OLED.c` | `OLED_I2C_SendByte()` 中添加相同的延时 |

### 备注

- OLED 和 MPU6050 使用**不同的软件 I2C 实现**（OLED 内嵌在 `OLED.c`，MPU6050 使用 `MyI2C.c`），两处都需要修改
- 如果未来改为硬件 I2C（STM32F103 有 2 个硬件 I2C 外设），此问题自动解决
- 当前 72MHz 主频下，`delay_us(1)` 约 72 个 CPU 周期，实际延时约 1μs；`delay_us(2)` 约 2μs，SCL 频率约 250kHz，在 SSD1306 和 MPU6050 的规格范围内

---

## 问题十五：menu.c — 主循环每毫秒全屏重绘导致功耗过大

### 现象

手表电池耗电过快，待机时间明显短于预期。即使手表静止显示时钟页面（无用户操作），屏幕仍被反复刷新。

### 原因分析

`main()` 的主循环结构：

```c
// main.c — 当前代码
while (1)
{
    OLED_Clear();
    Battery_Show_UI();
    OLED_Update();                          // 全屏 I2C 发送 1024 字节
    ClockUI_Move_Flag = First_Page_Clock(); // 内部 __WFI()，被 TIM2 1ms 唤醒
    if(ClockUI_Move_Flag == 1) { Menu_Page(); }
    else if(ClockUI_Move_Flag == 2) { SettingPage(); }
}
```

`First_Page_Clock()` 内部通过 `__WFI()` 等待中断，但 TIM2 每 **1ms** 触发一次中断。每次唤醒后：

1. 回到 `while(1)` 开头
2. `OLED_Clear()` — 清零 1024 字节显存
3. `Battery_Show_UI()` — 绘制电池图标（大部分时间使用缓存值，开销小）
4. `OLED_Update()` — **软件 I2C 发送 1024 字节 → ~3ms（72MHz 下）**
5. 进入 `First_Page_Clock()` → `__WFI()` → 约 1ms 后被 TIM2 唤醒
6. 重复步骤 2

**功耗分析：**

| 操作 | 频率 | 单次耗时 | 占空比 |
|------|------|---------|--------|
| OLED 全屏 I2C 刷新 | 1000 Hz | ~3 ms | **100%（持续工作）** |
| CPU 唤醒 | 1000 Hz | — | 几乎 100% |

屏幕被以 **1000 Hz**（每秒 1000 次）的速率全屏刷新，而人眼只需要 30~60 Hz。多余的 940+ 次刷新完全浪费电力。

### 解决方案

在主循环中添加帧率控制，仅在必要时刷新屏幕：

```c
// main.c — 修改后
#define FRAME_PERIOD_MS     33      // 约 30 FPS

while (1)
{
    static uint32_t last_frame_tick = 0;
    uint32_t now = HAL_GetTick();

    // 帧率控制：仅在到达下一帧时刻时才重绘
    if(now - last_frame_tick >= FRAME_PERIOD_MS)
    {
        last_frame_tick = now;

        OLED_Clear();
        Battery_Show_UI();
        OLED_Update();
    }

    ClockUI_Move_Flag = First_Page_Clock();
    if(ClockUI_Move_Flag == 1) { Menu_Page(); }
    else if(ClockUI_Move_Flag == 2) { SettingPage(); }
}
```

> ⚠️ **注意：** 此方案有一个重要前提——`__WFI()` 的唤醒源必须改为非周期性唤醒。当前 `First_Page_Clock()` 等函数内部使用 `while(1) { ... __WFI(); }` 模式，被 TIM2 每 1ms 唤醒一次。要真正降低功耗，需要：
>
> 1. 将时钟页面改为**仅在按键中断时唤醒**（使用 EXTI 外部中断代替轮询），而非定时器周期性唤醒
> 2. 或者将 TIM2 周期从 1ms 增加到 33ms（30Hz），但会影响按键扫描响应速度（KeyTick 也依赖 TIM2）
>
> **建议将此列为架构级改进，需要更全面的重新设计。**

**折中方案（低风险）：**

如果不想改动中断架构，可以简单降低主循环的刷新率：

```c
// First_Page_Clock() 内部，在 __WFI() 前添加
// 将 TIM2 周期从 1ms 改为 10ms（降低 10 倍刷新率）
// TIM2: Prescaler=719, Period=999 → 72MHz/720/1000 = 100Hz = 10ms
```

这样屏幕刷新率从 1000Hz 降到 100Hz，功耗显著降低，同时按键扫描间隔 10ms 仍可接受（人类按键反应时间 > 100ms）。

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `SmartWatch_HAL/HAL/Core/Src/main.c` | 主循环添加帧率控制 |
| `SmartWatch_HAL/HAL/Core/Src/main.c` | `MX_TIM2_Init()`：TIM2 Period 从 99 调整（可选，需权衡按键响应） |

### 备注

- 此问题与问题六（时钟频率恢复）有交互：恢复 72MHz 后 OLED 刷新更快，但每秒刷新次数不变，功耗问题反而更突出（CPU 更快完成工作后有更多空闲时间，但 `__WFI()` 仍被 1ms 周期唤醒）
- 长期方案建议将按键检测改为 EXTI 中断 + 消抖定时器，主循环只在有事件时才唤醒刷新

---

## Bug 汇总

| 编号 | 优先级 | 文件 | 问题 | 故障现象 |
|------|--------|------|------|---------|
| 问题七 | 🔴 P0 ✅ | `SetTime.c` | `Set_Min()` 使用错误的数组索引 `5`（秒）而非 `4`（分） | 分钟永远无法被修改 |
| 问题八 | 🔴 P0 ✅ | `SetTime.c` | `Set_Hour()` 上边界 `>= 25` 应为 `>= 24`，回绕值 `24` 应为 `23` | 小时可设为无效值 24 |
| 问题九 | 🟠 P1 ✅ | `menu.c` | `Battery_ShowUI()` 先显示后边界限制，负值传入 `uint32_t` | 低电量时屏幕显示乱码 |
| 问题十 | 🟡 P2 | `Key.c` | `Key_GetNum()` 读-改-写非原子，存在 ISR 竞态 | 偶尔按键无响应 |
| 问题十一 | 🟡 P2 | `dino.c` | `isColliding()` 内部含 UI 渲染和 1s 阻塞延时 | 游戏结束冻结 1s |
| 问题十二 | 🟢 P3 | `dino.c` | `dino.maxX = DINO_WIDTH` 应为 `DINO_X_POS + DINO_WIDTH` | 当前巧合正确，未来有隐患 |
| 问题十三 | 🟢 P3 | `dino.c` | `Dino_JumpCount` 静态初始化为 1 而非 0 | 当前被 `Game_Init()` 覆盖，代码不规范 |
| 问题十四 | 🟡 P2 | `OLED.c` / `MyI2C.c` | 软件 I2C 无延时，SCL 频率超限 | OLED 偶发花屏、MPU6050 读数偶发出错 |
| 问题十五 | 🟡 P2 | `menu.c` | 主循环每 1ms 全屏刷新，屏幕以 1000Hz 无意义重绘 | 电池耗电过快 |

### 修复建议优先级

1. ~~**立即修复（P0）：** 问题七 + 问题八~~ ✅ 已修复 — `Set_Min()` 索引修正为4，`Set_Hour()` 边界修正为 `>= 24` / 回绕 `23`
2. ~~**尽快修复（P1）：** 问题九~~ ✅ 已修复 — 边界检查移到 `OLED_ShowNum()` 之前，移除冗余代码
3. **计划修复（P2）：** 问题十、十一、十四、十五 — 影响可靠性、可维护性和功耗
4. **低优先级（P3）：** 问题十二、十三 — 代码规范性改进，当前无可见影响
