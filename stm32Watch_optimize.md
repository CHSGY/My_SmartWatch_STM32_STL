# STM32 智能手表项目优化建议

> 项目：My_SmartWatch_STM32_STL  
> 分析日期：2025年  
> 平台：STM32F10x (Cortex-M3)  
> 工具链：Keil uVision

---

## 1. 关键 Bug 修复

### 1.1 🐛 `dino_tick()` — 云朵环绕逻辑失效

**文件：** `Hardware/dino.c`  
**问题：** `Cloud_Count` 刚被置 0，紧接着判断 `Cloud_Count > 200`，该条件**永远不成立**，云朵永远不会环绕。

```c
// ❌ 当前代码（dino.c:225-232）
if (Cloud_Count >= 50) {
    Cloud_Pos++;
    Cloud_Count = 0;
    if (Cloud_Count > 200) {   // ← 刚置0，永远为false
        Cloud_Pos = 0;
    }
}

// ✅ 修复方案：判断 Cloud_Pos 而非 Cloud_Count
if (Cloud_Count >= 50) {
    Cloud_Count = 0;
    Cloud_Pos++;
    if (Cloud_Pos > 200) {
        Cloud_Pos = 0;
    }
}
```

---

### 1.2 🐛 `KeyTick()` — `KeyTimeFlag` 重复复位

**文件：** `Hardware/Key.c`  
**问题：** `KeyTimeFlag = 0` 在 `if` 内部和外部各出现一次，外部无条件复位导致内部复位冗余。虽然功能上每 20ms 扫描一次不受影响，但代码逻辑混乱，易引入时序错误。

```c
// ❌ 当前代码
void KeyTick(void) {
    KeyTimeFlag++;
    if (KeyTimeFlag >= 20) {
        Pre_KeyState = Cur_KeyState;
        Cur_KeyState = Key_GetState();
        if (Pre_KeyState != 0 && Cur_KeyState == 0) {
            Key_Num = Pre_KeyState;
            KeyTimeFlag = 0;   // ← 冗余
        }
        KeyTimeFlag = 0;       // ← 外部复位已足够
    }
}

// ✅ 修复方案：去掉内部冗余复位
void KeyTick(void) {
    KeyTimeFlag++;
    if (KeyTimeFlag >= 20) {
        Pre_KeyState = Cur_KeyState;
        Cur_KeyState = Key_GetState();
        if (Pre_KeyState != 0 && Cur_KeyState == 0) {
            Key_Num = Pre_KeyState;
        }
        KeyTimeFlag = 0;
    }
}
```

---

## 2. 性能优化

### 2.1 ⚡ `Battery_Show_UI()` — ADC 过采样导致严重卡顿

**文件：** `Hardware/menu.c`  
**问题：** 每次电池 UI 刷新都采集 3000 次 ADC 求均值。ADC 单次转换约 18μs，3000 次约 **54ms**，完全阻塞 UI。

```c
// ❌ 当前代码 — 每次刷新卡顿54ms
void Battery_Show_UI(void) {
    ...
    for (i = 0; i < 3000; i++) {
        AD_Value = AD_GetValue();
        sum += AD_Value;
    }
    AD_Value = sum / 3000;
    ...
}

// ✅ 优化方案：缓存结果，仅在需要时更新
static uint16_t cached_AD_Value = 0;
static uint8_t Battery_Refresh_Cnt = 0;

void Battery_Show_UI(void) {
    Battery_Refresh_Cnt++;
    if (Battery_Refresh_Cnt < 50) {   // 每50帧才采样一次
        // 使用缓存值显示
        Draw_Battery_UI(cached_AD_Value);
        return;
    }
    Battery_Refresh_Cnt = 0;

    // 降采样：取8~16次即可
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += AD_GetValue();
    }
    cached_AD_Value = sum / 16;
    Draw_Battery_UI(cached_AD_Value);
}
```

**收益：** UI 刷新速度提升约 50×，避免 ADC 采样阻塞画面渲染。

---

### 2.2 ⚡ `Gradienter_Func()` — `Show_Gradienter_UI()` 内部重复 `OLED_Update()`

**文件：** `Hardware/menu.c`  
**问题：** `Show_Gradienter_UI()` 内部已调用 `OLED_Update()`（第 991 行），但 `Gradienter_Func()` 循环末尾又额外调用了一次（第 1008 行），导致每次循环多进行一次全屏 SPI/I2C 传输。`OLED_Clear()` 在这里是必需的（否则旧帧圆环残留），**不是冗余**。

