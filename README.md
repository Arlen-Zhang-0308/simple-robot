# Simple Robot CubeMX Integrated Firmware

以代码或用户需求为设计源，本工程由 STM32CubeMX 6.18.0 + STM32Cube FW_F1 V1.8.7 生成，并接入 simple-robot 第一轮固件骨架。

## 目标平台

- MCU：STM32F103C8T6，Cortex-M3。
- 时钟：外部 8 MHz HSE，PLL x9，系统时钟 72 MHz。
- 工程：Makefile + STM32 HAL + FreeRTOS CMSIS-RTOS V1。
- HAL 时基：TIM1；FreeRTOS 使用 SysTick。

## 已接入

- CommTask：通过统一传输层接收命令；当前 UART 使用 USART1 中断逐字节接收，阻塞式发送响应。
- 通信系统：UART、nRF24L01、蓝牙和 Wi-Fi 使用统一协议接口，各链路独立缓冲和解析，响应沿原链路返回。
- 第一版仅启用无加密 UART；其他传输方式保留类型、配置和接入接口。
- MotionTask：20 ms 周期把运动状态写入 DRV8833 四路 PWM，并执行欠压、急停和 500 ms 通信失联停车。
- SensorTask：100 ms 周期发布模拟 5V 输入状态。
- DisplayTask：33 ms 周期读取 RobotState，并通过 SPI1 刷新 128×64 SSD1306 OLED。
- 上电后以 20 FPS 播放一轮 240 帧抽象机械生物变形动画，约 12 秒；动画由像素基元实时绘制，不占用帧表空间。
- 机器人停止、速度为 0 且无低电量或错误持续 3 秒后进入待机动画并循环；运动、低电量或错误状态会立即恢复信息页。
- 显示页 0：设备名称、输入电压、低电量状态、运动状态和最近错误码。
- 显示页 1：运动方向、速度和驱动就绪/低电量锁定状态；其他页码回退到状态页。
- PING / ACK、GET_STATUS、GET_POWER、Motion 状态命令和 CRC8。
- CRC 或长度错误返回 NACK；未知命令、错误 Payload、禁用模块和欠压使用统一错误响应。
- DRV8833 实际输出：PB6→AIN1、PB7→AIN2、PB8→BIN1、PB9→BIN2，TIM4 运行于 10 kHz。
- 7 针 SPI OLED：D0→PA5、D1→PA7、RES→PB0、DC→PB1、CS→GND；PB2 / BOOT1 不再使用。
- 四路舵机预留：TIM2 部分重映射，PA15 / PB3 / PA2 / PA3。
- 蓝牙预留：USART3，PB10 / PB11；Wi-Fi 预留：USART2，PA2 / PA3，PB15 为 EN。
- SSD1306 SPI OLED 显示与模拟电压检测；显示驱动仍保留 Stub 配置用于脱机测试。

任务优先级：

```text
CommTask / MotionTask  AboveNormal
SensorTask             Normal
DisplayTask            BelowNormal
```

## 构建

本项目后续直接维护现有 C/H、HAL MSP 和 Makefile，不再使用 `.ioc` 重新生成代码。`simple-robot.ioc` 仅保留历史参考，不代表当前电机配置。

已验证工具链：

```text
arm-none-eabi-gcc 14.2.1
GNU binutils 2.44
newlib 4.5.0
GNU Make 4.4.1
```

执行：

```bash
make clean
make -j2
```

产物：

```text
build/simple-robot.elf
build/simple-robot.hex
build/simple-robot.bin
```

最终构建内存占用：

```text
text 21924 bytes
data    16 bytes
bss  14392 bytes
```

## 当前边界

- 尚未连接真实板卡烧录或运行验证。
- 启动/待机动画已使用 ARM GCC 14.2.1 重新构建通过；生成的 `.elf`、`.hex` 和 `.bin` 已使用 pyOCD 在 STM32F103C8T6 实机完成烧录验证。
- 当前转向采用左右轮反向的原地转向；若实物电机方向相反，交换对应电机两根输入定义或电机接线。
- 前进、后退和转向命令必须在 500 ms 内续发；超时后状态切换为停止并清零四路 PWM。
- TIM4 四个通道用于电机；四路舵机已独立分配到 TIM2。Servo3/4 与 USART2 Wi-Fi 共用 PA2/PA3，默认优先使用舵机。
- UART 接收溢出当前只丢弃新字节，后续可增加错误计数或事件通知。
- nRF24L01、蓝牙和 Wi-Fi 尚未接入真实驱动；链路加密也尚未实现。
- OLED CS 当前固定接地，OLED 始终处于选中状态，因此当前接法下不能同时启用 nRF24L01 SPI 通信。
- 实物 OLED 已确认为 128×64 SSD1306 四线 SPI 模块，当前驱动与控制器型号一致。
- RobotState 当前按首轮最小骨架直接共享，后续接入真实硬件与更高并发后再增加临界区。
- ADC 与 SPI1 已由 HAL 初始化；传感器仍使用模拟值，显示已接入真实 SPI 驱动，TIM4 已用于真实电机 PWM。
- 不要使用 STM32CubeMX Generate Code 覆盖当前工程；外设和引脚调整直接修改现有代码与构建配置。
