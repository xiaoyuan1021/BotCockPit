#include "socketworker.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>

namespace {
constexpr int kHeartbeatIntervalMs = 1000;
constexpr int kHeartbeatTimeoutMs = 3000;
}

uint64_t SocketWorker::nowMs()
{
    return static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch());
}

SocketWorker::SocketWorker(QObject* parent) : QObject(parent)
{
    socket_ = new QTcpSocket(this);
    connect(socket_, &QTcpSocket::connected, this, &SocketWorker::onSocketConnected);
    connect(socket_, &QTcpSocket::disconnected, this,
            &SocketWorker::onSocketDisconnected);
    connect(socket_, &QTcpSocket::readyRead, this, &SocketWorker::onReadyRead);
    connect(socket_,
            QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this,
            [this](QAbstractSocket::SocketError) {
                failAndClose(socket_->errorString());
            });

    hb_timer_ = new QTimer(this);
    hb_timer_->setInterval(kHeartbeatIntervalMs);
    connect(hb_timer_, &QTimer::timeout, this, &SocketWorker::onHeartbeatTick);

    timeout_timer_ = new QTimer(this);
    timeout_timer_->setInterval(500);
    connect(timeout_timer_, &QTimer::timeout, this, [this]() {
        if (!socket_ || socket_->state() != QAbstractSocket::ConnectedState) {
            return;
        }
        if (last_rx_ms_ == 0) {
            return;
        }
        if (nowMs() - last_rx_ms_ > static_cast<qint64>(kHeartbeatTimeoutMs)) {
            failAndClose(QStringLiteral("heartbeat timeout"));
        }
    });
}

SocketWorker::~SocketWorker()
{
    disconnectFromHost();
}

void SocketWorker::connectToHost(const QString& host, int port)
{
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->abort();
    }
    decoder_.clear();
    hb_send_ts_.clear();
    pending_cmd_name_.clear();
    hello_sent_ = false;
    hello_ok_ = false;
    last_rx_ms_ = nowMs();
    socket_->connectToHost(host, static_cast<quint16>(port));
}

void SocketWorker::disconnectFromHost()
{
    hb_timer_->stop();
    timeout_timer_->stop();
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->disconnectFromHost();
        if (socket_->state() != QAbstractSocket::UnconnectedState) {
            socket_->abort();
        }
    }
}

void SocketWorker::onSocketConnected()
{
    emit connected();
    last_rx_ms_ = nowMs();

    const QByteArray hello =
        QJsonDocument(QJsonObject{
                          {QStringLiteral("client"), QStringLiteral("BotCockpit")},
                          {QStringLiteral("ui_version"), QStringLiteral("0.1.0")},
                          {QStringLiteral("proto_min"), QStringLiteral("0.1")},
                          {QStringLiteral("proto_max"), QStringLiteral("0.1")},
                      })
            .toJson(QJsonDocument::Compact);
    sendFrame(botcockpit_ui::MSG_HELLO, 0, std::string(hello.constData(), hello.size()));
    hello_sent_ = true;
    hb_timer_->start();
    timeout_timer_->start();
}

void SocketWorker::onSocketDisconnected()
{
    hb_timer_->stop();
    timeout_timer_->stop();
    emit disconnected();
}

void SocketWorker::failAndClose(const QString& message)
{
    emit errorOccurred(message);
    disconnectFromHost();
}

uint16_t SocketWorker::sendFrame(uint8_t type, uint8_t flags,
                                 const std::string& payload)
{
    if (!socket_ || socket_->state() != QAbstractSocket::ConnectedState) {
        return 0;
    }
    const uint16_t seq = next_seq_++;
    if (next_seq_ == 0) {
        next_seq_ = 1;
    }
    const auto bytes = botcockpit_ui::encode_frame(type, flags, seq, payload);
    socket_->write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<qint64>(bytes.size()));
    return seq;
}

void SocketWorker::sendCmdMode(const QString& mode)
{
    const QByteArray payload =
        QJsonDocument(QJsonObject{{QStringLiteral("mode"), mode}})
            .toJson(QJsonDocument::Compact);
    const uint16_t seq = sendFrame(
        botcockpit_ui::MSG_CMD_MODE, botcockpit_ui::FLAG_NEED_ACK,
        std::string(payload.constData(), payload.size()));
    if (seq) {
        pending_cmd_name_.insert(seq, QStringLiteral("CMD_MODE"));
    }
}

void SocketWorker::sendCmdTask(const QString& taskId, const QString& type,
                               double x, double y, double timeoutS)
{
    QJsonObject obj{
        {QStringLiteral("task_id"), taskId},
        {QStringLiteral("type"), type},
    };
    if (type == QLatin1String("goto")) {
        obj.insert(QStringLiteral("x"), x);
        obj.insert(QStringLiteral("y"), y);
        obj.insert(QStringLiteral("timeout_s"), timeoutS);
    }
    const QByteArray payload =
        QJsonDocument(obj).toJson(QJsonDocument::Compact);
    const uint16_t seq = sendFrame(
        botcockpit_ui::MSG_CMD_TASK, botcockpit_ui::FLAG_NEED_ACK,
        std::string(payload.constData(), payload.size()));
    if (seq) {
        pending_cmd_name_.insert(seq, QStringLiteral("CMD_TASK"));
    }
}

