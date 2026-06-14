# NucleiStudio 导入与全量评估复现步骤

本文记录如何从空 workspace 开始，在 NucleiStudio 中导入 `imu_gesture`，编译主机验证程序，并运行全量评估。

## 本质说明

`imu_gesture` 是 homework4 的主机侧算法验证工程，使用 Linux GCC 和项目自带 `Makefile` 构建。

NucleiStudio 在这里主要做三件事：

- 把 `/home/segzix/Projects/xinyuan/imu_gesture` 登记为 IDE 项目。
- 调用项目根目录的 `Makefile` 编译 `build/imu_gesture_host`。
- 通过 External Tools 调用 `make eval` 跑完整数据集评估。

注意区分：

- `imu_gesture`：主机算法验证工程，使用 Linux GCC。
- `VeriHealthi_QEMU_SDK_v3.7/qemu`：RISC-V/QEMU 固件工程，后续接入 SDK 时才使用。

平时评估 homework4 手势识别结果时，不需要构建 `qemu` 项目。

## 从空 workspace 导入项目

1. 启动 NucleiStudio。

2. workspace 选择独立目录，例如：

```text
/home/segzix/NucleiStudioWorkspace
```

不要选择下面这些源码目录作为 workspace：

```text
/home/segzix/Projects/xinyuan/imu_gesture
/home/segzix/Projects/xinyuan/VeriHealthi_QEMU_SDK_v3.7/qemu
```

3. 新建已有代码 Makefile 工程：

```text
File -> New -> Project...
-> C/C++ -> Makefile Project with Existing Code
```

4. 填写：

```text
Project Name: imu_gesture
Existing Code Location: /home/segzix/Projects/xinyuan/imu_gesture
Toolchain: Linux GCC
```

5. 点击：

```text
Finish
```

如果左侧没有 Project Explorer 面板，打开：

```text
Window -> Show View -> Project Explorer
```

如果菜单中没有该项，使用：

```text
Window -> Show View -> Other...
-> General -> Project Explorer
```

## 复用已有工程元数据

如果 `imu_gesture` 目录下已经保留 `.project` 和 `.cproject`，后续可以直接复用它们。

当前这两个文件的作用是让 NucleiStudio 识别 `imu_gesture` 为 C/C++ Makefile 工程：

- `.project`：保存 Eclipse 项目名称、C/C++ nature、Makefile nature 和 CDT builder。
- `.cproject`：保存 CDT C/C++ 项目的配置结构。

这种情况下可以使用导入已有工程：

```text
File -> Import...
-> General -> Existing Projects into Workspace
```

Root directory 选择：

```text
/home/segzix/Projects/xinyuan/imu_gesture
```

然后勾选 `imu_gesture`，不要勾选：

```text
Copy projects into workspace
```

如果 `.project` 或 `.cproject` 被删除，或者 `Build Project` 又变成灰色，就重新使用：

```text
File -> New -> Project...
-> C/C++ -> Makefile Project with Existing Code
```

注意：`imu_gesture_eval` 这种 External Tools 运行配置通常保存在 workspace 的 `.metadata` 里，不在项目目录里。换新 workspace 后，即使 `.project/.cproject` 还在，也可能需要重新配置一次全量评估。

## 构建主机程序

1. 关闭自动构建：

```text
Project -> Build Automatically
```

确保前面没有勾。

2. 在 Project Explorer 中单击选中 `imu_gesture` 项目。

3. 执行：

```text
Project -> Build Project
```

或右键 `imu_gesture`：

```text
Build Project
```

构建本质等价于：

```bash
cd /home/segzix/Projects/xinyuan/imu_gesture
make
```

成功后会生成：

```text
/home/segzix/Projects/xinyuan/imu_gesture/build/imu_gesture_host
```

如果 Console 输出：

```text
make: Nothing to be done for 'all'.
```

这不是错误，表示程序已经编译过，当前源码没有变化。

## 配置全量评估

全量评估不要点 `Build Project`，而是配置 External Tools 执行 `make eval`。

1. 打开：

```text
Run -> External Tools -> External Tools Configurations...
```

2. 左侧选择：

```text
Program
```

然后点击左上角新建按钮。

3. 填写：

```text
Name: imu_gesture_eval
Location: /usr/bin/make
Working Directory: /home/segzix/Projects/xinyuan/imu_gesture
Arguments: -C /home/segzix/Projects/xinyuan/imu_gesture eval
```

4. 点击：