```c
// ❌ 当前代码
void Show_Gradienter_UI(void) {
    ...
    OLED_DrawCircle(64,32,30,OLED_UNFILLED);
    ...
    OLED_Update();  // ← 第1次 Update（内部已调用）
}

uint8_t Gradienter_Func(void) {
    while (1) {
        ...
        OLED_Clear();           // ← 必需（清除上一帧）
        Show_Gradienter_UI();   // ← 内部已调 OLED_Update()
        OLED_Update();          // ← 冗余！第2次全屏刷新
    }
}

// ✅ 修复方案：移除 Gradienter_Func 中的冗余 OLED_Update()
uint8_t Gradienter_Func(void) {
    while (1) {
        KeyNum = Key_GetNum();
        if (KeyNum == 3) { ... }
        OLED_Clear();
        Show_Gradienter_UI();   // 内部已调用 OLED_Update()
        // OLED_Update();       // ← 删除此行
    }
}
```

**收益：** 每次循环节省一次 ~1ms 的 SPI 全屏传输。

---

### 2.3 ⚡ `OLED_ShowImage()` 调用频次优化

**涉及文件：** `Hardware/dino.c`  
**问题：** 在 `Dino_game_Animation()` 中每次循环都调用 5 次 `OLED_ShowImage()`（分数、地面、障碍物、云朵、恐龙），每帧都操作大量位图数据。可考虑仅更新变化区域以加速。

```c
// ✅ 优化思路：使用局部刷新
OLED_ClearArea(0, 44, 128, 18);         // 仅清除恐龙和障碍物区域
Show_Dino();
Show_Barrier();
OLED_UpdateArea(0, 44, 128, 18);        // 仅刷新变化区域
```

如果 OLED 驱动支持 `OLED_UpdateArea()`，局部刷新的帧率可提升 3~5 倍。

---

## 3. 代码健壮性改进

### 3.1 🔧 `AD_Init()` — `ADC_NbrOfChannel` 与实际配置不一致

**文件：** `Hardware/AD.c`  
**问题：** `ADC_NbrOfChannel` 设为 2，但实际只配置了 1 个通道（Channel 2）。在非扫描模式下该值理论上被忽略，但语义不一致，易让后续维护者困惑。

```c
// ❌ 当前代码
ADC_InitStructure.ADC_NbrOfChannel = 2;

// ✅ 修复方案
ADC_InitStructure.ADC_NbrOfChannel = 1;
```

---

### 3.2 🔧 `Key_Num` 缺少 `volatile` 修饰

**文件：** `Hardware/Key.c`  
**问题：** `Key_Num` 在中断函数 `KeyTick()` 中被写入，在主循环 `Key_GetNum()` 中被读取。缺少 `volatile` 可能导致编译器优化时读取缓存值而非真实内存值。

```c
// ✅ 修复方案
volatile uint8_t Key_Num;
```

同理，`KeyTimeFlag`、`Pre_KeyState`、`Cur_KeyState` 等中断与主循环共享的变量也应加 `volatile`。

---

### 3.3 🔧 `dino_tick()` — `Cloud_Pos` 类型范围不足

**文件：** `Hardware/dino.c`  
**问题：** `Cloud_Pos` 定义为 `uint8_t`（0~255），但环绕阈值设为 200。虽然目前不会溢出，但如果未来调整速度或增加新元素，应使用更大类型或明确约束。

```c
// ✅ 建议
uint16_t Cloud_Pos;   // 或保持 uint8_t 但明确注释范围
```

---

## 4. 架构与可维护性

### 4.1 📐 全局变量名规范

**涉及文件：** `Hardware/dino.c`  
**问题：** 全局变量（`score`, `ground_count`, `Barrier_Pos` 等）使用短名且无模块前缀，容易与其他模块冲突。

```c
// ❌ 当前
uint8_t Barrier_Pos;
uint8_t Cloud_Pos;

// ✅ 建议
uint8_t Dino_BarrierPos;
uint8_t Dino_CloudPos;
```

**或** 将游戏状态封装为结构体：

```c
typedef struct {
    int16_t score;
    uint16_t groundPos;
    uint8_t barrierPos;
    uint8_t cloudPos;
    uint8_t dinoJumpFlag;
    int16_t dinoJumpPos;
    // ...
} Dino_GameState_t;

static Dino_GameState_t GameState;
```

