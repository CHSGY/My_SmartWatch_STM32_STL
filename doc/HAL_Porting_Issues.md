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