```text
Apply -> Run
```

正确运行时，Console 中应该能看到 `make eval` 的评估输出，而不是 `make all`。

之后再次运行全量评估时，使用：

```text
Run -> External Tools -> imu_gesture_eval
```

也可以点顶部 External Tools 运行按钮旁边的小三角，选择 `imu_gesture_eval`。

## 终端等价命令

编译：

```bash
cd /home/segzix/Projects/xinyuan/imu_gesture
make
```

全量评估：

```bash
cd /home/segzix/Projects/xinyuan/imu_gesture
make eval
```

强制重新编译：

```bash
cd /home/segzix/Projects/xinyuan/imu_gesture
make -B
```

查看每个文件的详细运行输出：

```bash
cd /home/segzix/Projects/xinyuan/imu_gesture
make run-all
```

清理构建产物：

```bash
cd /home/segzix/Projects/xinyuan/imu_gesture
make clean
```

## eval 与 run-all 的区别

不需要先运行 `make run-all` 再运行 `make eval`。

- `make eval`：跑完整数据集并输出各类别汇总统计，适合看最终通过情况。
- `make run-all`：逐个运行所有 `.txt` 数据文件，输出每个文件的识别结果，适合排查具体样本。

推荐使用方式：

```text
平时看结果 -> make eval
结果异常 -> make run-all
```

## 单文件运行

如果要在 NucleiStudio 中运行单个数据文件：

1. 打开：

```text
Run -> Run Configurations...
```

2. 新建：

```text
C/C++ Application
```

3. 填写：

```text
Project: imu_gesture
C/C++ Application: build/imu_gesture_host
Working directory: ${workspace_loc:/imu_gesture}
```

4. 在 `Arguments` 页的 `Program arguments` 填入一个数据文件，例如：

```text
../VeriHealthi_IMU_Dataset/up/IMU_updown_right_2026_05_26_14_04_57_ID2.txt
```

5. 点击：

```text
Apply -> Run
```

等价终端命令：

```bash
cd /home/segzix/Projects/xinyuan/imu_gesture
./build/imu_gesture_host ../VeriHealthi_IMU_Dataset/up/IMU_updown_right_2026_05_26_14_04_57_ID2.txt
```

如果程序只输出：

```text
Usage: /home/segzix/Projects/xinyuan/imu_gesture/build/imu_gesture_host /path/to/imu_dataset.txt
```

说明程序已经启动成功，但没有传入数据文件参数。

## 常见问题

### Build Project 是灰色的

常见原因是项目不是以 C/C++ Makefile 工程导入，或者当前没有选中具体工程。

处理方式：

1. 在 Project Explorer 中单击选中 `imu_gesture` 项目。
2. 关闭 `Project -> Build Automatically`。
3. 如果 `.project/.cproject` 存在，先尝试重新导入已有工程：

```text
File -> Import...
-> General -> Existing Projects into Workspace
```

4. 如果仍然灰色，重新创建 Makefile 工程：

```text
File -> New -> Project...
-> C/C++ -> Makefile Project with Existing Code
```

不要复用旧的普通 Eclipse 项目元数据；只有当前包含 C/C++ nature 和 Makefile nature 的 `.project/.cproject` 才适合直接 Import。

### Run 之后还是 make all

如果 Console 里出现：

```text
Building in: .../build/make.run...
make -f ../../Makefile
make: Nothing to be done for 'all'.
```

说明当前触发的是 `Build Project`，不是全量评估。

全量评估必须从这里运行：

```text
Run -> External Tools -> imu_gesture_eval
```

### No rule to make target host/main_host.c

旧 Makefile 使用相对路径时，NucleiStudio 可能在临时构建目录执行 `make -f ../../Makefile`，导致找不到 `host/main_host.c`。

当前 Makefile 已经使用 `PROJECT_ROOT` 锚定到 Makefile 所在目录，正常不会再出现该问题。

如果再次出现，确认当前使用的是：

```text
/home/segzix/Projects/xinyuan/imu_gesture/Makefile
```

并在终端验证：

```bash
cd /home/segzix/Projects/xinyuan/imu_gesture
make -B
```

### 不要误构建 qemu

如果只是验证 homework4 的手势识别算法，不要操作：

```text
VeriHealthi_QEMU_SDK_v3.7/qemu
```

该工程用于后续生成 RISC-V/QEMU 固件，和当前 Linux 主机侧 `make eval` 不是同一条构建链路。
