#include "botcockpit_bridge/frame_codec.hpp"

namespace botcockpit {

std::vector<uint8_t> encode_frame(uint8_t type, uint8_t flags, uint16_t seq,
                                  const std::string& payload)
{
  const uint32_t length = 4u + static_cast<uint32_t>(payload.size());
  std::vector<uint8_t> out;
  out.reserve(4u + length);
  out.push_back(static_cast<uint8_t>(length & 0xFFu));
  out.push_back(static_cast<uint8_t>((length >> 8) & 0xFFu));
  out.push_back(static_cast<uint8_t>((length >> 16) & 0xFFu));
  out.push_back(static_cast<uint8_t>((length >> 24) & 0xFFu));
  out.push_back(type);
  out.push_back(flags);
  out.push_back(static_cast<uint8_t>(seq & 0xFFu));
  out.push_back(static_cast<uint8_t>((seq >> 8) & 0xFFu));
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

void FrameDecoder::append(const uint8_t* data, size_t n)
{
  if (data == nullptr || n == 0) {
    return;
  }
  buf_.insert(buf_.end(), data, data + n);
}

void FrameDecoder::append(const std::string& bytes)
{
  append(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
}

bool FrameDecoder::next(Frame& out)
{
  if (buf_.size() < 4) {
    return false;
  }
  const uint32_t length = static_cast<uint32_t>(buf_[0]) |
                          (static_cast<uint32_t>(buf_[1]) << 8) |
                          (static_cast<uint32_t>(buf_[2]) << 16) |
                          (static_cast<uint32_t>(buf_[3]) << 24);
  if (length < 4) {
    buf_.clear();
    return false;
  }
  if (buf_.size() < 4u + length) {
    return false;
  }
  out.type = buf_[4];
  out.flags = buf_[5];
  out.seq = static_cast<uint16_t>(buf_[6]) |
            (static_cast<uint16_t>(buf_[7]) << 8);
  out.payload.assign(buf_.begin() + 8, buf_.begin() + 4 + length);
  buf_.erase(buf_.begin(), buf_.begin() + 4 + length);
  return true;
}

void FrameDecoder::clear()
{
  buf_.clear();
}

}  // namespace botcockpit
