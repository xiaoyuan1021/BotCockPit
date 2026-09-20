#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "botcockpit_bridge/frame_codec.hpp"

namespace botcockpit {

// Blocking TCP server for protocol v0.1. Handles sticky/half frames.
class TcpServer {
 public:
  // Latest robot state JSON (without conn/rtt injection).
  using StateProvider = std::function<std::string()>;
  // cmd_name e.g. "CMD_ESTART"; payload JSON; returns CMD_ACK payload JSON.
  // Must not block for long; bridge_node may wait on a future.
  using CommandHandler =
      std::function<std::string(const std::string& cmd_name, uint16_t seq,
                                const std::string& payload)>;
  using LogFn = std::function<void(const std::string& line)>;

  TcpServer(StateProvider state_provider, CommandHandler cmd_handler, LogFn log);
  ~TcpServer();

  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;

  // Bind 0.0.0.0:port and start accept thread. Returns false on bind failure.
  bool start(int port);
  void stop();

  bool running() const { return running_.load(); }
  int port() const { return port_; }
  size_t client_count() const;

  // Broadcast STATE_DELTA (full snapshot body is allowed in week 1).
  void broadcast_state_delta();

  void set_state_provider(StateProvider fn);

 private:
  struct Client {
    int fd = -1;
    FrameDecoder decoder;
    uint64_t last_rx_ms = 0;
    uint64_t last_hb_rx_ms = 0;
    bool hello_ok = false;
    std::mutex send_mu;
  };

  void accept_loop();
  void client_loop(std::shared_ptr<Client> client);
  void handle_frame(const std::shared_ptr<Client>& client, const Frame& frame);
  bool send_frame(const std::shared_ptr<Client>& client, uint8_t type,
                  uint8_t flags, uint16_t seq, const std::string& payload);
  void remove_client(int fd);
  void heartbeat_watchdog();
  std::string inject_runtime_fields(const std::string& state_json,
                                    const std::string& conn_value);
  static uint64_t now_ms();

  StateProvider state_provider_;
  CommandHandler cmd_handler_;
  LogFn log_;

  int listen_fd_ = -1;
  int port_ = 0;
  std::atomic<bool> running_{false};
  std::thread accept_thread_;
  std::thread watchdog_thread_;

  mutable std::mutex clients_mu_;
  std::vector<std::shared_ptr<Client>> clients_;
};

}  // namespace botcockpit
