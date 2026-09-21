# BotCockpit — 第 3 周任务卡

- **依据**：`docs/PLAN.md` 第 3 周、`docs/PROTOCOL.md` v0.1、Week2 review backlog
- **目标**：像工具，不像 demo

| ID | 项 | 产出 |
|----|----|------|
| 3.1 | QML Fault + Settings | 故障列表页；连接/自动重连/端口设置 |
| 3.2 | 重连与状态恢复 | 断线自动重连（退避）；重连后 SNAPSHOT 对齐 |
| 3.3 | Model 细化 | 节点表、故障表按等级排序/筛选 |
| 3.4 | 体验 | 缩放可接受；离线禁用；急停视觉反馈 |
| 3.5 | 日志 | UI 日志面板 + bridge 结构化日志 |
| 3.6 | backlog | 同连接 ESTOP 帧优先；DELTA 仍可全量但 UI 新鲜度正确 |

## 验收（C1–C7）

| ID | 项 | 通过标准 |
|----|----|----------|
| C1 | Fault 页 | inject 故障后列表可见 code/level/node |
| C2 | Settings | 可改 host/port；自动重连开关 |
| C3 | 自动重连 | 杀 bridge 后自动重试；恢复后状态恢复 |
| C4 | 筛选 | 故障/节点可按等级看（ERROR/WARN） |
| C5 | 体验 | 离线控件禁用；ESTOP 视觉反馈明显 |
| C6 | 日志 | UI 有最近事件日志（连接/指令/故障） |
| C7 | 协议 | 不改 PROTOCOL 语义；QML 仍无编解码 |

**明确不做**：Gazebo、PARAM 热更、重型图表库。
