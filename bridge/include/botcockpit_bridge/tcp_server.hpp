#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "botcockpit_bridge/frame_codec.hpp"

namespace botcockpit {

class TcpServer {
 public:
  using StateProvider = std::function<std::string()>;
  using CommandHandler =
      std::function<std::string(const std::string& cmd_name, uint16_t seq,
                                const std::string& payload)>;
  // Non-blocking urgent path (ESTOP): publish ROS immediately, ACK later.
  using UrgentHandler =
      std::function<void(const std::string& cmd_name, uint16_t seq,
                         const std::string& payload)>;
  using LogFn = std::function<void(const std::string& line)>;

  TcpServer(StateProvider state_provider, CommandHandler cmd_handler,
            UrgentHandler urgent_handler, LogFn log);
  ~TcpServer();

  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;

  bool start(int port);
  void stop();

  bool running() const { return running_.load(); }
  int port() const { return port_; }
  size_t client_count() const;

  void broadcast_state_delta();
  void set_state_provider(StateProvider fn);
  // Called from bridge ROS callback to complete async ESTOP ACK.
  void complete_async_ack(uint16_t seq, const std::string& ack_json);

 private:
  struct Client {
    std::atomic<int> fd{-1};
    FrameDecoder decoder;
    uint64_t last_rx_ms = 0;
    uint64_t last_hb_rx_ms = 0;
    bool hello_ok = false;
    std::mutex send_mu;
  };

  struct CmdJob {
    std::shared_ptr<Client> client;
    uint16_t seq = 0;
    std::string cmd_name;
    std::string payload;
    bool urgent = false;
  };

  void accept_loop();
  void client_loop(std::shared_ptr<Client> client);
  void handle_frame(const std::shared_ptr<Client>& client, const Frame& frame);
  void enqueue_cmd(const std::shared_ptr<Client>& client, const Frame& frame,
                   const std::string& cmd_name);
  void cmd_worker_loop();
  bool send_frame(const std::shared_ptr<Client>& client, uint8_t type,
                  uint8_t flags, uint16_t seq, const std::string& payload);
  void remove_client(int fd);
  void remove_client(int fd, const std::shared_ptr<Client>& client);
  void heartbeat_watchdog();
  std::string inject_runtime_fields(const std::string& state_json,
                                    const std::string& conn_value);
  static uint64_t now_ms();

  StateProvider state_provider_;
  CommandHandler cmd_handler_;
  UrgentHandler urgent_handler_;
  LogFn log_;

  int listen_fd_ = -1;
  int port_ = 0;
  std::atomic<bool> running_{false};
  std::thread accept_thread_;
  std::thread watchdog_thread_;
  std::thread cmd_thread_;

  std::mutex queue_mu_;
  std::condition_variable queue_cv_;
  std::deque<CmdJob> cmd_queue_;

  mutable std::mutex clients_mu_;
  std::vector<std::shared_ptr<Client>> clients_;
};

}  // namespace botcockpit