void SocketWorker::sendCmdEstop(const QString& reason)
{
    const QByteArray payload =
        QJsonDocument(QJsonObject{{QStringLiteral("reason"), reason}})
            .toJson(QJsonDocument::Compact);
    const uint16_t seq = sendFrame(botcockpit_ui::MSG_CMD_ESTOP,
                                   botcockpit_ui::FLAG_URGENT |
                                       botcockpit_ui::FLAG_NEED_ACK,
                                   std::string(payload.constData(), payload.size()));
    if (seq) {
        pending_cmd_name_.insert(seq, QStringLiteral("CMD_ESTOP"));
    }
}

void SocketWorker::sendCmdReset()
{
    const QByteArray payload =
        QJsonDocument(QJsonObject{{QStringLiteral("confirm"), true}})
            .toJson(QJsonDocument::Compact);
    const uint16_t seq = sendFrame(
        botcockpit_ui::MSG_CMD_RESET, botcockpit_ui::FLAG_NEED_ACK,
        std::string(payload.constData(), payload.size()));
    if (seq) {
        pending_cmd_name_.insert(seq, QStringLiteral("CMD_RESET"));
    }
}

void SocketWorker::onHeartbeatTick()
{
    if (!socket_ || socket_->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    const qint64 ts = QDateTime::currentMSecsSinceEpoch();
    const QByteArray payload =
        QJsonDocument(QJsonObject{
                          {QStringLiteral("ts_ms"), ts},
                          {QStringLiteral("role"), QStringLiteral("console")},
                      })
            .toJson(QJsonDocument::Compact);
    const uint16_t seq = next_seq_++;
    if (next_seq_ == 0) {
        next_seq_ = 1;
    }
    hb_send_ts_.insert(seq, ts);
    const auto bytes = botcockpit_ui::encode_frame(
        botcockpit_ui::MSG_HEARTBEAT, 0, seq,
        std::string(payload.constData(), payload.size()));
    socket_->write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<qint64>(bytes.size()));
}

void SocketWorker::onReadyRead()
{
    const QByteArray data = socket_->readAll();
    last_rx_ms_ = nowMs();
    decoder_.append(data.constData(), static_cast<size_t>(data.size()));
    botcockpit_ui::Frame frame;
    while (decoder_.next(frame)) {
        handleFrame(frame);
    }
}

void SocketWorker::handleFrame(const botcockpit_ui::Frame& frame)
{
    using namespace botcockpit_ui;
    const QByteArray payload(frame.payload.data(),
                             static_cast<int>(frame.payload.size()));
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    const QJsonObject obj = doc.object();

    switch (frame.type) {
    case MSG_HELLO_ACK: {
        QVariantMap info;
        info.insert(QStringLiteral("ok"), obj.value(QStringLiteral("ok")).toBool());
        info.insert(QStringLiteral("proto"),
                    obj.value(QStringLiteral("proto")).toString());
        info.insert(QStringLiteral("server"),
                    obj.value(QStringLiteral("server")).toString());
        info.insert(QStringLiteral("reason"),
                    obj.value(QStringLiteral("reason")).toString());
        hello_ok_ = obj.value(QStringLiteral("ok")).toBool(false);
        emit helloAck(info);
        break;
    }
    case MSG_HEARTBEAT: {
        if (obj.value(QStringLiteral("role")).toString() ==
            QStringLiteral("bridge")) {
            const qint64 sendTs = hb_send_ts_.take(frame.seq);
            if (sendTs > 0) {
                const qint64 rtt = QDateTime::currentMSecsSinceEpoch() - sendTs;
                emit rttUpdated(static_cast<int>(rtt));
            }
        }
        break;
    }
    case MSG_STATE_SNAPSHOT:
    case MSG_STATE_DELTA: {
        QVariantMap state = obj.toVariantMap();
        emit stateUpdated(state);
        if (obj.contains(QStringLiteral("nodes"))) {
            emit nodesUpdated(obj.value(QStringLiteral("nodes")).toArray().toVariantList());
        }
        break;
    }
    case MSG_CMD_ACK: {
        QVariantMap ack = obj.toVariantMap();
        if (!ack.contains(QStringLiteral("seq")) ||
            ack.value(QStringLiteral("seq")).isNull()) {
            ack.insert(QStringLiteral("seq"), frame.seq);
        }
        if (!ack.contains(QStringLiteral("cmd"))) {
            ack.insert(QStringLiteral("cmd"), pending_cmd_name_.value(frame.seq));
        }
        pending_cmd_name_.remove(frame.seq);
        emit cmdAck(ack);
        break;
    }
    default:
        break;
    }
}
