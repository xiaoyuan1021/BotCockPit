# BotCockpit — 第 1 周任务卡

- **项目路径**：`D:\xiaoyuan\Bot_Project`
- **产品**：BotCockpit — ROS2 机器人调试上位机（Qt6 Quick/QML）
- **协议**：必须遵守 [`PROTOCOL.md`](./PROTOCOL.md) v0.1，禁止擅自改语义
- **协作模式**：AI 实现 / 用户验收；一项不过不进新功能
- **主路径**：仿真假车，不使用公司硬件与代码

---

## 环境准备（用户，约 0.5–1 天）

> 在 Ubuntu 22.04 上开发（推荐双系统）。WSL2 可作备选；Windows 原生 ROS2 不作为主路径。

- [ ] 安装 ROS2 Humble  
- [ ] 安装 Qt6（含 Quick/QML、CMake）  
- [ ] 安装 colcon、git  
- [ ] 本仓库可 clone；创建 ROS2 工作空间（例如 `~/botcockpit_ws`）  
- [ ] 验证：`ros2 topic list`、`qmake6 -v` 或 `cmake` 能找到 Qt6  

初始化示例：

```bash
cd ~/botcockpit_ws
# 将本仓库 src 以包形式放入 src/ 或软链接
git init  # 若尚未在 D:\xiaoyuan\Bot_Project 则在 Ubuntu 侧同步该目录
```

说明：若在 Windows 目录 `D:\xiaoyuan\Bot_Project` 编辑文档、在 Ubuntu 编译代码，两边保持同一 git 远程或同步文件夹。

---

## 给 AI 的统一提示词

```text
项目：BotCockpit
路径：D:\xiaoyuan\Bot_Project（或 Linux 同步路径）
依据：docs/PROTOCOL.md v0.1（禁止修改协议语义）
本周：只做第 1 周任务卡中的实现项，不做地图/导航/参数热更
约束：
- QML 不写协议业务；TCP 与编解码在 C++
- 可编译/可运行；每个任务给出「3 步手动验证」
- fake_robot 用 ROS2 Humble + rclcpp；工具脚本用 Python3
- 代码与日志使用英文标识符；注释可中英
禁止：
- 公司代码/UI 资源
- 无关重构
- 验收未过时新增功能
```

---

## 任务 1.2 工程骨架（AI）

**范围**
- 创建 ROS2 包：`botcockpit_sim`（或 `botcockpit_msgs`+`botcockpit_sim`）、`botcockpit_bridge`
- 创建 Qt 工程：`ui/botcockpit`（Qt6 Quick + C++，CMake）
- 创建 `tools/fake_client.py`
- 根目录/包内可 `colcon build`（sim+bridge）；QML 应用可 `cmake && build`

**接口约定**
- bridge 默认监听 `0.0.0.0:8765`
- 包名与 README 一致，避免再发明第三套名字

**手动验证**
1. `colcon build --packages-select botcockpit_bridge botcockpit_sim` 成功  
2. QML 工程 configure+build 成功，可启动空白窗口  
3. 目录结构与 README「仓库结构」一致  

---

## 任务 1.3 fake_robot 仿真节点（AI）

**范围**
- ROS2 Lifecycle 节点 `fake_robot`
- `configure` 后可发布状态；`activate` 后模拟：
  - `pose` 缓慢变化（例如 x 以 0.1 m/s 递增后循环）
  - `battery` 缓降
- 状态字段至少覆盖 PROTOCOL STATE 中：`mode/phase/pose/battery/nodes/faults/estop/control_enabled`
- 提供简易指令接口（Service 或 Topic + bridge 适配）：
  - 模式设置
  - `goto` 任务（仿真运动即可）
  - `estop` / `reset`
  - 故障注入（例如让 `fake_robot` 自身 ERROR）

**第 1 周可简化状态机**：`BOOT → INITIALIZING → IDLE → RUNNING(goto) → IDLE`；ESTOP 可先实现为标志位，完整 FAULT 规则放到第 2 周。

**手动验证**
1. `ros2 run botcockpit_sim fake_robot` 可启动，lifecycle 可 configure/activate  
2. `ros2 topic echo`（或 bridge 日志）能看到 pose/battery 变化  
3. 注入故障后状态中 `nodes[].status` 出现 `ERROR`  

---

## 任务 1.4 botcockpit_bridge（AI）

