#pragma once

#include <cstdint>

namespace botcockpit {

// PROTOCOL.md v0.1 — message types
constexpr uint8_t MSG_HEARTBEAT = 0x01;
constexpr uint8_t MSG_HELLO = 0x02;
constexpr uint8_t MSG_HELLO_ACK = 0x03;
constexpr uint8_t MSG_STATE_SNAPSHOT = 0x10;
constexpr uint8_t MSG_STATE_DELTA = 0x11;
constexpr uint8_t MSG_CMD_MODE = 0x20;
constexpr uint8_t MSG_CMD_TASK = 0x21;
constexpr uint8_t MSG_CMD_ESTOP = 0x22;
constexpr uint8_t MSG_CMD_RESET = 0x23;
constexpr uint8_t MSG_CMD_ACK = 0x2F;
constexpr uint8_t MSG_EVENT_FAULT = 0x30;
constexpr uint8_t MSG_PARAM_GET = 0x40;
constexpr uint8_t MSG_PARAM_SET = 0x41;
constexpr uint8_t MSG_PARAM_ACK = 0x42;

// flags
constexpr uint8_t FLAG_NEED_ACK = 0x01;
constexpr uint8_t FLAG_URGENT = 0x02;

// timing (ms)
constexpr int HB_INTERVAL_MS = 1000;
constexpr int HB_TIMEOUT_MS = 3000;
constexpr int ACK_TIMEOUT_MS = 2000;
constexpr int STATE_DELTA_MS = 200;
constexpr int DEFAULT_PORT = 8765;

constexpr const char* PROTO_VERSION = "0.1";
constexpr const char* SERVER_NAME = "botcockpit_bridge/0.1.0";

inline const char* msg_type_name(uint8_t type)
{
  switch (type) {
    case MSG_HEARTBEAT: return "HEARTBEAT";
    case MSG_HELLO: return "HELLO";
    case MSG_HELLO_ACK: return "HELLO_ACK";
    case MSG_STATE_SNAPSHOT: return "STATE_SNAPSHOT";
    case MSG_STATE_DELTA: return "STATE_DELTA";
    case MSG_CMD_MODE: return "CMD_MODE";
    case MSG_CMD_TASK: return "CMD_TASK";
    case MSG_CMD_ESTOP: return "CMD_ESTOP";
    case MSG_CMD_RESET: return "CMD_RESET";
    case MSG_CMD_ACK: return "CMD_ACK";
    case MSG_EVENT_FAULT: return "EVENT_FAULT";
    case MSG_PARAM_GET: return "PARAM_GET";
    case MSG_PARAM_SET: return "PARAM_SET";
    case MSG_PARAM_ACK: return "PARAM_ACK";
    default: return "UNKNOWN";
  }
}

}  // namespace botcockpit