---

### 4.2 📐 `dino_tick()` 与 `KeyTick()` 中断职责拆分

**问题：** 两个 `_tick()` 函数都依赖 1ms 定时器中断，但耦合在同一个 `TIM2_IRQHandler` 中。未来若需调整 tick 频率或增加新模块，中断函数会膨胀。

```c
// ✅ 建议结构
void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET) {
        Dino_Tick();      // 游戏物理更新
        Key_Tick();       // 按键扫描
        // 未来：Battery_Tick(); Sensor_Tick(); ...
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}
```

---

### 4.3 📐 幻数（Magic Number）抽取为宏

**涉及文件：** `Hardware/dino.c`, `Hardware/menu.c`  
**问题：** 多处直接使用硬编码数字：

| 位置 | 值 | 含义 |
|------|-----|------|
| dino.c:229 | 200 | 云朵最大 X 位置 |
| dino.c:83 | 144 | 障碍物最大 X 位置 |
| dino.c:84-85 | 44, 62 | 地面 Y 范围 |
| dino.c:81-82 | 0, 16 | 恐龙 X 范围 |
| menu.c:305-306 | 44, 62 | 地面 Y 范围（镜像） |

```c
// ✅ 建议
#define CLOUD_MAX_POS       200
#define BARRIER_MAX_POS     144
#define GROUND_Y_MIN        44
#define GROUND_Y_MAX        62
#define DINO_WIDTH          16
#define DINO_HEIGHT         18
```

---

## 5. 功耗优化（电池供电场景）

### 5.1 🔋 OLED 刷新策略

当前每帧执行 `OLED_Update()` 全屏刷新（128×64 像素，约 1024 字节 SPI/I2C 传输）。建议：

```c
// ✅ 使用局部刷新替代全刷
OLED_UpdateArea(X, Y, Width, Height);
```

对于游戏循环（`Dino_game_Animation`），仅刷新变化区域（地面 + 障碍物 + 恐龙所在的带状区域），SPI 传输量减少约 70%。

### 5.2 🔋 空闲时进入低功耗模式

在 `Menu_Page()` 等等待用户输入的死循环中，可插入 `__WFI()` 指令：

```c
while (1) {
    KeyNum = Key_GetNum();
    if (KeyNum != 0) { ... }
    __WFI();   // 等待中断唤醒（按键中断或定时器）
}
```

---

## 6. 已知功能缺失

| 模块 | 现状 | 建议 |
|------|------|------|
| **秒表** (`StopClock`) | 秒表功能完整：有 UI 绘制、Tick 计时、开始/停止/清除控制。但 `Sec` 变量直接使用 `uint8_t sec`（menu.c:469），秒数最大仅 255 秒（约 4 分钟），无法支持长时间计时 | 改为 `uint16_t sec` 或使用 `uint32_t`；或使用 BCD 编码的时分秒格式 |
| **闹钟/倒计时** | 无实现 | 可复用 RTC 闹钟中断 |
| **设置持久化** | 时间设置只修改内存数组，掉电后复位 | 利用 BKP 寄存器或片内 Flash 保存设置 |
| **心率/计步** | MPU6050 已初始化但仅用于水平仪 | 添加计步算法 |

---

## 7. 编译警告清理建议

检查以下潜在的编译警告：

| 文件 | 问题 | 严重程度 |
|------|------|----------|
| `dino.c` | `Jump_count` 初始化为 1 而非 0（但 `Game_Init()` 会重置为 0） | 低 — 不影响功能 |
| `dino.c:146` | `if (Cloud_Pos%2 == 0)` 中使用 `%` 操作 `uint8_t` | 低 — 编译器会自动提升为 int |
| `menu.c:552` | `uint8_t Temp_StopClock_Flag` 值在触发一次动作后不会自动归零，下次 Key3 按下前可能重复执行同一操作 | 中 — 逻辑隐患（但功能上无害，因为启动/停止是幂等的） |
| `Key.c:129-137` | `Key_GetState()` 中两个分支都读 `PA4==0`，分别判断 `press_time < 1000` 和 `press_time >= 1000` | 低 — 逻辑等价于一个 `if/else` 内的 `press_time` 判断 |

---

## 8. 🚀 提升技术竞争力：进阶优化路线图

