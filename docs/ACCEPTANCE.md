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

**本周结论**：**第 1 周完成**。

---

## Week 2 — 2026-09-20 — **自动化通过 / UI 控制待现场点选**

编译：`botcockpit_bridge` + `botcockpit_sim` + `ui/botcockpit` 均 BUILD_OK（SSH 至 192.168.9.98）。

| ID | 项 | 结果 | 现象 / 复现 | 备注 |
|----|----|------|-------------|------|
| B1 | 状态机 | PASS（launch） | IDLE 可接 goto；ESTOP 任意态；FAULT 见 inject | test_normal + test_estop |
| B2 | 指令 ACK | PASS（launch） | CMD_* 均回 ACK，seq 匹配；UI 已接 `lastCmdAckText` | |
| B3 | 急停锁定 | PASS | ESTOP 后 TASK/AUTO 被拒；RESET 无 confirm → `confirm_required`；confirm 后回 IDLE | test_estop_lock |
| B4 | 断线安全 | PASS | console offline → `control_enabled=false`，RUNNING SAFE 停；恢复后可再控 | test_link_loss_safety |
| B5 | launch_testing | PASS | 3/3 Passed（normal / estop / link_loss） | `ctest` in build/botcockpit_sim |
| B6 | 故障注入 | PASS | `tools/inject_fault.py` → `phase=FAULT`；`--clear` 可清 | smoke 日志 |
| B7 | QML Control | PASS（主路径） | 截图：PHASE RUNNING · TASK goto EXECUTING · ACK `CMD_TASK OK ACCEPTED seq=10` · ctl OFF；ESTOP 按钮可用 | ESTOP/Reset 对话框建议再各点一次作完整记录 |

**本周结论**：PLAN 第 2 周验收 **B1–B7 通过**（自动化 + Control UI 主路径）。**2026-09-20 代码 review：Ready to close**（无 Critical）。

### Week 2 Code Review（收官）

| 类型 | 结论 |
|------|------|
| Critical | **无** — 主路径/安全策略/架构约束成立 |
| 已确认 OK | 状态机 §4、ESTOP/RESET、console SAFE 停、ACK、QML 只绑 Model 不写协议 |
| 收官时已修 | `hasRobotState` 误判；`wait_state` 假通过；link_loss 断言 SAFE 停出 RUNNING |
| 记入 Week 3 | bridge 同连接 ESTOP 优先级（§7.1）；TCP 全链路 launch 测试；B7 ESTOP/RESET 对话框补点；RESET 残余 ERROR 的 reason；DELTA 真增量 |

**未关闭缺陷（Week 3  backlog）**：
1. TCP 同连接上 ESTOP 可能排在进行中 CMD 之后（需指令队列/紧急通道）
2. launch_testing 未覆盖 bridge TCP 转发
3. UI 上 ESTOP/Reset 确认框完整点击（B7 细节）
4. STATE_DELTA 仍为全量快照

**复现 launch_testing：**
```bash
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash
cd ~/ros2_ws/build/botcockpit_sim && ctest --output-on-failure
```

**未关闭缺陷**：
1. B7：QML Control 页人工验收（ESTOP/RESET 对话框、控件禁用）
2. STATE_DELTA 仍为全量快照（WEEK1 允许；第 3 周可改增量）

**实现要点（面试可讲）**：
- 状态机：ESTOP 最高优先；ERROR→FAULT；WARN→DEGRADED；console 心跳丢失 → SAFE 停
- bridge 发布 `botcockpit/console` 在线位；无客户端时 robot 禁止新任务
- Control 页仅调用 C++ `ConnectionController` API；编解码仍在 SocketWorker 线程
