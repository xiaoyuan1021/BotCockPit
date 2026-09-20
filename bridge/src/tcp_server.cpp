#include "botcockpit_bridge/tcp_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <sstream>

#include "botcockpit_bridge/json_util.hpp"
#include "botcockpit_bridge/protocol.hpp"

namespace botcockpit {

namespace {
constexpr size_t kReadChunk = 4096;
}

uint64_t TcpServer::now_ms()
{
  using namespace std::chrono;
  return static_cast<uint64_t>(
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count());
}

TcpServer::TcpServer(StateProvider state_provider, CommandHandler cmd_handler,
                     LogFn log)
    : state_provider_(std::move(state_provider)),
      cmd_handler_(std::move(cmd_handler)),
      log_(std::move(log))
{
}

TcpServer::~TcpServer()
{
  stop();
}

void TcpServer::set_state_provider(StateProvider fn)
{
  state_provider_ = std::move(fn);
}

bool TcpServer::start(int port)
{
  if (running_.load()) {
    return true;
  }
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    if (log_) {
      log_("[tcp] socket() failed");
    }
    return false;
  }
  int yes = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    if (log_) {
      log_("[tcp] bind() failed on port " + std::to_string(port));
    }
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  if (::listen(listen_fd_, 8) < 0) {
    if (log_) {
      log_("[tcp] listen() failed");
    }
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  port_ = port;
  running_.store(true);
  accept_thread_ = std::thread([this]() { accept_loop(); });
  watchdog_thread_ = std::thread([this]() { heartbeat_watchdog(); });
  if (log_) {
    log_("[tcp] listening on 0.0.0.0:" + std::to_string(port));
  }
  return true;
}

void TcpServer::stop()
{
  // Idempotent shutdown: allow Ctrl+C → spin exit → stop_tcp() → dtor.
  const bool was_running = running_.exchange(false);
  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
    listen_fd_ = -1;
  }
  {
    std::lock_guard<std::mutex> lock(clients_mu_);
    for (auto& c : clients_) {
      if (c && c->fd >= 0) {
        ::shutdown(c->fd, SHUT_RDWR);
        ::close(c->fd);
        c->fd = -1;
      }
    }
  }
  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }
  if (watchdog_thread_.joinable()) {
    watchdog_thread_.join();
  }
  {
    std::lock_guard<std::mutex> lock(clients_mu_);
    clients_.clear();
  }
  if (was_running && log_) {
    log_("[tcp] server stopped");
  }
}

size_t TcpServer::client_count() const
{
  std::lock_guard<std::mutex> lock(clients_mu_);
  size_t n = 0;
  for (const auto& c : clients_) {
    if (c && c->fd >= 0 && c->hello_ok) {
      ++n;
    }
  }
  return n;
}

void TcpServer::accept_loop()
{
  while (running_.load()) {
    sockaddr_in peer{};
    socklen_t len = sizeof(peer);
    const int fd =
        ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&peer), &len);
    if (fd < 0) {
      if (!running_.load()) {
        break;
      }
      continue;
    }
    int yes = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));

    auto client = std::make_shared<Client>();
    client->fd = fd;
    client->last_rx_ms = now_ms();
    client->last_hb_rx_ms = client->last_rx_ms;
    {
      std::lock_guard<std::mutex> lock(clients_mu_);
      clients_.push_back(client);
    }
    if (log_) {
      char ip[64] = {0};
      ::inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
      log_("[tcp] client connected " + std::string(ip) + ":" +
           std::to_string(ntohs(peer.sin_port)));
    }
    std::thread([this, client]() { client_loop(client); }).detach();
  }
}

void TcpServer::client_loop(std::shared_ptr<Client> client)
{
  uint8_t buf[kReadChunk];
  while (running_.load() && client->fd >= 0) {
    const ssize_t n = ::recv(client->fd, buf, sizeof(buf), 0);
    if (n == 0) {
      break;
    }
    if (n < 0) {
      if (!running_.load()) {
        break;
      }
      continue;
    }
    client->last_rx_ms = now_ms();
    client->decoder.append(buf, static_cast<size_t>(n));
    Frame frame;
    while (client->decoder.next(frame)) {
      handle_frame(client, frame);
      if (client->fd < 0) {
        break;
      }
    }
  }
  const int fd = client->fd;
  client->fd = -1;
  remove_client(fd);
  if (fd >= 0) {
    ::close(fd);
  }
  if (log_) {
    log_("[tcp] client disconnected");
  }
}

void TcpServer::remove_client(int fd)
{
  if (fd < 0) {
    return;
  }
  std::lock_guard<std::mutex> lock(clients_mu_);
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (*it && (*it)->fd == fd) {
      clients_.erase(it);
      break;
    }
  }
}

bool TcpServer::send_frame(const std::shared_ptr<Client>& client, uint8_t type,
                           uint8_t flags, uint16_t seq,
                           const std::string& payload)
{
  if (!client || client->fd < 0) {
    return false;
  }
  const auto bytes = encode_frame(type, flags, seq, payload);
  std::lock_guard<std::mutex> lock(client->send_mu);
  size_t off = 0;
  while (off < bytes.size()) {
    const ssize_t n =
        ::send(client->fd, bytes.data() + off, bytes.size() - off, MSG_NOSIGNAL);
    if (n <= 0) {
      return false;
    }
    off += static_cast<size_t>(n);
  }
  return true;
}

