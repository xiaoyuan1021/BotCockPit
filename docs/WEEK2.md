# BotCockpit — 第 2 周任务卡

- **依据**：`docs/PROTOCOL.md` v0.1、`docs/PLAN.md` 第 2 周
- **协作**：AI 实现 / 用户验收；协议语义不得擅改
- **主路径**：fake_robot + bridge + QML Control

---

## 任务清单

| ID | 项 | 产出 |
|----|----|------|
| 2.1 | 状态机 | `BOOT→INITIALIZING→IDLE⇄RUNNING→FAULT/DEGRADED`，ESTOP 最高优先 |
| 2.2 | 指令闭环 | CMD_MODE/TASK/ESTOP/RESET + ACK/NACK（seq 回显） |
| 2.3 | 安全策略 | 控制端离线/心跳超时/关键故障 → 禁止新任务 + SAFE 停止 |
| 2.4 | QML Control | 模式、goto/暂停/继续/取消、软急停、复位（二次确认） |
| 2.5 | launch_testing | ≥3：正常任务、断线安全、急停锁定 |
| 2.6 | 故障注入 | `tools/inject_fault.py` |

## 验收（复制到 ACCEPTANCE.md）

| ID | 项 | 通过标准 |
|----|----|----------|
| B1 | 状态机 | IDLE 可接任务；RUNNING 忙拒新任务；ESTOP 任意态可入 |
| B2 | 指令 ACK | UI/客户端发出指令均见 `CMD_ACK` 且 seq 一致 |
| B3 | 急停 | ESTOP 后拒绝 AUTO/任务直至 `CMD_RESET`（confirm）成功 |
| B4 | 断线安全 | 杀 bridge/假车或 console 离线后 ~3s 内 control_enabled=false，RUNNING 变 SAFE 停 |
| B5 | launch_testing | 3 条全绿 |
| B6 | 故障注入 | inject 后 phase/FAULT 或节点 ERROR，UI 可见 |
| B7 | QML Control | 二次确认急停/复位；离线/禁止时控件禁用（ESTOP 连接后可用） |

## 明确不做（本周）

- PARAM_GET/SET 实现
- 地图 / Nav2 / 多车
- 重型图表库