**范围**
- TCP 服务端：按 PROTOCOL 帧格式读写，**正确处理粘包/半包**
- 实现消息：
  - `HELLO` / `HELLO_ACK`
  - `HEARTBEAT`（双向）
  - `STATE_SNAPSHOT`（连接成功后立即）
  - `STATE_DELTA`（默认 5 Hz，无变化也可发完整快照，第 1 周允许）
- 收到 `CMD_*`：未实现功能返回 `CMD_ACK` + `ok:false` + `reason:"not_implemented"`（ESTOP 可先尽量转发给 fake_robot）
- 订阅 fake_robot 状态，填入 SNAPSHOT/DELTA
- 心跳超时打日志（第 1 周可不强制安全停）

**手动验证**
1. 启动 fake_robot + bridge 后，`fake_client.py` 能收到 HELLO_ACK 与 SNAPSHOT  
2. 连续粘包发送仍能解析（可用测试脚本）  
3. kill bridge 后客户端能感知断开；重启后可重连  

---

## 任务 1.5 QML：Connect + Dashboard（AI）

**范围**
- `Main.qml`：页面切换或 Stack/侧栏（两页即可）
- **ConnectPage**
  - IP、端口默认 `127.0.0.1:8765`
  - 连接/断开按钮
  - 连接状态、心跳 RTT、错误提示
- **DashboardPage**
  - 模式、相位、estop、control_enabled
  - 节点表：name / status / last_hb
  - pose、battery
- C++：`ConnectionController` + `RobotState`（QObject）+ 节点 TableModel
- **线程**：Socket 在非 UI 线程；信号槽回 UI 更新

**QML 约束**
- 不出现协议编解码逻辑
- 断线时控件状态正确（为第 2 周禁用控件预留）

**手动验证**
1. 连上 bridge 后 Dashboard 数值随假车变化  
2. 断开 bridge，UI 变为离线且不崩溃  
3. 连续运行 5 分钟无卡死、无明显 UI 阻塞  

---

## 任务 1.6 tools/fake_client.py（AI）

**范围**
- CLI：`--host --port`
- 流程：TCP 连接 → HELLO → 打印 HELLO_ACK → 收 SNAPSHOT → 周期 HEARTBEAT
- 打印 type/seq 与关键 JSON 字段
- `--send-sticky`：一次 write 多帧，测 bridge 粘包
- 退出：Ctrl+C 干净关闭

**手动验证**
1. `python3 tools/fake_client.py --host 127.0.0.1 --port 8765` 显示状态字段  
2. `--send-sticky` 不导致 bridge 崩溃  
3. 不启动 QML 时也可完成协议冒烟  

---

## 第 1 周验收表（用户勾选）

结果请复制到 `docs/ACCEPTANCE.md`。

| ID | 验收项 | 通过标准 | 结果 |
|----|--------|----------|------|
| A1 | 全栈启动 | 能启动 fake_robot + bridge；QML 可连接 | ☐ |
| A2 | 握手 | HELLO → HELLO_ACK，`proto=="0.1"` | ☐ |
| A3 | 心跳 | UI 显示 RTT；断 bridge 后约 3s 内显示离线 | ☐ |
| A4 | 状态可见 | Dashboard 中 mode/phase/pose/battery/节点表随仿真变化 | ☐ |
| A5 | 粘包/半包 | fake_client 或 sticky 模式下 bridge 可解析、不崩 | ☐ |
| A6 | 优雅退出 | Ctrl+C 后无僵尸节点、8765 端口释放 | ☐ |
| A7 | 文档一致 | 抽查 PROTOCOL 字段与实现一致（type/flags/seq/JSON） | ☐ |
| A8 | QML 体验 | 5 分钟操作无崩溃；断线后界面状态正确 | ☐ |

**通过标准**：A1–A7 全过为「第 1 周完成」；A8 未过可记缺陷进第 2 周，但不得阻塞协议类缺陷。

---

## 明确不在本周

- Nav2 / 地图管理 / 多车队  
- PARAM_GET/SET  
- 完整 FAULT 状态机与 launch_testing（第 2 周）  
- Gazebo 精美场景（可选，非验收）  

---

## 本周结束时用户要能口头讲清

1. 帧里 `length/type/flags/seq` 各是什么  
2. 为什么 ACK 要带 `seq`  
3. QML 为什么不直接写 socket 业务  
4. 断线时工具与机器人侧各应做什么（本周可只实现 UI 离线，第 2 周补安全停）
