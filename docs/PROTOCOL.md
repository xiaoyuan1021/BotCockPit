# BotCockpit 通信协议 v0.1

> 状态：初稿，供实现与验收。**实现不得擅自修改语义**；若需变更，先改本文档并升版本号。

- **产品**：BotCockpit（Qt6/QML 上位机 + ROS2 机器人侧）  
- **链路**：`Qt Console (C++)  ←—TCP—→  botcockpit_bridge  ←—ROS2—→  fake_robot / 真车节点`  
- **默认端口**：`8765`  
- **字节序**：多字节整数一律 **小端**  
- **文本**：JSON 为 UTF-8

---

## 1. 帧格式

```text
偏移  长度  字段
0     4     length      uint32  // 自 type（偏移4）起至 payload 末尾的字节数
4     1     type        uint8   // 消息类型
5     1     flags       uint8   // 见下
6     2     seq         uint16  // 序号；ACK 必须原样回显
8     N     payload     bytes   // JSON；无 payload 则 N=0
```

**length 最小值**：4（type+flags+seq）。  
**粘包/半包**：接收端必须按 length 切帧，允许 TCP 任意分片。

### flags

| bit | 名 | 含义 |
|-----|-----|------|
| 0 | NEED_ACK | 需要对端回 CMD_ACK / ACK 类消息 |
| 1 | URGENT | 紧急（如软急停）；桥接/机器人侧优先处理 |
| 2–7 | 保留 | 发送置 0 |

---

## 2. 消息类型

| type | 名称 | 方向 | 需要 ACK | 说明 |
|------|------|------|----------|------|
| 0x01 | HEARTBEAT | C↔B | 否 | 双向心跳 |
| 0x02 | HELLO | C→B | 否 | 连接后首条业务消息 |
| 0x03 | HELLO_ACK | B→C | — | 协议协商结果 |
| 0x10 | STATE_SNAPSHOT | B→C | 否 | 全量状态，连接成功后立即发 |
| 0x11 | STATE_DELTA | B→C | 否 | 增量/周期状态，默认 5 Hz |
| 0x20 | CMD_MODE | C→B | 是 | 模式切换 |
| 0x21 | CMD_TASK | C→B | 是 | 任务下发 |
| 0x22 | CMD_ESTOP | C→B | 是 | 软急停（flags 须含 URGENT） |
| 0x23 | CMD_RESET | C→B | 是 | 故障复位 |
| 0x2F | CMD_ACK | B→C | — | 指令应答 |
| 0x30 | EVENT_FAULT | B→C | 否 | 故障事件 |
| 0x40 | PARAM_GET | C→B | 是 | 第 3 周再实现 |
| 0x41 | PARAM_SET | C→B | 是 | 第 3 周再实现 |
| 0x42 | PARAM_ACK | B→C | — | 参数应答 |

方向：C=Console（QML 客户端），B=bridge（机器人侧网关）。

---

## 3. 超时与心跳

| 参数 | 默认值 | 说明 |
|------|--------|------|
| HEARTBEAT 周期 | 1000 ms | 双向 |
| 心跳超时 | 3000 ms | 超时视作链路故障 |
| 指令 ACK 超时 | 2000 ms | Console 未收到 ACK 则 UI 标失败 |
| STATE_DELTA 周期 | 200 ms | bridge → Console |
| 重连退避 | 1s, 2s, 4s, 8s… 上限 10s | Console 侧重连 |

**安全约定**：  
Console 心跳超时 → bridge 标记终端离线 → 机器人侧 **禁止新任务** 并进入安全停止策略（仿真同样执行）。

---

## 4. 系统状态机（机器人侧）

```text
BOOT → INITIALIZING → IDLE ⇄ RUNNING
                ↓         ↓
              FAULT ←—— DEGRADED
                ↓
              ESTOP  （任意状态可进入，优先级最高）
                ↓
              IDLE   （复位成功且无故障）
```

| 相位 phase | 含义 | 允许的新任务 |
|------------|------|----------------|
| BOOT / INITIALIZING | 启动/初始化 | 否 |
| IDLE | 空闲 | 是（若 mode 合适） |
| RUNNING | 任务执行中 | 否（先暂停/取消策略见下） |
| DEGRADED | 降级（非关键节点异常） | 否 |
| FAULT | 故障 | 否 |
| ESTOP | 急停 | 否 |

**模式 mode**：`TELEOP` | `AUTO` | `REMOTE`  

规则：  
1. `ESTOP` 由任意状态可进入，只能通过 `CMD_RESET`（且无故障源）退出到 `IDLE`。  
2. `ESTOP` / `FAULT` / 心跳离线 时，拒绝 `CMD_TASK` 与 `CMD_MODE=AUTO`。  
3. `CMD_RESET` 的 payload 必须含 `confirm: true`，否则 NACK。  
4. RUNNING 时新 `CMD_TASK` 默认 NACK（`busy`）；后续可加 `CMD_TASK` 带 `preempt: true`（v0.2）。

---

## 5. Payload JSON 规范

### 5.1 HEARTBEAT (0x01)

```json
{ "ts_ms": 1710000000123, "role": "console" }
```

`role`: `console` | `bridge`。bridge 回发时 `role":"bridge"`。

### 5.2 HELLO (0x02)

```json
{ "client": "BotCockpit", "ui_version": "0.1.0", "proto_min": "0.1", "proto_max": "0.1" }
```

### 5.3 HELLO_ACK (0x03)

```json
{ "ok": true, "proto": "0.1", "server": "botcockpit_bridge/0.1.0" }
```

