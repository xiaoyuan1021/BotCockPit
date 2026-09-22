# BotCockpit Plan A — 导航算法调试台

## 产品句

面向 ROS2 差速车仿真的**算法调试上位机**：栅格地图上 A\* 规划 + Pure Pursuit 跟踪，操作员下发任务并保留软急停/安全接管。

## 本迭代已实现

| 模块 | 说明 |
|------|------|
| `sim/include/botcockpit_sim/nav.hpp` | 走廊栅格图、障碍膨胀、A\*、Pure Pursuit |
| `fake_robot` goto | CMD_TASK → A\* 路径 → 差速运动学跟踪 |
| STATE `nav` | `status` / `path_len` / `path` / `goal` |
| UI | Control 横幅显示 `NAV: TRACKING · N wp` |
| 测试 | `tools/test_astar.py` 离线自测（路径过墙缝） |

## 安全衔接

- ESTOP / console 离线：打断 RUNNING，路径失效于 SAFE 停  
- A\* 失败：任务 REJECT + `E_TASK_FAILED`  
- bridge：URGENT ESTOP 不经指令队列阻塞（§7.1 加强）

## 与自动驾驶技能映射

规划（A\*+膨胀）· 跟踪（Pure Pursuit）· 状态机 · 仿真评测 · 车端安全/接管

## 后续（可选）

- ~~QML Canvas 画 path/障碍~~ → **NavMapView.qml**（走廊图 + 路径 + 位姿箭头 + 目标十字）  
- ~~跟踪误差~~ → state.nav.track_err / UI `err x.xxx m`；误差 &gt;1.5 m 判失败  
- ~~TCP 路径脚本~~ → `tools/test_tcp_nav.py`（HELLO/粘包/TASK/ESTOP）  
- 动态障碍与重规划  
- 跟踪误差历史曲线  
- Gazebo 接入（第二阶段）
