# BotCockpit 验收记录

> 每周验收后填写。失败项写现象与复现步骤，AI 只修复验收项。

## 模板

### Week N — 日期

| ID | 项 | 结果 (PASS/FAIL) | 现象 / 复现 | 备注 |
|----|----|------------------|-------------|------|
| | | | | |

**本周结论**：通过 / 未通过  
**未关闭缺陷**：

---

## Week 1 — 2026-09-20 — **通过**

环境：Ubuntu 22.04 x86_64，ROS2 Humble，Qt 6.2.4；native gcc/g++；仓库 `~/ros2_ws/Bot/Bot_Project`。

| ID | 项 | 结果 | 现象 / 复现 | 备注 |
|----|----|------|-------------|------|
| A1 | 全栈启动 | PASS | colcon bridge+sim 成功；fake_robot + bridge + QML 可连 | |
| A2 | 握手 | PASS | HELLO_ACK `ok=true` `proto=0.1` `server=botcockpit_bridge/0.1.0` | |
| A3 | 心跳/断线 | PASS | 心跳双向正常；杀 bridge 后 fake_client `Broken pipe`；QML 显示 OFFLINE/NO LINK | |
| A4 | 状态可见 | PASS | Dashboard：IDLE/TELEOP/battery≈94.4%/pose.x 变化/节点 OK | 截图 2026-09-20 |
| A5 | 粘包/半包 | PASS | `--self-test` PASS；`--send-sticky` 161 字节一次写入可解析 | |
| A6 | 优雅退出 | PASS | `Ctrl+C` 后 `ss -lptn \| grep 8765` 无监听 | 曾修 SIGINT→rclcpp::shutdown |
| A7 | 文档一致 | PASS | type/flags/seq/JSON 与 PROTOCOL.md v0.1 一致 | |
| A8 | QML 体验 | PASS | Connect+Dashboard 可用；断线状态正确；状态随仿真刷新 | Qt6.2.4 |

**本周结论**：**第 1 周完成**，进入第 2 周（状态机完善 + Control 页 + launch_testing）。

**未关闭缺陷**：无（第 1 周范围）

**过程记录**：
- 交叉编译环境污染 ROS2 host 构建 → 注释 `~/.bashrc` 交叉 export，改用 `/opt/ros/humble` + host gcc
- bridge 自定义 SIGINT 导致无法退出 → 改为 ROS2 默认 shutdown
- QML 首连 battery=0 → fake_robot configure 后立即 publish；UI 增加 hasRobotState 提示
- Qt 6.2 无 `qt_standard_project_setup` → CMake 改 AUTOMOC + qrc
- 静态 review 后 Week1 收尾修复：ESTOP→RESET 不再被 E_ESTOP_ACTIVE 卡死；bridge 按机器人状态新鲜度下发 conn=OFFLINE；NACK reason=timeout；帧 length 上限 64KiB；TCP fd 原子关闭；bridge 周期主动 HEARTBEAT

---

## Week 2 待办（来自 code review，未在 Week1 关闭）

| 优先级 | 项 | 说明 |
|--------|----|------|
| P0 | Control 页 + 指令闭环 UI | CMD_MODE/TASK/ESTOP/RESET + 二次确认 |
| P0 | launch_testing ≥3 条 | 正常任务 / 断线安全 / 急停锁定 |
| P1 | ESTOP 期间 bridge 不转发运动指令 | §7.4 已在 bridge 加 estop 拒绝 TASK/AUTO，需测试 |
| P1 | EVENT_FAULT 独立消息 | 当前仅嵌在 state.faults[] |
| P2 | 协议头去重 / STATE 真增量 DELTA | 减重复与带宽 |
| P2 | Dashboard last_hb 显示相对时间 | 可用性 |
