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

## Week 1 — 2026-09-20（协议冒烟，基于用户终端日志）

环境：Ubuntu 22.04 x86_64，`~/ros2_ws`，ROS2 Humble；已清理 aarch64 交叉环境变量，native `gcc/g++` 编译。

| ID | 项 | 结果 | 现象 / 复现 | 备注 |
|----|----|------|-------------|------|
| A1 | 全栈启动 | PASS | `colcon build` 2 包 Finished；fake_robot + bridge 运行中，fake_client 可连 | 需 lifecycle configure/activate 后状态为 IDLE |
| A2 | 握手 | PASS | HELLO_ACK `ok=true`，`proto=0.1`，`server=botcockpit_bridge/0.1.0` | |
| A3 | 心跳 / 断线 | PASS（协议侧） | 杀 bridge 后 fake_client 心跳发送失败：`connection failed: [Errno 32] Broken pipe`；此前 STATE_DELTA 停止 | QML 离线 UI 待 A8 一并看 |
| A4 | 状态可见 | PASS | SNAPSHOT/DELTA 含 mode/phase/pose/battery/nodes/estop/control_enabled；pose 递增、battery 缓降 | |
| A5 | 粘包 / 半包 | PASS | `--self-test` PASS；`--send-sticky` 一次 write 161 字节，bridge 正常解析并回 ACK | |
| A6 | 优雅退出 | PASS | `ss -lptn \| grep 8765`：运行中有 LISTEN；`Ctrl+C` 后无输出，端口释放 | 修复 SIGINT 后复测通过 |
| A7 | 文档一致 | PASS（抽查） | type/flags/seq/JSON 与 PROTOCOL.md v0.1 一致；CMD_ACK 回显 seq | |
| A8 | QML 体验 | 进行中 | 开始构建 ui/botcockpit | 依赖 Qt6 |

**本周结论**：协议与 ROS 侧主路径（A1–A7）通过；A8 QML 待补测后可宣布「第 1 周完成」。

**未关闭缺陷**：
1. A8：QML Connect/Dashboard 联调

**备注（实现侧，非缺陷）**：
- STATE 中 `heartbeat_rtt_ms` 由 bridge 填 0，真实 RTT 由 Console 根据 HEARTBEAT 往返计算（见 QML）  
- 交叉编译污染已通过注释 `~/.bashrc` + 使用 `/opt/ros/humble` + host `CC/CXX` 解决  

---

## Week 1 — 待用户补测后勾选

参考 `WEEK1.md` 验收表 A1–A8（上表为基于 2026-09-20 日志的草稿）。