> 本节聚焦**职业发展导向**的优化。这些改动不是为了修复当前项目，
> 而是为了在简历中展示行业级嵌入式开发能力。
> 每项都标注了**面试高频度**（★越多面试越常问）和**难度等级**。

---

### 8.1 引入实时操作系统（RTOS）——最核心的技能飞跃

| 维度 | 说明 |
|------|------|
| **🎯 面试频度** | ★★★★★（几乎每家嵌入式公司必问） |
| **📈 难度** | ⭐⭐⭐⭐（中等偏上） |
| **🕐 预估工时** | 2~3 周 |

**当前瓶颈：** 项目使用超级循环（Super Loop）架构，所有功能在 `while(1)` 中轮询。
中断服务函数中做了过多工作，缺乏任务优先级管理。

**方案：移植 FreeRTOS（或 RT-Thread）**

```c
// ✅ RTOS 化的任务结构
void Task_Display(void *pvParameters) {
    while (1) {
        OLED_Update();
        vTaskDelay(pdMS_TO_TICKS(33));  // ~30fps
    }
}

void Task_KeyScan(void *pvParameters) {
    while (1) {
        KeyTick();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void Task_BatteryMonitor(void *pvParameters) {
    while (1) {
        AD_Value = AD_GetValue();
        // 电量计算...
        vTaskDelay(pdMS_TO_TICKS(5000));  // 每5秒采样一次
    }
}

void Task_DinoGame(void *pvParameters) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // 等待按键通知
        Dino_game_Animation();
    }
}
```

**✅ 简历亮点：**
- "基于 FreeRTOS 的多任务嵌入式系统设计"
- "任务优先级规划、信号量/队列 IPC 机制"
- "RTOS 下的中断嵌套与临界区保护"

**推荐替代方案：** RT-Thread（国产开源，国内岗位认可度极高，且有组件生态）

---

### 8.2 迁移至现代 MCU 平台

| 维度 | 说明 |
|------|------|
| **🎯 面试频度** | ★★★★（体现技术更新能力） |
| **📈 难度** | ⭐⭐⭐ |
| **🕐 预估工时** | 1~2 周 |

**当前瓶颈：** STM32F103C8T6 是 2007 年发布的 Cortex-M3 内核，72MHz，20KB RAM，
无硬件 FPU、无 DCMI、无 USB HS。在 2025 年的招聘市场中，F1 系列已非主流。

**推荐迁移路径：**

| 目标型号 | 内核 | 优势 | 学习价值 |
|----------|------|------|----------|
| **STM32G431** | CM4F @170MHz | FPU、CORDIC、FMAC | 工业控制、数字电源热门 |
| **STM32H750** | CM7 @480MHz | 大 RAM、LCD-TFT、LTDC | 高端嵌入式、HMI |
| **ESP32-S3** | Xtensa LX7 + CM3 | BLE/WiFi、大 RAM | AIoT 绝对主流 |
| **AT32F403A** | CM4F @240MHz | 国产替代、性价比高 | 国产化趋势 |

**✅ 简历亮点：**
- "MCU 平台迁移经验（F103 → G4/H7）"
- "HAL/LL 库驱动重构"
- "熟悉 ARM Cortex-M4F DSP 指令集"

---

### 8.3 添加 BLE 蓝牙通信功能

| 维度 | 说明 |
|------|------|
| **🎯 面试频度** | ★★★★★（IoT 岗位必问） |
| **📈 难度** | ⭐⭐⭐⭐⭐ |
| **🕐 预估工时** | 3~4 周 |

**当前瓶颈：** 手表无法与手机通信，没有蓝牙，没有 App。

**方案：**

```
┌─────────────────────┐     BLE GATT     ┌──────────────────┐
│  STM32 + BLE 模块   │ ◄──────────────► │  手机 App       │
│  (ESP32 / NRF52832) │   Notification    │  (Flutter/小程序) │
│                     │                  │                  │
│  [服务]             │                  │  [显示]          │
│   · 电量特征        │                  │   · 电量         │
│   · 时间同步特征    │                  │   · 心率         │
│   · 通知推送特征    │                  │   · 消息推送     │
└─────────────────────┘                  └──────────────────┘
```

**硬件方案对比：**

