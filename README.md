# VeriHealthi — 可穿戴健康设备嵌入式 SDK 与算法作业

> **VESC2026（VeriSilicon Embedded System Contest 2026）课程项目**
>
> 作者：盛子轩 &nbsp;|&nbsp; 日期：2026-05-12

---

## 项目简介

本项目由两大部分组成：

| 部分 | 说明 |
|------|------|
| **VeriHealthi QEMU SDK v3.6** | 基于 RISC-V（Nuclei N309）的可穿戴健康 SoC 嵌入式 SDK，含 FreeRTOS、BSP/驱动、BLE 协议栈与 QEMU 仿真环境 |
| **算法作业（Homework 3）** | 改进计步算法（C + Python）+ BP 神经网络二分类示例（C + Python），含 80 文件 IMU 数据集 |

---

## 目录结构

```
xinyuan/
├── VeriHealthi_QEMU_SDK_v3.6/          # QEMU 嵌入式 SDK
│   ├── build.sh                         #   交叉编译脚本（RISC-V 32）
│   ├── qemu.sh                          #   QEMU 仿真启动脚本
│   ├── build/qemu.out                   #   预编译 ELF 固件
│   ├── qemu/                            #   Eclipse CDT 调试配置
│   └── galaxy_sdk/                      #   SDK 源码
│       ├── main.c                       #     应用入口（FreeRTOS 任务示例）
│       ├── n309_iot_qemu.ld             #     链接脚本
│       ├── config/include/              #     功能配置 & FreeRTOSConfig.h
│       ├── bsp/                         #     Board Support Package
│       ├── drivers/                     #     设备驱动 HAL（UART、模拟 IMU 传感器…）
│       ├── os/                          #     FreeRTOS 内核
│       ├── osal/                        #     OS 抽象层
│       └── modules/                     #     公共模块 & NMSIS DSP 库
├── VeriHealthi_Algorithm_Homework_Code_Data/
│   ├── AccData/                         # 6 轴 IMU 数据集（80 文件）
│   │   ├── walk/                        #   走路数据（12 文件，20-100 步/文件）
│   │   ├── run/                         #   跑步数据（12 文件，20-100 步/文件）
│   │   └── others/                      #   噪声/干扰数据（56 文件，0 步/文件）
│   ├── StepCounter/                     # 计步算法
│   │   ├── C/                           #   C 语言实现（改进版 + 原始版）
│   │   └── Python/                      #   Python 仿真实现
│   └── NN/                              # BP 神经网络二分类
│       ├── C/                           #   C 语言推理实现
│       └── Python/                      #   PyTorch 训练 + 仿真
├── Docs/
│   └── 计步算法设计与测试报告.md          # 改进计步算法详细报告
├── VeriHealthi QEMU SDK发布说明 20260510.pdf
└── VeriHealthi Embedded SDK开发手册-VESC2026-V1.0.pdf
```

---

## 技术栈

| 层次 | 技术 |
|------|------|
| **目标 CPU** | RISC-V RV32IMAFc + XXLDSP 扩展（Nuclei N309） |
| **RTOS** | FreeRTOS（Nuclei RISC-V 移植版） |
| **仿真** | `qemu-system-riscv32`, machine `nuclei_evalsoc` |
| **编译工具链** | `riscv64-unknown-elf-gcc/g++`（Nuclei RISC-V GCC） |
| **DSP 加速** | NMSIS DSP 库（RISC-V 向量/DSP 指令） |
| **算法开发** | C99（单精度浮点）+ Python 3.10（NumPy / Pandas / PyTorch / Matplotlib） |

---

## 快速开始

### 1. 构建 & 运行嵌入式 SDK

**前置条件**：已安装 Nuclei RISC-V 工具链（`riscv64-unknown-elf-gcc`）与 `qemu-system-riscv32`。

```bash
# 编译固件
cd VeriHealthi_QEMU_SDK_v3.6
bash build.sh

# 启动 QEMU 仿真（GDB 监听 :1234）
bash qemu.sh
```

固件启动后，通过 UART 输出 `"Hello VeriHealthi!"` 并执行 10 秒的示例任务计数。

### 2. 运行计步算法

```bash
# Python 仿真（自动测试全部 80 个数据文件）
cd VeriHealthi_Algorithm_Homework_Code_Data
python3 StepCounter/Python/step_counter_improved.py

# C 语言版本
gcc -Wall -O2 -o /tmp/step_test \
  StepCounter/C/alg_step_counter_improved.c \
  StepCounter/C/main_improved.c -lm
/tmp/step_test
```

### 3. 运行 BP 神经网络

```bash
cd VeriHealthi_Algorithm_Homework_Code_Data

# Python 训练 + 推理
python3 NN/Python/bp_networks_simulation.py

# C 语言推理
gcc -o /tmp/bp_test NN/C/alg_bp_networks.c NN/C/bp_classify_main.c -lm
/tmp/bp_test
```

---

## 改进计步算法要点

| 环节 | 策略 | 效果 |
|------|------|------|
| 加速度模值 | 合成三轴加速度 `|a| = √(ax²+ay²+az²)` | 方向无关 |
| 信号滤波 | DC 去除 + 滑动平均滤波器 | 消除基线漂移与高频噪声 |
| 峰值检测 | 自适应阈值 + 邻近峰值合并 | 减少重计数 |
| 活动分类 | 陀螺仪模值 4 级分类（静止/微动/行走/剧烈运动） | 区分走路与日常噪声 |
| 周期性验证 | 峰值间隔约束（250ms~1500ms） | 剔除无规律抖动 |

**测试结果（80 文件数据集）**：

| 指标 | 值 |
|------|----|
| 整体 MAE | **7.38 步** |
| 整体 MAPE | **17.02%** |
| 噪声数据误检率 | **16.1%**（较原始算法降低 84%） |

详细内容见 [`Docs/计步算法设计与测试报告.md`](Docs/计步算法设计与测试报告.md)。

---

## 硬件配置（QEMU 仿真）

| 参数 | 值 |
|------|----|
| Flash | 3 MB @ `0x20000000` |
| RAM | 256 KB @ `0x90000000` |
| 堆栈 | 2 KB / 20 KB 堆 |
| FreeRTOS Tick | 100 Hz |
| 设备名 | `VHR-C0C0` |

---

## 参考环境

- GCC 12.2.0
- Python 3.10.5
- NumPy 1.26.4, Pandas 2.3.3, PyTorch 2.3.1+cpu, Matplotlib 3.10.9
- Nuclei RISC-V GCC Toolchain
- QEMU 6.0+（riscv32-softmmu）
