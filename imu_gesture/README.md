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

## 项目原理

整体目标是把 50Hz 的 6 轴 IMU 原始数据转成题目要求的手势输出：`pinch`、`clench`、`up`、`down`，其它动作保持静默。当前工程把“读取数据、缓存分块、CRC 校验、算法判断、统一输出”拆开，方便同一套核心代码同时用于主机验证和后续 QEMU SDK 接入。

主机侧数据流如下：

```text
.txt 数据文件
    -> host/main_host.c 逐行解析 INT16
    -> ImuGyroAccelData(gx, gy, gz, ax, ay, az, debug)
    -> imu_dataset_runner_process_sample()
    -> CRC-8/SMBus 流式统计前 320000 Byte
    -> imu_buffer 双缓冲按 64 帧组块
    -> algo_manager_process()
    -> gesture_algo_process()
    -> 只输出非 other 手势
```

关键模块职责：

- `host/main_host.c`：主机验证入口，只负责打开 `.txt` 文件、跳过头部、把每 7 个 `INT16` 组装成一帧 IMU 样本。
- `src/imu_dataset_runner.c`：数据集运行器，负责逐样本推进、前 `320000B` CRC-8/SMBus 计算、缓冲满块后的算法调用，以及结束时 flush 剩余样本。
- `src/imu_buffer.c`：两个 64 样本块组成的双缓冲。采样侧写入，算法侧处理 ready block，处理后 release，减少大数组复制。
- `src/algo_manager.c`：算法输出边界。算法只返回枚举结果，manager 根据已处理字节数换算时间戳，并通过回调统一打印。
- `src/gesture_algo.c`：当前低资源整数规则基线。它维护滑动窗口，统计加速度变化、陀螺仪能量、峰值和方向变化，再用阈值区分四类目标手势。
- `embedded/imu_app_skeleton.c`：嵌入式接入骨架，展示 ISR 只发 data ready 信号，实际 HAL 读取、Event 通知和算法处理放在 task 上下文中。
- `embedded/qemu_const_runner.c`：QEMU SDK 验证入口，用编译进程序的 `const ImuGyroAccelData[]` 代替 `fopen` 文件读取。

几个重要约束：

- 单帧 `ImuGyroAccelData` 固定为 14 Byte，字段顺序是 `gx gy gz ax ay az debug`。
- 采样率固定按 `50Hz` 计算时间戳，输出时间来自已经处理的完整样本数。
- CRC 只覆盖输入流的前 `320000B`；如果单文件不足该长度，主机程序会打印 pending。
- 算法内部不打印，`other` 不输出，避免嵌入式和比赛输出格式被算法实现细节污染。
- 当前算法是规则基线，主要用于验证工程数据流和输出格式，不代表最终高准确率模型。

## 基本运行命令

下面所有命令都假设当前工作目录已经是 `homework4_imu_gesture`：

```bash
cd /home/segzix/Projects/xinyuan/homework4_imu_gesture
```

编译 host 验证程序：

```bash
make -B
```

当前默认数据集使用主办方手势数据集：

```text
../VeriHealthi_IMU_Dataset
```

该目录按 `pinch`、`clench`、`up`、`down`、`others` 分类存放数据，目录名就是当前 host 评估使用的标准标签。先找一个可运行样本：

```bash
find ../VeriHealthi_IMU_Dataset -type f -name '*.txt' | head
```

运行单个 IMU 文本文件：

```bash
make run DATA=../VeriHealthi_IMU_Dataset/pinch/IMU_pinch_right_2026_05_29_14_20_10_ID5.txt
```

一条命令编译并跑完整正式手势数据集：

```bash
make run-all
```

`make run-all` 默认使用 `../VeriHealthi_IMU_Dataset`。如果要临时运行其它数据目录，可以覆盖 `DATA_DIR`：

```bash
make run-all DATA_DIR=../VeriHealthi_Algorithm_Homework_Code_Data/AccData
```

不用 Makefile 时，也可以用下面这一条命令批量跑完整正式手势数据集，查看每个文件的识别输出：

```bash
make -B && find ../VeriHealthi_IMU_Dataset -type f -name '*.txt' | sort | while IFS= read -r f; do printf '\n== %s ==\n' "$f"; build/imu_gesture_host "$f"; done
```

和标准目录标签做统计比较：

```bash
make eval
```

`make eval` 的统计口径：

- `pinch/clench/up/down`：该类别文件中至少输出一次同名手势，算 `correct_files`；没有输出同名手势，算 `miss_or_fp_files`。
- `others`：没有任何 `pinch/clench/up/down` 输出，算 `correct_files`；只要输出任意手势，算 `miss_or_fp_files`。
- `wrong_files`：文件里出现了不符合目录标签的手势输出。对 `others` 来说，任何手势输出都是 wrong。
- `wrong_lines`：不符合目录标签的手势输出总行数。
- `accepted_pair_files` / `accepted_pair_lines`：仅用于 `up/down`。正式数据集中 `up` 和 `down` 目录有大量同名且内容相同的 `IMU_updown_*.txt` 文件，单个文件可能同时包含抬腕和放下动作；因此 `make eval` 会把 `up` 文件中的 `down`、`down` 文件中的 `up` 单独统计为配对动作，不计入 `wrong_*`。

如果要把旧的 `AccData` 当作非手势负样本检查误报，可以覆盖数据目录：

```bash
make run-all DATA_DIR=../VeriHealthi_Algorithm_Homework_Code_Data/AccData
```

旧 `AccData` 只有 `walk/run/others` 标签，不能完整评估四类手势准确率；它更适合用来观察非手势数据是否误触发 `pinch/clench/up/down`。

如果你的数据是直接放在 `homework4_imu_gesture/AccData/*.txt`，使用这个批量命令：

```bash
make run-all DATA_DIR=AccData
```

清理主机构建产物：

```bash
make clean
```

主机解析器兼容现有 `homework3` 风格文本：前若干行是头部，出现 `TYPE` 后按每行一个整数读取，每 7 个值组成一个 `ImuGyroAccelData`：`gx gy gz ax ay az debug`。

同样兼容 `VeriHealthi_IMU_Dataset/**/*.txt` 的 202606 初赛数据格式：前 5 行元数据，`TYPE = 6-AXIS IMU RAW DATA` 后每 7 个 `INT16` 值组成一帧，采样率 50Hz。

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