std::string TcpServer::inject_runtime_fields(const std::string& state_json,
                                             const std::string& conn_value)
{
  // Week-1: bridge may send full state as DELTA; ensure conn/rtt present.
  std::string body = state_json;
  if (body.empty() || body[0] != '{') {
    body = "{}";
  }
  // Strip trailing '}' and merge runtime keys if missing.
  if (!body.empty() && body.back() == '}') {
    body.pop_back();
  }
  if (!body.empty() && body.back() != ',' && body.back() != '{') {
    body += ",";
  }
  body += "\"conn\":\"" + json::escape(conn_value) + "\"";
  body += ",\"heartbeat_rtt_ms\":0";
  // Remove duplicate keys if state already has them — keep last (ours) for
  // week-1 simplicity: clients typically take first match; rebuild cleanly
  // by preferring source keys. We only append if not present.
  // Rebuild: if original already had conn, JSON parsers may use first value.
  // So construct a thin wrapper only when needed.
  body += "}";
  // Prefer deterministic object: if original had conn, keep original by
  // sending original when it already contains "\"conn\"".
  if (state_json.find("\"conn\"") != std::string::npos) {
    return state_json;
  }
  return body;
}

void TcpServer::handle_frame(const std::shared_ptr<Client>& client,
                             const Frame& frame)
{
  if (log_) {
    std::ostringstream oss;
    oss << "[tcp] rx type=0x" << std::hex << static_cast<int>(frame.type)
        << std::dec << " (" << msg_type_name(frame.type) << ") seq="
        << frame.seq << " len=" << frame.payload.size();
    log_(oss.str());
  }

  switch (frame.type) {
    case MSG_HELLO: {
      std::string proto_min;
      std::string proto_max;
      json::get_string(frame.payload, "proto_min", proto_min);
      json::get_string(frame.payload, "proto_max", proto_max);
      const bool ok = (proto_min == PROTO_VERSION) || (proto_max == PROTO_VERSION) ||
                      (proto_min.empty() && proto_max.empty());
      std::string ack;
      if (ok) {
        ack = std::string("{\"ok\":true,\"proto\":\"") + PROTO_VERSION +
              "\",\"server\":\"" + SERVER_NAME + "\"}";
        send_frame(client, MSG_HELLO_ACK, 0, frame.seq, ack);
        client->hello_ok = true;
        const std::string raw_state = state_provider_ ? state_provider_() : "{}";
        if (raw_state.find("\"battery\"") == std::string::npos) {
          log_("[tcp] robot state has no battery/pose — start fake_robot and "
               "lifecycle configure+activate");
        }
        const std::string state = inject_runtime_fields(raw_state, "ONLINE");
        send_frame(client, MSG_STATE_SNAPSHOT, 0, 0, state);
      } else {
        ack = "{\"ok\":false,\"reason\":\"proto_unsupported\"}";
        send_frame(client, MSG_HELLO_ACK, 0, frame.seq, ack);
      }
      break;
    }
    case MSG_HEARTBEAT: {
      client->last_hb_rx_ms = now_ms();
      const std::string pong =
          std::string("{\"ts_ms\":") + std::to_string(now_ms()) +
          ",\"role\":\"bridge\"}";
      send_frame(client, MSG_HEARTBEAT, 0, frame.seq, pong);
      break;
    }
    case MSG_CMD_MODE:
    case MSG_CMD_TASK:
    case MSG_CMD_ESTOP:
    case MSG_CMD_RESET:
    case MSG_PARAM_GET:
    case MSG_PARAM_SET: {
      std::string cmd_name = msg_type_name(frame.type);
      std::string ack_payload;
      if (!client->hello_ok) {
        ack_payload =
            "{\"seq\":" + std::to_string(frame.seq) +
            ",\"ok\":false,\"cmd\":\"" + cmd_name +
            "\",\"status\":\"REJECTED\",\"reason\":\"proto_error\"}";
      } else if (!cmd_handler_) {
        ack_payload =
            "{\"seq\":" + std::to_string(frame.seq) +
            ",\"ok\":false,\"cmd\":\"" + cmd_name +
            "\",\"status\":\"REJECTED\",\"reason\":\"not_implemented\"}";
      } else {
        ack_payload = cmd_handler_(cmd_name, frame.seq, frame.payload);
      }
      send_frame(client, MSG_CMD_ACK, 0, frame.seq, ack_payload);
      break;
    }
    default: {
      if (log_) {
        log_("[tcp] ignore unhandled type 0x" +
             std::to_string(static_cast<int>(frame.type)));
      }
      break;
    }
  }
}

void TcpServer::broadcast_state_delta()
{
  if (!running_.load()) {
    return;
  }
  const std::string raw = state_provider_ ? state_provider_() : "{}";
  const std::string body = inject_runtime_fields(raw, "ONLINE");
  std::vector<std::shared_ptr<Client>> snapshot;
  {
    std::lock_guard<std::mutex> lock(clients_mu_);
    snapshot = clients_;
  }
  for (auto& c : snapshot) {
    if (c && c->hello_ok && c->fd >= 0) {
      // Week-1 allows full snapshot body on DELTA.
      send_frame(c, MSG_STATE_DELTA, 0, 0, body);
    }
  }
}

void TcpServer::heartbeat_watchdog()
{
  while (running_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const uint64_t now = now_ms();
    std::vector<std::shared_ptr<Client>> snapshot;
    {
      std::lock_guard<std::mutex> lock(clients_mu_);
      snapshot = clients_;
    }
    for (auto& c : snapshot) {
      if (!c || c->fd < 0) {
        continue;
      }
      if (!c->hello_ok) {
        continue;  // HELLO grace handled by client_loop disconnect
      }
      if (now - c->last_hb_rx_ms > static_cast<uint64_t>(HB_TIMEOUT_MS)) {
        if (log_) {
          log_("[tcp] heartbeat timeout, dropping client");
        }
        ::shutdown(c->fd, SHUT_RDWR);
      }
    }
  }
}

}  // namespace botcockpit