| 方案 | 成本 | 难度 | 简历加分 |
|------|------|------|----------|
| **ESP32-C3 + ESP-IDF** | ￥15 | ⭐⭐⭐⭐ | BLE + WiFi + RISC-V 三重亮点 |
| **NRF52832 + SDK** | ￥25 | ⭐⭐⭐⭐⭐ | 纯 BLE 专业方案，Nordic 官方 SDK |
| **STM32WB55** | ￥30 | ⭐⭐⭐⭐⭐ | 单芯片方案，ST 官方 BLE 栈 |
| **AT09/HC-08 透传** | ￥8 | ⭐⭐ | 简单但不具技术深度 |

**✅ 简历亮点：**
- "BLE GATT 协议栈开发，Service/Characteristic 自定义"
- "手机端 微信小程序/Flutter App 联动开发"
- "无线 OTA 固件升级方案设计"

---

### 8.4 实现 Bootloader + OTA 远程升级

| 维度 | 说明 |
|------|------|
| **🎯 面试频度** | ★★★★（体现系统级思维） |
| **📈 难度** | ⭐⭐⭐⭐ |
| **🕐 预估工时** | 2 周 |

**方案：**

```
Flash 布局：
┌────────────────────┐  0x08000000
│    Bootloader       │  (16KB)
│  - CRC校验          │
│  - 固件下载         │
│  - 回滚机制         │
├────────────────────┤  0x08004000
│    App-V1 (当前)    │  (32KB)
├────────────────────┤  0x0800C000
│    App-V2 (备份)    │  (32KB)
├────────────────────┤  0x08014000
│    参数区           │  (8KB)
└────────────────────┘
```

**关键技术点：**
- IAP（In-Application Programming）原理
- 向量表重定位（SCB->VTOR）
- 固件完整性校验（CRC32/MD5）
- 双备份 + 看门狗回滚保护

**✅ 简历亮点：**
- "基于 STM32 的 Bootloader/OTA 方案设计"
- "固件差分升级算法（Xdelta/Diff）"
- "可靠性设计：CRC 校验 + 双区备份 + 看门狗回滚"

---

### 8.5 构建 CMake + GitHub Actions CI/CD

| 维度 | 说明 |
|------|------|
| **🎯 面试频度** | ★★★（体现工程化能力） |
| **📈 难度** | ⭐⭐ |
| **🕐 预估工时** | 3~5 天 |

**当前瓶颈：** 项目依赖 Keil MDK（收费、仅 Windows、命令行不友好），
无法 CI、无法自动化测试。

**方案：**

```cmake
# ✅ CMakeLists.txt (跨平台)
cmake_minimum_required(VERSION 3.20)
project(MySmartWatch C ASM)

set(MCU_FAMILY STM32F103x8)
set(LINKER_SCRIPT "STM32F103C8Tx_FLASH.ld")

add_executable(${PROJECT_NAME}
    User/main.c
    Hardware/OLED.c
    Hardware/Key.c
    Hardware/MPU6050.c
    Hardware/dino.c
    Hardware/menu.c
    System/MyRTC.c
    System/Timer.c
    # ...
)

target_include_directories(${PROJECT_NAME} PRIVATE
    User Hardware System
)

target_link_options(${PROJECT_NAME} PRIVATE
    -T ${LINKER_SCRIPT}
    -u _printf_float
)
```

```yaml
# ✅ .github/workflows/build.yml
name: Firmware CI
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install ARM GCC
        run: sudo apt install -y gcc-arm-none-eabi cmake build-essential
      - name: Build
        run: |
          cmake -B build -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake
          cmake --build build
      - name: Generate Hex
        run: arm-none-eabi-objcopy -O ihex build/firmware.elf firmware.hex
      - name: Archive Firmware
        uses: actions/upload-artifact@v4
        with:
          name: firmware
          path: firmware.hex
```

**✅ 简历亮点：**
- "嵌入式项目 CMake 构建系统搭建"
- "GitHub Actions CI/CD 自动化编译 + 固件发布"
- "跨平台开发环境（Linux/macOS/Windows）"

---

### 8.6 引入单元测试框架

| 维度 | 说明 |
|------|------|
| **🎯 面试频度** | ★★★★（大厂嵌入式必考） |
| **📈 难度** | ⭐⭐⭐ |
| **🕐 预估工时** | 1~2 周 |

**当前瓶颈：** 项目中无任何测试代码，bug 只能靠上板实测发现。

**方案：使用 Ceedling + Unity + CMock（嵌入式测试三件套）**

