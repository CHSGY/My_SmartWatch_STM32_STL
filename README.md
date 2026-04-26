# STM32 Watch

基于 STM32F103C8 的智能手表项目，包含时钟、菜单系统、体感游戏、计步器等功能的嵌入式开源项目。

## 项目简介

这是一个运行在 STM32F103C8 微控制器上的智能手表应用，采用无操作系统（bare-metal）架构，通过自定义 OLED 驱动和按钮输入实现人机交互。

## 硬件配置

| 项目 | 配置 |
|------|------|
| MCU | STM32F103C8 (ARM Cortex-M3) |
| Flash | 64KB |
| SRAM | 20KB |
| 主频 | 72MHz (8MHz 晶振 + PLL) |
| 显示 | 0.96" OLED (SSD1306, 128x64, I2C) |
| 传感器 | MPU6050 (6轴 IMU) |
| 按键 | 3个按钮 (上/下/确认) |
| LED | 2个指示灯 |
| ADC | 电池电压监测 |

## 技术栈

- **编程语言**: C (ANSI C)
- **编译器**: ARMCC (Keil uVision 5)
- **固件库**: STM32 Standard Peripheral Library
- **架构**: 无操作系统 Bare-metal 架构

## 目录结构

```
STM32_Watch/
├── Hardware/          # 硬件驱动层
│   ├── OLED.c/h      # OLED显示驱动 (SSD1306)
│   ├── MyI2C.c/h    # 软件I2C主设备
│   ├── MPU6050.c/h  # 6轴传感器驱动
│   ├── Key.c/h      # 按键输入处理
│   ├── LED.c/h      # LED控制
│   ├── AD.c/h       # ADC电池监测
│   ├── menu.c/h    # 菜单系统
│   ├── dino.c/h     # 恐龙游戏
│   └── SetTime.c/h  # 时间设置
├── System/           # 系统模块
│   ├── Timer.c/h    # 定时器 (TIM2)
│   ├── MyRTC.c/h    # RTC实时时钟
│   └── Delay.c/h    # 延时函数
├── Library/          # STM32标准外设库
├── Start/            # 启动文件和系统配置
├── User/             # 用户应用入口
└── Project.uvprojx   # Keil工程文件
```

## 核心模块

### 硬件驱动层 (Hardware/)
- `OLED.c/h` - SSD1306 OLED显示驱动，通过I2C接口
- `MyI2C.c/h` - 软件模拟I2C主设备（bit-banging）
- `MPU6050.c/h` - MPU6050六轴传感器驱动（加速度计+陀螺仪）
- `Key.c/h` - 三按键输入，带消抖处理
- `LED.c/h` - LED状态指示灯控制
- `AD.c/h` - ADC电池电压监测
- `menu.c/h` - 菜单系统UI
- `dino.c/h` - 恐龙跑酷游戏
- `SetTime.c/h` - RTC时间设置界面

### 系统层 (System/)
- `Timer.c/h` - TIM2定时器，提供1ms系统tick
- `MyRTC.c/h` - RTC实时时钟功能
- `Delay.c/h` - 微秒/毫秒延时

## 功能特性

- [x] 时钟显示（实时时间+日期）
- [x] 电池电压监测
- [x] 菜单系统（带动画效果）
- [x] 计时器/秒表
- [x] 手电筒功能
- [x] MPU6050数据查看（加速度/陀螺仪）
- [x] 水平仪/气泡尺
- [x] 恐龙跑酷游戏
- [x] emoji表情显示
- [x] RTC时间设置

## 架构设计

```
┌────────────────────────────────────┐
│         应用层 (User)              │
│     main.c / menu.c / dino.c      │
├────────────────────────────────────┤
│       硬件抽象层 (Hardware)        │
│   OLED / Key / MPU6050 / LED      │
├────────────────────────────────────┤
│        系统层 (System)            │
│    Timer / MyRTC / Delay / AD     │
├────────────────────────────────────┤
│     STM32外设库 (Library)         │
│   GPIO / TIM / I2C / ADC / RCC    │
├────────────────────────────────────┤
│         核心层 (Start)            │
│   Cortex-M3 / NVIC / startup     │
└────────────────────────────────────┘
```

## 编程约定

- 命名：驼峰命名法（如 `MyRTC_Init`）
- 常量：全大写下划线分隔（如 `OLED_WIDTH`）
- 头文件保护：`#ifndef __XXX_H__`
- 外设定义：STM32F10x_MD (Medium Density)
- 编码：UTF-8

## 编译说明

1. 使用 Keil uVision 5 打开 `Project.uvprojx`
2. 选择目标芯片 `STM32F103C8`
3. 编译：`F7` 或 `Build > Build Target`
4. 下载：`F8` 或 `Flash > Download`

## 依赖关系

### 外设使用情况
| 外设 | 说明 |
|------|------|
| GPIO | 按键、LED、I2C引脚 |
| TIM2 | 系统tick定时器 |
| I2C2 | OLED、MPU6050通信 |
| ADC1 | 电池电压采样 |

### 引脚分配
- PB10 - OLED SCL
- PB11 - OLED SDA
- PB1 - 按键1（上）
- PA6 - 按键2（下）
- PA4 - 按键3（确认）
- PA0 - LED1
- PA1 - LED2
- PA2 - ADC电池监测

## 注意事项

1. 本项目使用标准外设库而非HAL库
2. 采用superloop主循环架构，无RTOS
3. 系统tick为1ms，由TIM2中断产生
4. I2C为软件模拟（bit-banging）
5. 游戏逻辑在1ms tick中运行

## 许可证

MIT License