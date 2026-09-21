#include "botcockpit_bridge/tcp_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
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
                     UrgentHandler urgent_handler, LogFn log)
    : state_provider_(std::move(state_provider)),
      cmd_handler_(std::move(cmd_handler)),
      urgent_handler_(std::move(urgent_handler)),
      log_(std::move(log))
{
}

TcpServer::~TcpServer()
{
  stop();
}

void TcpServer::set_state_provider(StateProvider fn)
{
  std::lock_guard<std::mutex> lock(clients_mu_);
  state_provider_ = std::move(fn);
}

bool TcpServer::start(int port)
{
  if (running_.load()) {
    return true;
  }
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    return false;
  }
  int yes = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  if (::listen(listen_fd_, 8) < 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  port_ = port;
  running_.store(true);
  accept_thread_ = std::thread([this]() { accept_loop(); });
  watchdog_thread_ = std::thread([this]() { heartbeat_watchdog(); });
  cmd_thread_ = std::thread([this]() { cmd_worker_loop(); });
  if (log_) {
    log_("[tcp] listening on 0.0.0.0:" + std::to_string(port));
  }
  return true;
}

void TcpServer::stop()
{
  const bool was_running = running_.exchange(false);
  queue_cv_.notify_all();
  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
    listen_fd_ = -1;
  }
  {
    std::lock_guard<std::mutex> lock(clients_mu_);
    for (auto& c : clients_) {
      if (c) {
        const int cfd = c->fd.exchange(-1);
        if (cfd >= 0) {
          ::shutdown(cfd, SHUT_RDWR);
          ::close(cfd);
        }
      }
    }
  }
  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }
  if (watchdog_thread_.joinable()) {
    watchdog_thread_.join();
  }
  if (cmd_thread_.joinable()) {
    cmd_thread_.join();
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
    if (c && c->fd.load() >= 0 && c->hello_ok) {
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
    client->fd.store(fd);
    client->last_rx_ms = now_ms();
    client->last_hb_rx_ms = client->last_rx_ms;
    {
      std::lock_guard<std::mutex> lock(clients_mu_);
      clients_.push_back(client);
    }
    if (log_) {
      char ip[64] = {0};
      ::inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
      log_("[tcp] client connected " + std::string(ip));
    }
    std::thread([this, client]() { client_loop(client); }).detach();
  }
}

void TcpServer::client_loop(std::shared_ptr<Client> client)
{
  uint8_t buf[kReadChunk];
  while (running_.load()) {
    const int fd = client->fd.load();
    if (fd < 0) {
      break;
    }
    const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n == 0) {
      break;
    }
    if (n < 0) {
      if (!running_.load() || client->fd.load() < 0) {
        break;
      }
      continue;
    }
    client->last_rx_ms = now_ms();
    client->decoder.append(buf, static_cast<size_t>(n));
    std::vector<Frame> batch;
    Frame frame;
    while (client->decoder.next(frame)) {
      batch.push_back(frame);
    }
    // PROTOCOL §7.1: handle ESTOP first in this recv batch; non-urgent CMDs
    // are queued so recv thread is never blocked on ACK wait.
    std::stable_sort(batch.begin(), batch.end(),
                     [](const Frame& a, const Frame& b) {
                       return (a.type == MSG_CMD_ESTOP) > (b.type == MSG_CMD_ESTOP);
                     });
    for (const Frame& f : batch) {
      handle_frame(client, f);
      if (client->fd.load() < 0) {
        break;
      }
    }
  }
  const int fd = client->fd.exchange(-1);
  if (fd >= 0) {
    ::close(fd);
  }
  remove_client(-1, client);
  if (log_) {
    log_("[tcp] client disconnected");
  }
}

void TcpServer::remove_client(int fd)
{
  remove_client(fd, nullptr);
}

