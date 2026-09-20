# BotCockpit — ROS2 机器人调试上位机

> 基于 Qt Quick/QML 的 ROS2 机器人调试工具：看状态、发指令、管故障。

**一句话介绍（简历/面试统一口径）**  
连接 ROS2 机器人后，实时查看节点与诊断状态，下发任务与软急停，处理故障复位的桌面调试工具。

## 产品定位

- **是**：实验室 / 联调现场用的桌面工具（轻量 Foxglove + 工业上位机信息密度）
- **不是**：车队调度中台、WMS、算法竞赛项目、公司产品复刻
- **主路径**：ROS2 仿真假车（`fake_robot`），不依赖实车与公司硬件

## 技术栈

| 层 | 选型 |
|----|------|
| UI | Qt6 Quick / QML |
| 客户端后端 | C++（TCP、协议编解码、Model） |
| 机器人侧 | ROS2 Humble（rclcpp） |
| 通信 | TCP + 自定义帧 + JSON payload（见 `docs/PROTOCOL.md`） |
| 测试 | 手动验收 + `tools/fake_client.py` + `launch_testing`（第 2 周起） |

## 仓库结构

```text
Bot_Project/
├── README.md
├── docs/
│   ├── PLAN.md            # 四周计划
│   ├── PROTOCOL.md        # 通信协议（实现不得擅自改语义）
│   ├── WEEK1.md           # 第 1 周任务卡与验收
│   └── ACCEPTANCE.md      # 周验收记录（边测边填）
├── bridge/                # ROS2 包 botcockpit_bridge（TCP ↔ ROS2）
├── sim/                   # ROS2 包 botcockpit_sim（fake_robot）
├── src/                   # 可选：C++ 公共库 / 后续拆包
├── ui/
│   └── botcockpit/        # Qt6 QML 应用
├── tests/
└── tools/
    ├── fake_client.py     # 无 UI 协议客户端
    └── mock_bridge.py     # 无 ROS2 时的协议冒烟 mock（非正式组件）
```

实际 ROS2 工作空间可放在 `ros2_ws/`（安装系统后创建），本仓库以文档 + 源码目录约定为准。

## 快速开始（环境就绪后）

```bash
# 0) Ubuntu 22.04 + ROS2 Humble + Qt6；将 bridge/ 与 sim/ 链入工作空间
mkdir -p ~/botcockpit_ws/src
ln -s /path/to/Bot_Project/bridge ~/botcockpit_ws/src/botcockpit_bridge
ln -s /path/to/Bot_Project/sim    ~/botcockpit_ws/src/botcockpit_sim
cd ~/botcockpit_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select botcockpit_bridge botcockpit_sim
source install/setup.bash

# 1) 机器人侧（两个终端）
ros2 run botcockpit_sim fake_robot
# lifecycle: configure + activate
ros2 lifecycle set /fake_robot configure
ros2 lifecycle set /fake_robot activate
ros2 run botcockpit_bridge bridge --ros-args  # 或 --port 8765
# bridge 可执行文件名：bridge；端口默认 8765（也可用 --port）

# 2) 假客户端冒烟
python3 tools/fake_client.py --host 127.0.0.1 --port 8765
python3 tools/fake_client.py --self-test
python3 tools/fake_client.py --send-sticky --duration 2

# 3) QML 终端
cd ui/botcockpit && cmake -B build && cmake --build build && ./build/botcockpit
```

**无 ROS2 的协议冒烟（Windows/Linux 均可）**

```bash
python tools/mock_bridge.py --port 8765
python tools/fake_client.py --host 127.0.0.1 --port 8765 --duration 3
```

`mock_bridge.py` 仅用于开发期协议联调，正式主路径仍是 `fake_robot + botcockpit_bridge`。

## 四页工作流（QML）

1. **Connect** — IP/端口、连接/断开、心跳 RTT  
2. **Dashboard** — 模式/相位、节点健康、关键状态  
3. **Control** — 模式切换、任务下发、暂停/继续、软急停、故障复位  
4. **Fault** — 故障码列表、简单日志  

**QML 约束**：UI 只绑定 C++ 的 `RobotState` / Model，不在 QML 里写协议业务。

## 快速开始（环境就绪后）

```bash
# 1) ROS2 侧（示例，按实际包名调整）
source /opt/ros/humble/setup.bash
ros2 run botcockpit_sim fake_robot
ros2 run botcockpit_bridge bridge --port 8765

# 2) 假客户端冒烟
python3 tools/fake_client.py --host 127.0.0.1 --port 8765

# 3) QML 终端
cd ui/botcockpit && cmake -B build && cmake --build build && ./build/botcockpit
```

## 文档

- 计划：[`docs/PLAN.md`](docs/PLAN.md)  
- 协议：[`docs/PROTOCOL.md`](docs/PROTOCOL.md)  
- 第 1 周：[`docs/WEEK1.md`](docs/WEEK1.md)  

## 已实现范围（第 1 周代码）

| 模块 | 路径 | 说明 |
|------|------|------|
| fake_robot | `sim/` | Lifecycle 节点，状态发布 / goto / estop / reset / 故障注入 |
| bridge | `bridge/` | TCP 8765，HELLO/心跳/SNAPSHOT/DELTA，CMD 转发与 ACK |
| QML | `ui/botcockpit/` | Connect + Dashboard；C++ 线程收发，QML 只绑定 Model |
| 工具 | `tools/fake_client.py` | 协议冒烟；`--self-test` / `--send-sticky` |
| 工具 | `tools/mock_bridge.py` | 无 ROS2 时的本地协议 mock |

## 边界与合规

- 不复制公司 UI 资源、代码、协议实现  
- 对外描述使用「工业 AMR 调试终端通用交互模式」  
- 仿真数据与开源依赖即可，勿接入公司现场数据  

## 命名说明

曾用工作名 Pitwall；现正式名 **BotCockpit**，对外副标题：  
**基于 Qt Quick/QML 的 ROS2 机器人调试上位机**