失败示例：`{ "ok": false, "reason": "proto_unsupported" }`

### 5.4 STATE_SNAPSHOT (0x10) / STATE_DELTA (0x11)

```json
{
  "ts_ms": 1710000000456,
  "conn": "ONLINE",
  "mode": "TELEOP",
  "phase": "IDLE",
  "heartbeat_rtt_ms": 12,
  "nodes": [
    { "name": "bridge", "status": "OK", "last_hb_ms": 1710000000400 },
    { "name": "fake_robot", "status": "OK", "last_hb_ms": 1710000000398 }
  ],
  "pose": { "x": 0.0, "y": 0.0, "yaw": 0.0 },
  "battery": 83,
  "task": { "id": null, "type": null, "status": "NONE" },
  "faults": [],
  "estop": false,
  "control_enabled": true
}
```

字段说明：

| 字段 | 类型 | 说明 |
|------|------|------|
| conn | string | `ONLINE` / `DEGRADED` / `OFFLINE` / `ESTOP` |
| mode | string | `TELEOP` / `AUTO` / `REMOTE` |
| phase | string | 见状态机 |
| nodes[].status | string | `OK` / `WARN` / `ERROR` / `LOST` |
| battery | number | 0–100，仿真可模拟 |
| faults | array | 见 EVENT_FAULT 的对象列表 |
| estop | bool | 是否急停 |
| control_enabled | bool | UI 是否允许下发任务/切 AUTO |

DELTA 可只带变化字段，但必须含 `ts_ms`；未出现的键表示不变。

### 5.5 CMD_MODE (0x20)

```json
{ "mode": "AUTO" }
```

### 5.6 CMD_TASK (0x21)

```json
{ "task_id": "T-003", "type": "goto", "x": 2.5, "y": 1.0, "timeout_s": 30 }
```

| type | 参数 | 说明 |
|------|------|------|
| `goto` | x, y | 仿真假车直线/分段运动即可 |
| `pause` | — | 暂停当前任务 |
| `resume` | — | 继续 |
| `cancel` | — | 取消 |

### 5.7 CMD_ESTOP (0x22)

```json
{ "reason": "operator" }
```

发送时 `flags` 必须置 `URGENT|NEED_ACK`。

### 5.8 CMD_RESET (0x23)

```json
{ "confirm": true }
```

### 5.9 CMD_ACK (0x2F)

```json
{
  "seq": 42,
  "ok": true,
  "cmd": "CMD_TASK",
  "task_id": "T-003",
  "status": "ACCEPTED",
  "reason": null
}
```

失败：

```json
{ "seq": 42, "ok": false, "cmd": "CMD_TASK", "status": "REJECTED", "reason": "estop_active" }
```

常见 `reason`：`estop_active` `phase_busy` `not_allowed_in_mode` `invalid_payload` `confirm_required` `unknown_task` `timeout` `proto_error`

`status`：`ACCEPTED` | `REJECTED` | `EXECUTING` | `DONE` | `FAILED`（EXECUTING/DONE 可用 STATE 的 task 字段推，不必反复 ACK）

### 5.10 EVENT_FAULT (0x30)

```json
{
  "ts_ms": 1710000000999,
  "code": "E_NODE_TIMEOUT",
  "level": "ERROR",
  "node": "fake_robot",
  "detail": "cmd ack timeout 3.0s",
  "active": true
}
```

| level | 含义 |
|-------|------|
| INFO | 提示 |
| WARN | 可降级运行 |
| ERROR | 进入 FAULT 或需人工处理 |

建议故障码（可扩展）：

| code | level | 含义 |
|------|-------|------|
| E_HEARTBEAT_LOST | ERROR | 终端/节点心跳丢失 |
| E_NODE_TIMEOUT | ERROR | 关键节点无响应 |
| E_ESTOP_ACTIVE | WARN/ERROR | 急停中 |
| E_TASK_FAILED | ERROR | 任务失败 |
| W_BATTERY_LOW | WARN | 电量低（仿真可触发） |

---

## 6. ROS2 桥接侧约定（bridge ↔ 机器人）

协议 JSON 与 ROS2 接口解耦；bridge 内部映射建议如下（实现可微调，但需在 ACCEPTANCE 中记录）：

| 协议消息 | ROS2 建议接口 |
|----------|----------------|
| 状态 | Topic `botcockpit/state`（自定义或 JSON string） |
| 节点诊断 | Topic `/diagnostics`（diagnostic_msgs）或 bridge 汇总 |
| 任务/急停/复位 | Service 或 Topic 指令 + 结果话题 |
| 模式切换 | Service `botcockpit/set_mode` |

`fake_robot` 必须实现：状态发布、goto 简易运动、estop 立即停、reset、故障注入开关。

---

## 7. 安全与优先级

1. **处理优先级**：`CMD_ESTOP` > 心跳/连接维护 > `CMD_RESET` > 其他指令  
2. **UI 联动**：`control_enabled=false` 时 QML 禁用任务与 AUTO（本地即可）  
3. **双端防重**：同一 `task_id` 重复下发返回 NACK 或幂等 DONE  
4. **不执行**：ESTOP 期间任何运动类 ROS 指令 bridge 不得转发给执行器节点  

---

## 8. 版本

| 版本 | 日期 | 说明 |
|------|------|------|
| 0.1 | 2026-09-20 | 初稿：帧格式、核心消息、状态机、安全约定 |

变更流程：改本文档 → 升版本 → 同步实现与测试 → 记入 `ACCEPTANCE.md`。
