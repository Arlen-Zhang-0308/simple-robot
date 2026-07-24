# Simple Robot CubeMX Integrated Firmware

以代码或用户需求为设计源，本工程由 STM32CubeMX 6.18.0 + STM32Cube FW_F1 V1.8.7 生成，并接入 simple-robot 第一轮固件骨架。

## 目标平台

- MCU：STM32F103C8T6，Cortex-M3。
- 时钟：外部 8 MHz HSE，PLL x9，系统时钟 72 MHz。
- 工程：Makefile + STM32 HAL + FreeRTOS CMSIS-RTOS V1。
- HAL 时基：TIM1；FreeRTOS 使用 SysTick。

## 已接入

- CommTask：USART1 中断逐字节接收，ring buffer 增量解析，阻塞式发送响应。
- MotionTask：20 ms 周期执行状态机安全检查。
- SensorTask：100 ms 周期发布模拟 5V 输入状态。
- DisplayTask：33 ms 周期调用 Display Stub。
- PING / ACK、GET_STATUS、GET_POWER、Motion 状态命令和 CRC8。
- OLED Stub、模拟电压、Motion 状态机占位。

任务优先级：

```text
CommTask / MotionTask  AboveNormal
SensorTask             Normal
DisplayTask            BelowNormal
```

## 构建

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
text 15704 bytes
data    16 bytes
bss  12800 bytes
```

## 当前边界

- 尚未连接真实板卡烧录或运行验证。
- UART 接收溢出当前只丢弃新字节，后续可增加错误计数或事件通知。
- RobotState 当前按首轮最小骨架直接共享，后续接入真实硬件与更高并发后再增加临界区。
- ADC、TIM4 PWM、SPI1 已由 HAL 初始化，但第一轮实现仍使用模拟传感器、状态机占位和 Display Stub。
- 若重新从 CubeMX 生成代码，应保留 USER CODE 区，并确认 Makefile 仍包含 App、Config、Driver、Service 源码和 include 路径。
