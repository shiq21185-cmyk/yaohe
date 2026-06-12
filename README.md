# STM32 智能药盒项目

## 项目简介

这是一个基于 `STM32F103C8T6` 和 `FreeRTOS` 的智能药盒工程，当前代码集成了重量检测、温湿度采集、光敏状态检测、蓝牙时间同步、语音模块、舵机开合、OLED 显示以及 MQTT 上传功能。

工程更适合按“模块驱动 + 任务调度”的方式阅读：

- `Hardware/` 负责外设驱动和通信模块
- `User/` 负责业务逻辑、任务创建和状态管理
- `Systerm/` 负责延时、定时器、舵机等底层支持
- `FreeRTOS/` 提供任务调度与同步机制

## 主要功能

- HX711 读取药盒重量，支持去皮与 100g 标定换算
- DHT11 采集温度、湿度
- 光敏传感器检测开盖相关状态变化
- HC-06 蓝牙接收时间同步和 3 组闹钟配置
- 闹钟数据写入 STM32 Flash，掉电后可恢复
- OLED 显示温度、湿度、重量、时间和编辑状态
- 语音模块接收命令并驱动播报、开盖、调节音量等动作
- 舵机控制药盒开合
- ESP8266 连接 MQTT，上传温湿度、重量和状态数据

## 软件架构

### 启动流程

`User/main.c` 当前启动顺序大致如下：

1. 初始化 `LED`、`OLED`
2. 创建 OLED 与时间互斥锁
3. 初始化 `TIM4`
4. 初始化 `HC06`
5. 初始化 `USART1(9600)` 和 `USART2(115200)`
6. 初始化按键
7. 初始化 `ESP8266` 并连接 MQTT
8. 初始化 `HX711`
9. 执行去皮 `Get_Maopi()`
10. 首次读取 DHT11 与重量并刷新显示
11. 创建 `AppTaskCreate`，再由它统一创建各业务任务

### FreeRTOS 任务划分

当前 `User/app_tasks.c` 中创建了以下任务：

- `HX711_Task`
  负责重量采样、滤波、空盒检测、重量触发上传
- `Display_Task`
  负责 OLED 页面显示和编辑态刷新
- `DHT11_Task`
  周期读取温湿度并更新上传状态
- `Upload_Task`
  负责 MQTT 数据上报
- `Key_Task`
  负责按键扫描、页面切换和本地编辑逻辑
- `Time_Task`
  负责系统时间推进、蓝牙同步、闹钟检查、光敏相关处理
- `Servo_Task`
  负责执行开盖、关盖指令
- `Voice_Task`
  负责语音模块初始化、命令处理与播报触发

## 模块说明

### 重量检测

HX711 当前实现位于 `Hardware/hx711.c`，特点如下：

- `PB14` 为 `SCK`
- `PB15` 为 `DOUT`
- 开机后通过 `Get_Maopi()` 读取空载值作为去皮基准
- 使用 8 次采样，并做“去掉最大值和最小值后求平均”的简单滤波
- 当前标定常量为：
  - `HX711_CAL_WEIGHT_G = 100.0f`
  - `HX711_CAL_RAW_100G = 8493860UL`
- 重量换算公式为：

```c
weight = (raw - Weight_Maopi) * 100 / (HX711_CAL_RAW_100G - Weight_Maopi)
```

- 设有零点死区 `0.2g`
- 最大重量上限 `5000g`

如果更换了传感器、供电方式或机械结构，优先重新校准 `HX711_CAL_RAW_100G`，不要直接沿用当前常量。

### 蓝牙与闹钟

`Hardware/hc06.c` 实现了一个固定 3 字节协议：

- 时间同步：`[hour, minute, second]`
- 闹钟设置：`[24 + alarm_index, hour, minute]`

其中：

- `alarm_index` 范围为 `0~2`
- 共维护 3 组闹钟
- 闹钟数据会通过 `flash_storage.c` 存入 Flash 最后一页

Flash 存储位置定义在 `Hardware/flash_storage.h`：

- 起始地址：`0x0800FC00`
- 使用最后 1KB 页
- 数据结构包含 `magic`、3 组闹钟和 `checksum`

### 网络上传

`Hardware/esp8266.c` 中当前写死了 MQTT 服务器参数：

- Broker：`broker.hivemq.com`
- Port：`1883`

应用层定义的 MQTT 主题位于 `User/app_tasks.h`：

- `stm32/temperature`
- `stm32/humidity`
- `stm32/weight`
- `stm32/status`

如果后续接入自己的云平台，通常需要先修改：

- Wi-Fi 账号密码
- MQTT Broker 地址和端口
- 主题命名
- 上报数据格式