```c
// ✅ 对 dino.c 的物理逻辑做单元测试（无需硬件）

// test/test_dino.c
#include "unity.h"
#include "dino.h"

void setUp(void) { Dino_Reset(); }
void tearDown(void) {}

void test_dino_initial_score_is_zero(void) {
    TEST_ASSERT_EQUAL(0, Dino_GetScore());
}

void test_barrier_wraps_after_144_pixels(void) {
    for (int i = 0; i < 150; i++) {
        Dino_Tick();   // 软件仿真 tick
    }
    TEST_ASSERT_TRUE(Dino_GetBarrierPos() < 144);
}

void test_cloud_wraps_after_200_pixels(void) {
    for (int i = 0; i < 300; i++) {
        Dino_Tick();
    }
    TEST_ASSERT_TRUE(Dino_GetCloudPos() < 200);
}
```

**✅ 简历亮点：**
- "嵌入式 TDD（测试驱动开发）实践"
- "Ceedling + Unity + CMock 单元测试框架"
- "Mock 对象隔离硬件依赖，实现纯逻辑测试"

---

### 8.7 集成 LVGL GUI 框架

| 维度 | 说明 |
|------|------|
| **🎯 面试频度** | ★★★（HMI/消费电子方向加分） |
| **📈 难度** | ⭐⭐⭐⭐ |
| **🕐 预估工时** | 2~4 周 |

**当前瓶颈：** 像素级手动绘图，UI 迭代慢，无动画/触摸/主题支持。

**方案：** 将 OLED 驱动对接 LVGL（需至少 8KB RAM，F103 勉强可用，G4/H7 更佳）

```c
// ✅ LVGL 化后的 UI 代码示例
void lv_ui_init(void) {
    lv_obj_t *main_page = lv_obj_create(lv_scr_act());
    
    // 时间显示标签（自动字体渲染）
    lv_obj_t *time_label = lv_label_create(main_page);
    lv_label_set_text(time_label, "12:30:00");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    
    // 电池图标（内置Symbol）
    lv_obj_t *battery_icon = lv_label_create(main_page);
    lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_FULL);
    
    // 动画（自动处理刷新）
    lv_anim_t a;
    lv_anim_set_var(&a, time_label);
    lv_anim_set_exec_cb(&a, time_update_anim_cb);
    lv_anim_set_time(&a, 1000);
    lv_anim_start(&a);
}
```

**✅ 简历亮点：**
- "LVGL 嵌入式 GUI 框架移植与应用"
- "自定义控件开发，动画系统集成"
- "面向 HMI 产品的 UI 架构设计"

---

### 8.8 传感器融合算法（卡尔曼滤波）

| 维度 | 说明 |
|------|------|
| **🎯 面试频度** | ★★★★（机器人/飞控/可穿戴方向） |
| **📈 难度** | ⭐⭐⭐⭐⭐ |
| **🕐 预估工时** | 2~3 周 |

**当前瓶颈：** MPU6050 仅用于水平仪（读取原始角度），数据噪声大，
无滤波、无计步、无姿态解算。

**方案：实现一维卡尔曼滤波 + 计步算法**

```c
// ✅ 卡尔曼滤波（俯仰角估计）
typedef struct {
    float Q;    // 过程噪声协方差
    float R;    // 测量噪声协方差
    float P;    // 估计误差协方差
    float K;    // 卡尔曼增益
    float X;    // 估计值
} KalmanFilter_t;

float Kalman_Update(KalmanFilter_t *kf, float measurement) {
    // 预测
    kf->P = kf->P + kf->Q;
    // 更新
    kf->K = kf->P / (kf->P + kf->R);
    kf->X = kf->X + kf->K * (measurement - kf->X);
    kf->P = (1 - kf->K) * kf->P;
    return kf->X;
}
```

**计步算法（时域峰值检测）：**

```c
#define STEP_THRESHOLD    1.2f    // 加速度阈值
#define STEP_MIN_INTERVAL 200     // 最小步间隔(ms)

uint32_t Pedometer_Update(float acc_magnitude) {
    static uint32_t last_step_time = 0;
    static float last_peak = 0;
    uint32_t now = GetTick();
    
    if (acc_magnitude > STEP_THRESHOLD && 
        acc_magnitude > last_peak &&
        (now - last_step_time) > STEP_MIN_INTERVAL) {
        last_step_time = now;
        step_count++;
    }
    last_peak = acc_magnitude;
    return step_count;
}
```

