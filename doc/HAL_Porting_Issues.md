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