### 串口分配

当前工程的串口用途如下：

- `USART1`：HC-06 蓝牙，`PA9/PA10`
- `USART2`：ESP8266，`PA2/PA3`
- `USART3`：语音模块，`PB10/PB11`

### 按键与执行机构

- `Hardware/key.c/h`
  定义 4 个按键，当前映射为 `PB8`、`PB9`、`PB6`、`PB7`
- `Hardware/PWM.c/h`
  提供舵机 PWM 输出，注释中使用 `PB3 - TIM2_CH2`
- `Systerm/Servo.c/h`
  对舵机角度和脉宽进行上层封装

## 工程目录结构

```text
.
├── Hardware/                 硬件驱动与外设接口
│   ├── DHT11.c/h             温湿度传感器驱动
│   ├── esp8266.c/h           Wi-Fi 与 MQTT 驱动
│   ├── flash_storage.c/h     闹钟 Flash 存储
│   ├── hc06.c/h              蓝牙时间/闹钟同步
│   ├── hx711.c/h             重量传感器驱动
│   ├── key.c/h               按键驱动
│   ├── LED.c/h               LED 驱动
│   ├── light.c/h             光敏传感器驱动
│   ├── OLED.c/h              OLED 显示驱动
│   ├── OLED_Font.h           OLED 字库
│   ├── PWM.c/h               PWM 输出驱动
│   ├── usart.c/h             串口初始化与中断处理
│   └── xrvoice.c/h           语音模块驱动
├── User/                     应用层与 FreeRTOS 任务
│   ├── app_tasks.c/h         任务创建、状态管理、业务逻辑
│   ├── FreeRTOSConfig.h      FreeRTOS 配置
│   ├── main.c                主程序入口
│   ├── main.h                主程序头文件
│   ├── stm32f10x_conf.h      外设库配置
│   ├── stm32f10x_it.c/h      中断处理
│   └── voice_command_handler.c/h 语音命令业务处理
├── Systerm/                  底层支持模块
│   ├── Delay.c/h             微秒/毫秒延时
│   ├── Servo.c/h             舵机上层控制
│   ├── sys.c/h               系统相关配置
│   └── Timer.c/h             定时器初始化
├── FreeRTOS/                 FreeRTOS 内核源码
├── Library/                  STM32 标准外设库
├── Start/                    启动文件与启动配置
├── Objects/                  编译生成的目标文件
├── Listings/                 编译生成的列表文件
├── build/                    构建目录
├── DebugConfig/              调试配置
├── .eide/                    EIDE 工程配置
└── Poject.uvprojx            Keil MDK 工程文件
```

## 工程备注

平时主要打开的工程文件：

- `Poject.uvprojx`
- `Poject.uvoptx`
- `Poject.uvguix.*`

仓库里还保留了这些编辑器配置：

- `.eide/`
- `.vscode/`
- `.clang-format`
- `.clangd`

以后改参数时优先看这些文件：

- `Hardware/hx711.h`、`Hardware/hx711.c`
  HX711 引脚、去皮、标定常量
- `Hardware/usart.c`、`Hardware/xrvoice.c`
  HC-06、ESP8266、语音模块串口分配
- `Hardware/DHT11.c/h`、`Hardware/light.c/h`
  温湿度和光敏传感器
- `Hardware/PWM.h`、`Systerm/Servo.c/h`
  舵机 PWM 和角度控制
- `Hardware/esp8266.c`
  Wi-Fi、MQTT 参数

## 更新历程

### 2026-04-19
简介：调整重量整数方案与空盒阈值，完善项目基础说明文档。

- 重量处理采用整数方案，兼顾资源占用与显示稳定性
- 空盒触发阈值调整为 `1g`
- 空盒退出阈值调整为 `2g`，加入回差以减少误触发

### 2026-06-12
简介：重构 HX711 重量标定与小数显示、上传逻辑。

- 重量计算改为“空载去皮值 + 100g 标定值”的两点换算方式
- 当前参考标定参数为 `100g -> 8493860`
- 保留多次采样并去掉一个最大值、一个最小值的滤波方式
- OLED 重量显示调整为一位小数格式
- MQTT 上传的重量数据改为一位小数字符串
- `weight` 与 `Weight_Shiwu` 改为浮点重量，减少中间环节截断
- 增加 `flash_storage.c/h`，用于保存和恢复闹钟配置

### 2026-06-12
简介：重写 README，改为面向开发者的工程说明。

- 按当前代码整理模块职责、启动流程和 FreeRTOS 任务
- 补充串口分配、MQTT 主题、Flash 存储和 HX711 标定说明
- 重建工程目录结构说明，便于后续维护和二次开发
