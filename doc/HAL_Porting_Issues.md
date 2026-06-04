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
