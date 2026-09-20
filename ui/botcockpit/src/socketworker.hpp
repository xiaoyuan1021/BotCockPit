#pragma once

#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "protocol.hpp"

class QTcpSocket;

// Runs on a worker thread: TCP + protocol v0.1 client side.
class SocketWorker : public QObject
{
    Q_OBJECT
public:
    explicit SocketWorker(QObject* parent = nullptr);
    ~SocketWorker() override;

public slots:
    void connectToHost(const QString& host, int port);
    void disconnectFromHost();
    void sendCmdMode(const QString& mode);
    void sendCmdTask(const QString& taskId, const QString& type, double x,
                     double y, double timeoutS);
    void sendCmdEstop(const QString& reason);
    void sendCmdReset();

signals:
    void connected();
    void disconnected();
    void helloAck(const QVariantMap& info);
    void stateUpdated(const QVariantMap& state);
    void nodesUpdated(const QVariantList& nodes);
    void rttUpdated(int ms);
    void cmdAck(const QVariantMap& ack);
    void errorOccurred(const QString& message);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onReadyRead();
    void onHeartbeatTick();

private:
    uint16_t sendFrame(uint8_t type, uint8_t flags, const std::string& payload);
    void handleFrame(const botcockpit_ui::Frame& frame);
    void failAndClose(const QString& message);
    static uint64_t nowMs();

    QTcpSocket* socket_ = nullptr;
    QTimer* hb_timer_ = nullptr;
    QTimer* timeout_timer_ = nullptr;
    botcockpit_ui::FrameDecoder decoder_;
    uint16_t next_seq_ = 1;
    QHash<quint16, qint64> hb_send_ts_;
    QHash<quint16, QString> pending_cmd_name_;
    qint64 last_rx_ms_ = 0;
    bool hello_sent_ = false;
    bool hello_ok_ = false;
};
