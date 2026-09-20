#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace botcockpit_ui {

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

constexpr uint8_t FLAG_NEED_ACK = 0x01;
constexpr uint8_t FLAG_URGENT = 0x02;

constexpr int HB_INTERVAL_MS = 1000;
constexpr int HB_TIMEOUT_MS = 3000;

struct Frame {
    uint8_t type = 0;
    uint8_t flags = 0;
    uint16_t seq = 0;
    std::string payload;
};

constexpr uint32_t kMinFrameLength = 4;
constexpr uint32_t kMaxFrameLength = 64u * 1024u;

std::vector<uint8_t> encode_frame(uint8_t type, uint8_t flags, uint16_t seq,
                                  const std::string& payload);

class FrameDecoder {
public:
    void append(const char* data, size_t n);
    bool next(Frame& out);
    void clear();

private:
    std::vector<uint8_t> buf_;
};

}  // namespace botcockpit_ui
