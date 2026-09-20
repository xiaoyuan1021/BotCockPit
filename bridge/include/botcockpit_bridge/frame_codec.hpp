#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace botcockpit {

struct Frame {
  uint8_t type = 0;
  uint8_t flags = 0;
  uint16_t seq = 0;
  std::string payload;  // UTF-8 JSON, may be empty
};

// Frame layout (little-endian):
//   length:u32 | type:u8 | flags:u8 | seq:u16 | payload:bytes
// length counts from type through end of payload (min 4, max kMaxFrameLength).
constexpr uint32_t kMinFrameLength = 4;
constexpr uint32_t kMaxFrameLength = 64u * 1024u;
std::vector<uint8_t> encode_frame(uint8_t type, uint8_t flags, uint16_t seq,
                                  const std::string& payload);

class FrameDecoder {
 public:
  void append(const uint8_t* data, size_t n);
  void append(const std::string& bytes);
  // Extract one complete frame. Returns false if buffer has a partial frame.
  // On protocol error (length < 4), buffer is cleared and false returned.
  bool next(Frame& out);
  void clear();
  size_t buffered() const { return buf_.size(); }

 private:
  std::vector<uint8_t> buf_;
};

}  // namespace botcockpit