void TcpServer::remove_client(int fd, const std::shared_ptr<Client>& client)
{
  std::lock_guard<std::mutex> lock(clients_mu_);
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (!*it) {
      continue;
    }
    if ((client && *it == client) || (fd >= 0 && (*it)->fd.load() == fd)) {
      clients_.erase(it);
      break;
    }
  }
}

bool TcpServer::send_frame(const std::shared_ptr<Client>& client, uint8_t type,
                           uint8_t flags, uint16_t seq,
                           const std::string& payload)
{
  if (!client) {
    return false;
  }
  const int fd = client->fd.load();
  if (fd < 0) {
    return false;
  }
  const auto bytes = encode_frame(type, flags, seq, payload);
  std::lock_guard<std::mutex> lock(client->send_mu);
  size_t off = 0;
  while (off < bytes.size()) {
    const ssize_t n =
        ::send(fd, bytes.data() + off, bytes.size() - off, MSG_NOSIGNAL);
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
  if (!state_json.empty() && state_json.find("\"conn\"") != std::string::npos) {
    return state_json;
  }
  std::string body = state_json.empty() || state_json[0] != '{' ? "{}" : state_json;
  if (!body.empty() && body.back() == '}') {
    body.pop_back();
  }
  if (!body.empty() && body.back() != ',' && body.back() != '{') {
    body += ",";
  }
  body += "\"conn\":\"" + json::escape(conn_value) + "\"";
  body += ",\"heartbeat_rtt_ms\":0}";
  return body;
}

void TcpServer::enqueue_cmd(const std::shared_ptr<Client>& client,
                            const Frame& frame, const std::string& cmd_name)
{
  CmdJob job;
  job.client = client;
  job.seq = frame.seq;
  job.cmd_name = cmd_name;
  job.payload = frame.payload;
  job.urgent = (frame.type == MSG_CMD_ESTOP);
  if (!job.urgent) {
    std::lock_guard<std::mutex> lock(queue_mu_);
    cmd_queue_.push_back(job);
    queue_cv_.notify_one();
    return;
  }
  // Urgent: call non-blocking handler on recv thread immediately (§7.1).
  if (urgent_handler_) {
    urgent_handler_(cmd_name, job.seq, job.payload);
    if (log_) {
      log_("[tcp] URGENT " + cmd_name + " seq=" + std::to_string(job.seq));
    }
  } else if (cmd_handler_) {
    const std::string ack = cmd_handler_(cmd_name, job.seq, job.payload);
    send_frame(client, MSG_CMD_ACK, 0, job.seq, ack);
  }
}

void TcpServer::cmd_worker_loop()
{
  while (true) {
    CmdJob job;
    {
      std::unique_lock<std::mutex> lock(queue_mu_);
      queue_cv_.wait(lock, [this]() {
        return !running_.load() || !cmd_queue_.empty();
      });
      if (!running_.load() && cmd_queue_.empty()) {
        return;
      }
      if (cmd_queue_.empty()) {
        continue;
      }
      job = cmd_queue_.front();
      cmd_queue_.pop_front();
    }
    if (!job.client || job.client->fd.load() < 0) {
      continue;
    }
    std::string ack_payload;
    if (!job.client->hello_ok) {
      ack_payload = "{\"seq\":" + std::to_string(job.seq) +
                    ",\"ok\":false,\"cmd\":\"" + job.cmd_name +
                    "\",\"status\":\"REJECTED\",\"reason\":\"proto_error\"}";
    } else if (!cmd_handler_) {
      ack_payload = "{\"seq\":" + std::to_string(job.seq) +
                    ",\"ok\":false,\"cmd\":\"" + job.cmd_name +
                    "\",\"status\":\"REJECTED\",\"reason\":\"not_implemented\"}";
    } else {
      ack_payload = cmd_handler_(job.cmd_name, job.seq, job.payload);
    }
    send_frame(job.client, MSG_CMD_ACK, 0, job.seq, ack_payload);
  }
}

void TcpServer::complete_async_ack(uint16_t seq, const std::string& ack_json)
{
  std::vector<std::shared_ptr<Client>> snapshot;
  {
    std::lock_guard<std::mutex> lock(clients_mu_);
    snapshot = clients_;
  }
  for (auto& c : snapshot) {
    if (c && c->hello_ok && c->fd.load() >= 0) {
      send_frame(c, MSG_CMD_ACK, 0, seq, ack_json);
    }
  }
}

void TcpServer::handle_frame(const std::shared_ptr<Client>& client,
                             const Frame& frame)
{
  switch (frame.type) {
    case MSG_HELLO: {
      std::string proto_min;
      std::string proto_max;
      json::get_string(frame.payload, "proto_min", proto_min);
      json::get_string(frame.payload, "proto_max", proto_max);
      const bool ok = (proto_min == PROTO_VERSION) || (proto_max == PROTO_VERSION) ||
                      (proto_min.empty() && proto_max.empty());
      if (ok) {
        const std::string ack = std::string("{\"ok\":true,\"proto\":\"") +
                                PROTO_VERSION + "\",\"server\":\"" + SERVER_NAME +
                                "\"}";
        send_frame(client, MSG_HELLO_ACK, 0, frame.seq, ack);
        client->hello_ok = true;
        const std::string raw_state = state_provider_ ? state_provider_() : "{}";
        send_frame(client, MSG_STATE_SNAPSHOT, 0, 0,
                   inject_runtime_fields(raw_state, "OFFLINE"));
      } else {
        send_frame(client, MSG_HELLO_ACK, 0, frame.seq,
                   "{\"ok\":false,\"reason\":\"proto_unsupported\"}");
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
      enqueue_cmd(client, frame, msg_type_name(frame.type));
      break;
    }
    default:
      break;
  }
}

void TcpServer::broadcast_state_delta()
{
  if (!running_.load()) {
    return;
  }
  std::string raw = "{}";
  {
    std::lock_guard<std::mutex> lock(clients_mu_);
    if (state_provider_) {
      raw = state_provider_();
    }
  }
  const std::string body = inject_runtime_fields(raw, "OFFLINE");
  std::vector<std::shared_ptr<Client>> snapshot;
  {
    std::lock_guard<std::mutex> lock(clients_mu_);
    snapshot = clients_;
  }
  for (auto& c : snapshot) {
    if (c && c->hello_ok && c->fd.load() >= 0) {
      send_frame(c, MSG_STATE_DELTA, 0, 0, body);
    }
  }
}

void TcpServer::heartbeat_watchdog()
{
  uint64_t last_tx = 0;
  while (running_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const uint64_t now = now_ms();
    std::vector<std::shared_ptr<Client>> snapshot;
    {
      std::lock_guard<std::mutex> lock(clients_mu_);
      snapshot = clients_;
    }
    if (now - last_tx >= static_cast<uint64_t>(HB_INTERVAL_MS)) {
      last_tx = now;
      const std::string hb = std::string("{\"ts_ms\":") + std::to_string(now) +
                             ",\"role\":\"bridge\"}";
      for (auto& c : snapshot) {
        if (c && c->hello_ok && c->fd.load() >= 0) {
          send_frame(c, MSG_HEARTBEAT, 0, 0, hb);
        }
      }
    }
    for (auto& c : snapshot) {
      if (!c) {
        continue;
      }
      const int fd = c->fd.load();
      if (fd < 0) {
        continue;
      }
      const uint64_t ref = c->hello_ok ? c->last_hb_rx_ms : c->last_rx_ms;
      const uint64_t limit = c->hello_ok ? static_cast<uint64_t>(HB_TIMEOUT_MS)
                                          : static_cast<uint64_t>(HB_TIMEOUT_MS * 2);
      if (ref > 0 && now - ref > limit) {
        ::shutdown(fd, SHUT_RDWR);
      }
    }
  }
}

}  // namespace botcockpit