**✅ 简历亮点：**
- "卡尔曼滤波在嵌入式平台的实现与调优"
- "基于 MPU6050 的计步/姿态估计算法"
- "传感器数据融合中的信号处理技巧"

---

### 8.9 DMA + 双缓冲 OLED 显示流水线

| 维度 | 说明 |
|------|------|
| **🎯 面试频度** | ★★★★（体现性能优化思维） |
| **📈 难度** | ⭐⭐⭐ |
| **🕐 预估工时** | 1 周 |

**当前瓶颈：** 当前 `OLED_Update()` 使用 CPU 轮询 SPI/I2C 发送，
数据传输期间 CPU 被占用，无法并行渲染下一帧。

**方案：DMA + 双缓冲**

```
┌──────────┐    DMA     ┌──────────┐
│ Buffer A  │◄─────────►│  OLED     │  ← DMA 正在发送 Buffer A
│ (渲染中)  │  SPI/I2C  │  显示屏   │
├──────────┤           └──────────┘
│ Buffer B  │
│ (显示中)  │
└──────────┘
      ↓ 帧结束中断交换
┌──────────┐
│ Buffer A  │  ← 现在显示
├──────────┤
│ Buffer B  │  ← CPU 开始渲染下一帧
└──────────┘
```

```c
// 双缓冲切换
void OLED_Update_DMA(void) {
    // 等待上一次DMA传输完成
    while (DMA_GetFlagStatus(DMA1_FLAG_TC1) == RESET);
    
    // 交换缓冲区
    active_buf ^= 1;
    
    // 启动DMA传输当前帧
    DMA_SetCurrDataCounter(DMA1_Channel1, 1024);
    DMA_Cmd(DMA1_Channel1, ENABLE);
    
    // CPU 立即返回，渲染下一帧到非活跃缓冲区
    Render_Frame(display_buf[active_buf ^ 1]);
}
```

**✅ 简历亮点：**
- "DMA + 双缓冲机制优化显示流水线"
- "CPU 与外设并行工作，帧率提升 2-3 倍"
- "消除画面撕裂（tearing）"

---

### 8.10 编写专业级文档体系

| 维度 | 说明 |
|------|------|
| **🎯 面试频度** | ★★（但面试官显著好感） |
| **📈 难度** | ⭐ |
| **🕐 预估工时** | 1~2 天 |

**方案：**

```
docs/
├── architecture.md      # 软件架构图 + 模块依赖
├── hardware.md           # 引脚分配表 + 原理图
├── api/
│   ├── oled.md           # OLED 驱动 API 手册
│   ├── key.md            # 按键模块 API
│   └── dino.md           # 游戏模块 API
├── build.md              # 编译环境搭建指南
└── changelog.md          # 版本变更日志
```

同时，在代码中添加 **Doxygen 注释**：
```c
/**
 * @brief 更新恐龙跳跃物理状态
 * @param jump_flag 跳跃标志：0-未跳 1-跳跃中
 * @param jump_pos  跳跃高度(像素)，范围 0~16
 * @param jump_count 跳跃计时器，控制抛物线时间轴
 * @note  跳跃曲线: y = 16 * sin(π * count/1000)
 *        每 tick 调用一次（1ms间隔）
 */
void Dino_UpdateJump(uint8_t jump_flag, int16_t *jump_pos, int16_t *jump_count);
```

**✅ 简历亮点：**
- "完善的嵌入式项目文档体系"
- "Doxygen 自动化 API 文档生成"
- "硬件/软件接口规范文档"

---

## 9. 📊 职业竞争力提升路线图

### 9.1 优先级排序（按投入产出比）

