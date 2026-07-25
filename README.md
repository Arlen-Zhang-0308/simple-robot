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
- DisplayTask：33 ms 周期调用 Display Stub。
- PING / ACK、GET_STATUS、GET_POWER、Motion 状态命令和 CRC8。
- CRC 或长度错误返回 NACK；未知命令、错误 Payload、禁用模块和欠压使用统一错误响应。
- DRV8833 实际输出：PB6→AIN1、PB7→AIN2、PB8→BIN1、PB9→BIN2，TIM4 运行于 10 kHz。
- OLED Stub 与模拟电压检测。

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
text 16788 bytes
data    16 bytes
bss  13368 bytes
```

## 当前边界

- 尚未连接真实板卡烧录或运行验证。
- 当前转向采用左右轮反向的原地转向；若实物电机方向相反，交换对应电机两根输入定义或电机接线。
- 前进、后退和转向命令必须在 500 ms 内续发；超时后状态切换为停止并清零四路 PWM。
- TIM4 四个通道已用于电机，不能同时用于四路舵机。
- UART 接收溢出当前只丢弃新字节，后续可增加错误计数或事件通知。
- nRF24L01、蓝牙和 Wi-Fi 尚未接入真实驱动；链路加密也尚未实现。
- RobotState 当前按首轮最小骨架直接共享，后续接入真实硬件与更高并发后再增加临界区。
- ADC 与 SPI1 已由 HAL 初始化，传感器和显示仍使用模拟/Stub；TIM4 已用于真实电机 PWM。
- 不要使用 STM32CubeMX Generate Code 覆盖当前工程；外设和引脚调整直接修改现有代码与构建配置。
