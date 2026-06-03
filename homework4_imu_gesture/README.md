# IMU 手势识别最小工程骨架

本目录用于承接题目要求的 `imu_task`、`algo_task`、IMU 原始数据缓存、CRC-8/SMBus 校验和手势识别算法验证。当前版本是最小可运行骨架，优先保证模块边界、安全约束和后续接入 VeriHealthi SDK 的位置清晰。

## 目录结构

```text
homework4_imu_gesture/
├── embedded/                 # VeriHealthi SDK 接入骨架
│   ├── imu_app_skeleton.c    # imu_task/algo_task/Event/ISR 约束示例
│   └── qemu_const_runner.c   # QEMU SDK const 数据入口，不依赖 fopen
├── host/                     # 主机侧数据集验证入口
│   └── main_host.c
├── include/                  # 公共头文件
│   ├── algo_manager.h
│   ├── crc8_smbus.h
│   ├── gesture_algo.h
│   ├── imu_dataset_runner.h
│   ├── imu_qemu_const_runner.h
│   ├── imu_buffer.h
│   └── imu_gesture_types.h
├── src/                      # 可复用 C 实现
│   ├── algo_manager.c
│   ├── crc8_smbus.c
│   ├── gesture_algo.c
│   ├── imu_dataset_runner.c
│   └── imu_buffer.c
└── Makefile                  # 主机侧最小验证构建
```

## 设计约束映射

| 题目要求 | 当前骨架位置 |
|---|---|
| 创建 `imu_task` 读取 IMU | `embedded/imu_app_skeleton.c` |
| 创建 `algo_task` 处理 IMU 数据 | `embedded/imu_app_skeleton.c` |
| IMU 采样率 50Hz | `IMU_GESTURE_SAMPLE_RATE_HZ` |
| ISR 只触发 data ready 信号 | `imu_data_ready_isr()`，不调用 HAL/Event/printf/CRC |
| 足量数据后 Event 通知算法 | `imu_task()` 调用 `vpi_event_notify(EVENT_SEN_DATA_READY, ...)` |
| 首次 320000Byte CRC | `crc8_smbus_update()`，在 task 上下文计算 |
| 算法 manager 统一打印 | `algo_manager_process()` 通过回调输出结果 |
| 算法内部禁止打印 | `gesture_algo.c` 不包含打印接口 |
| 其它动作不打印 | manager 仅对有效手势触发回调 |
| QEMU SDK 不支持 `fopen` | `embedded/qemu_const_runner.c` 处理 `const ImuGyroAccelData[]` |
| RAM 仅 256K | runner 逐样本流式处理，测试数据应声明为 `const` |

## 主机验证

编译：

```bash
cd homework4_imu_gesture
make
```

运行单个 IMU 文本文件：

```bash
make run DATA=../homework3/AccData/walk/example.txt
```

或者直接：

```bash
./build/imu_gesture_host /path/to/imu_dataset.txt
```

主机解析器兼容现有 `homework3` 风格文本：前若干行是头部，出现 `TYPE` 后按每行一个整数读取，每 7 个值组成一个 `ImuGyroAccelData`：`gx gy gz ax ay az debug`。

同样兼容 `VeriHealthi_QEMU_SDK.202606_preliminary/VeriHealthi_IMU_Dataset/**/*.txt` 的 202606 初赛数据格式：前 5 行元数据，`TYPE = 6-AXIS IMU RAW DATA` 后每 7 个 `INT16` 值组成一帧，采样率 50Hz。

## QEMU SDK 集成要点

根据 `VeriHealthi_QEMU_SDK.202606_preliminary_analysis.md`：

- QEMU SDK 不支持 `fopen`，不要在嵌入式工程中读取文本文件。
- 将用于 QEMU 验证的数据转换为 `const ImuGyroAccelData samples[]`，编译进工程，只把少量测试数据放入 RAM 路径。
- 在 SDK app task 中包含 `imu_qemu_const_runner.h` 并调用 `imu_gesture_run_const_dataset(samples, sample_count)`，该入口位于 `embedded/qemu_const_runner.c`。
- 输出格式必须保持 `xxxms, pinch`、`xxxms, clench`、`xxxms, up` 或 `xxxms, down`；`others` 不输出。
- 算法和 runner 使用整数运算，不依赖 `double` 和动态分配，便于适配 256K RAM 的 QEMU 环境。

## 后续接入 SDK

`embedded/imu_app_skeleton.c` 中所有 `TODO SDK HAL` 都是需要根据初赛 SDK IMU 章节替换的接口点，包括：

- IMU 设备获取、初始化和配置 50Hz 采样率，可参考 SDK v3.7 的 `hal_imu_get_device()`、`hal_imu_init()`、`hal_imu_set_accel_cfg()`、`hal_imu_set_gyro_cfg()` 和 `hal_imu_set_work_mode()`。
- IMU data ready 中断注册。
- IMU data ready 中断注册可参考 `hal_imu_cfg_interrupt()` 和 `hal_imu_enable_interrupt()`。
- 从 HAL 读取完整 `ImuGyroAccelData` 结构体可参考 `hal_imu_read_gyro_accel()`。
- 将该文件纳入 SDK 构建脚本并链接本目录 `src/*.c`。

## 当前算法状态

当前 `gesture_algo.c` 是低资源规则基线，用滑动窗口统计加速度变化、陀螺仪能量和垂直方向变化，能验证完整数据流和输出格式，但不是最终竞赛模型。后续应基于 `VeriHealthi_IMU_Dataset.zip` 统计各手势阈值，或替换为小型决策树/模板匹配模型。
