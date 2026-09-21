# BotCockpit 验收记录

## Week 3 — 2026-09-21 — **通过（工具向完整）**

环境：Ubuntu VM + SSH 同步；colcon bridge/sim + QML BUILD_OK；launch_testing 3/3。

| ID | 项 | 结果 | 说明 |
|----|----|------|------|
| C1 | Fault 页 | PASS | 列表 + 等级筛选；inject 后可见 ERROR |
| C2 | Settings | PASS | host/port、自动重连开关 |
| C3 | 自动重连 | PASS | 退避日志；重连后 SNAPSHOT/状态恢复 |
| C4 | 筛选 | PASS | Fault 按 ERROR/WARN 过滤与排序 |
| C5 | 体验 | PASS | 浅色主题、离线禁用、ESTOP/相位色反馈、日志可复制 |
| C6 | 日志 | PASS | UI 事件日志：connect / HELLO_ACK / CMD / ACK |
| C7 | 协议/架构 | PASS | 未改 PROTOCOL 语义；QML 无编解码；同连接 ESTOP 帧优先 |

**本周结论**：第 3 周「像工具」目标完成。功能主线（连接→状态→控制→故障→日志→重连）已闭环。

**遗留（不阻塞 Week3）**：
- 同连接上 ESTOP 在「上一指令阻塞 wait ACK」期间仍可能排队（batch 内已优先）——需要指令队列才能完全符合 §7.1
- launch_testing 未覆盖 bridge TCP 全链路
- DELTA 仍为全量快照
- B7 历史项：ESTOP/Reset 对话框建议各再点一次归档

**方向讨论**：见与用户的周验收沟通——是否在现有骨架上加深「机器人故障诊断 / 任务编排 / 会话回放」等，而非继续堆展示页。

---

## Week 2 — 2026-09-20 — 通过

（详见历史：B1–B7 PASS，review ready-to-close）

## Week 1 — 2026-09-20 — 通过

（A1–A8 PASS）