| 优先级 | 优化项 | 学习周期 | 简历含金量 | 推荐指数 |
|--------|--------|----------|------------|----------|
| **P0** | 🥇 移植 FreeRTOS | 2~3 周 | ★★★★★ | ⭐⭐⭐⭐⭐ |
| **P0** | 🥇 添加 BLE 通信 | 3~4 周 | ★★★★★ | ⭐⭐⭐⭐⭐ |
| **P1** | 🥈 CMake + CI/CD | 3~5 天 | ★★★★ | ⭐⭐⭐⭐⭐ |
| **P1** | 🥈 DMA + 双缓冲 | 1 周 | ★★★★ | ⭐⭐⭐⭐ |
| **P1** | 🥈 单元测试框架 | 1~2 周 | ★★★★ | ⭐⭐⭐⭐ |
| **P2** | 🥉 迁移至 G4/H7 | 1~2 周 | ★★★ | ⭐⭐⭐⭐ |
| **P2** | 🥉 Bootloader/OTA | 2 周 | ★★★ | ⭐⭐⭐⭐ |
| **P3** | 卡尔曼滤波 + 计步 | 2~3 周 | ★★★ | ⭐⭐⭐ |
| **P3** | LVGL GUI | 2~4 周 | ★★ | ⭐⭐⭐ |
| **P4** | 文档体系完善 | 1~2 天 | ★ | ⭐⭐⭐ |

### 9.2 三个月学习路径规划

```
第1个月：基础夯实（每周20h）
├── Week 1-2：FreeRTOS 移植 + 3个任务（显示/按键/电池）
├── Week 3  ：CMake 构建 + GitHub CI 流水线
└── Week 4  ：DMA 双缓冲显示优化

第2个月：通信 + 可靠性（每周20h）
├── Week 5-6：BLE 模块驱动 + GATT 服务
├── Week 7  ：微信小程序/Flutter App 配套开发
└── Week 8  ：Bootloader + OTA 方案

第3个月：算法 + 算法 + 求职准备（每周15h）
├── Week 9-10：卡尔曼滤波 + 计步算法
├── Week 11  ：整理项目到简历 + GitHub README
└── Week 12  ：模拟面试 + 刷题（嵌入式八股）
```

### 9.3 简历上的写法示范

> **智能手表嵌入式系统** (GitHub 开源项目)
> - 基于 STM32F103 + FreeRTOS 的多任务嵌入式系统设计，实现 OLED 显示、按键交互、RTC 时间管理
> - 添加 BLE 蓝牙通信模块，实现手机 App 与手表的电量/时间同步、通知推送功能
> - 搭建 CMake + GitHub Actions CI/CD 构建流水线，自动编译/测试/发布固件
> - 实现 IAP Bootloader + OTA 升级，双备份回滚保护，CRC32 固件校验
> - 硬件级优化：DMA 双缓冲显示流水线，CPU 利用率降低 60%
> - 嵌入式 TDD 实践：Ceedling 单元测试覆盖核心逻辑模块

### 9.4 面试常见问题（基于本项目）

| 面试题 | 本题的对应回答方向 |
|--------|-------------------|
| "你怎么管理多个任务？" | 引入 FreeRTOS，讲述任务优先级、IPC 设计 |
| "遇到过什么棘手 bug？" | 云朵环绕 Bug / 中断共享变量 volatile |
| "怎么优化性能？" | ADC 采样子抽样、DMA 双缓冲、局部刷新 |
| "怎么做低功耗？" | WFI 睡眠、OLED 局部刷新、ADC 周期采样 |
| "有团队协作经验吗？" | Git flow、semantic commit、CI/CD、文档规范 |
| "对 BLE 了解吗？" | GATT 服务设计、Nordic/ESP BLE 栈经验 |

---

## 总结

### 短期修复（提升代码质量）

1. **🔴 `dino_tick()` 云朵环绕 Bug** — 障碍物移出后不回绕，导致游戏不公平
2. **🔴 `Battery_Show_UI()` 3000次 ADC 采样** — 每次进入都卡顿 54ms
3. **🟡 `Key_Num` 缺少 `volatile`** — 高优化等级下可能产生未定义行为
4. **🟡 `Gradienter_Func()` 重复 `OLED_Update()`** — `Show_Gradienter_UI()` 内部已调用，外层又多调一次
5. **🟢 `AD_Init()` `NbrOfChannel` 不一致** — 代码可读性
6. **🟢 `StopClock` 秒表 `sec` 变量类型不足** — `uint8_t` 最大 255 秒（约 4 分钟）

### 长期投资（职业竞争力）

| 项目 | 投入 | 产出 |
|------|------|------|
| FreeRTOS 移植 | 2~3 周 | 面试必问 + 架构思维飞跃 |
| BLE 通信 | 3~4 周 | IoT 岗位核心技能 |
| CMake + CI | 3~5 天 | 工程化能力证明 |
| 单元测试 | 1~2 周 | 大厂软工基础要求 |
| Bootloader | 2 周 | 系统级设计能力 |
