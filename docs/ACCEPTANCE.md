# BotCockpit 验收记录

## Week 3 — 2026-09-20 — 进行中（代码已落地，待 UI 点选）

编译：bridge + sim + QML BUILD_OK（SSH VM）。

| ID | 项 | 结果 | 说明 |
|----|----|------|------|
| C1 | Fault 页 | 待 UI | 列表 + ERROR/WARN 筛选 + 日志面板已实现 |
| C2 | Settings | 待 UI | host/port、自动重连开关 |
| C3 | 自动重连 | 待 UI | 退避 1→10s；日志记录 attempts |
| C4 | 筛选 | 待 UI | FaultTableModel 等级排序/筛选 |
| C5 | 体验 | 待 UI | 离线禁用、ESTOP 色条、窗口自适应 |
| C6 | 日志 | 待 UI | 连接/指令/ACK 写入 UI 日志 |
| C7 | 协议 | PASS | 未改语义；QML 仍无编解码；同连接 ESTOP 帧优先已实现 |

### Week 2 backlog 进度
- TCP 同连接 ESTOP 帧优先（batch stable_sort）— **已实现**（跨指令阻塞仍见 Week3 备注）
- launch_tests wait_state 严格化 — **已实现**
- hasRobotState 误判 — **已实现**
- B7 ESTOP/Reset 对话框补点 — 待 UI
- DELTA 全量快照 — 保留（协议 Week1 允许）

### 复现
```bash
cd ~/ros2_ws/Bot/Bot_Project/ui/botcockpit && ./build/botcockpit
# Fault 页：python3 tools/inject_fault.py 后查看列表
# Settings：关 bridge 观察自动重连日志
```
