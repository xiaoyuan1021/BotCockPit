#include "connectioncontroller.hpp"

#include <QMetaObject>

#include "nodetablemodel.hpp"
#include "robotstate.hpp"
#include "socketworker.hpp"

ConnectionController::ConnectionController(RobotState* state,
                                           NodeTableModel* nodes,
                                           QObject* parent)
    : QObject(parent), state_(state), nodes_(nodes)
{
    worker_ = new SocketWorker();
    worker_->moveToThread(&worker_thread_);
    connect(&worker_thread_, &QThread::finished, worker_, &QObject::deleteLater);

    reconnect_timer_.setSingleShot(true);
    connect(&reconnect_timer_, &QTimer::timeout, this,
            &ConnectionController::onReconnectTimer);

    connect(this, &ConnectionController::startWorker, worker_,
            &SocketWorker::connectToHost);
    connect(this, &ConnectionController::stopWorker, worker_,
            &SocketWorker::disconnectFromHost);
    connect(this, &ConnectionController::workerCmdMode, worker_,
            &SocketWorker::sendCmdMode);
    connect(this, &ConnectionController::workerCmdTask, worker_,
            &SocketWorker::sendCmdTask);
    connect(this, &ConnectionController::workerCmdEstop, worker_,
            &SocketWorker::sendCmdEstop);
    connect(this, &ConnectionController::workerCmdReset, worker_,
            &SocketWorker::sendCmdReset);

    connect(worker_, &SocketWorker::connected, this,
            &ConnectionController::onConnected, Qt::QueuedConnection);
    connect(worker_, &SocketWorker::disconnected, this,
            &ConnectionController::onDisconnected, Qt::QueuedConnection);
    connect(worker_, &SocketWorker::helloAck, this,
            &ConnectionController::onHelloAck, Qt::QueuedConnection);
    connect(worker_, &SocketWorker::stateUpdated, this,
            &ConnectionController::onState, Qt::QueuedConnection);
    connect(worker_, &SocketWorker::nodesUpdated, this,
            &ConnectionController::onNodes, Qt::QueuedConnection);
    connect(worker_, &SocketWorker::rttUpdated, this,
            &ConnectionController::onRtt, Qt::QueuedConnection);
    connect(worker_, &SocketWorker::cmdAck, this,
            &ConnectionController::onCmdAck, Qt::QueuedConnection);
    connect(worker_, &SocketWorker::errorOccurred, this,
            &ConnectionController::onError, Qt::QueuedConnection);

    worker_thread_.start();
}

ConnectionController::~ConnectionController()
{
    manual_disconnect_ = true;
    reconnect_timer_.stop();
    if (worker_thread_.isRunning() && worker_) {
        QMetaObject::invokeMethod(worker_, "disconnectFromHost",
                                  Qt::BlockingQueuedConnection);
        worker_thread_.quit();
        if (!worker_thread_.wait(3000)) {
            worker_thread_.requestInterruption();
            worker_thread_.wait(1000);
        }
    }
}

void ConnectionController::connectToServer(const QString& host, int port)
{
    if (!state_) {
        return;
    }
    manual_disconnect_ = false;
    backoff_s_ = 1;
    reconnect_timer_.stop();
    if (state_->connected() || state_->connecting()) {
        return;
    }
    last_host_ = host;
    last_port_ = port;
    state_->setEndpoint(host, port);
    state_->setConnecting(true);
    state_->setErrorString(QString());
    state_->appendLog(tr("connect %1:%2").arg(host).arg(port));
    emit startWorker(host, port);
}

void ConnectionController::disconnectFromServer()
{
    manual_disconnect_ = true;
    reconnect_timer_.stop();
    if (state_) {
        state_->setReconnectAttempts(0);
        state_->appendLog(tr("manual disconnect"));
    }
    emit stopWorker();
}

void ConnectionController::cmdMode(const QString& mode)
{
    if (state_ && !state_->connected()) {
        return;
    }
    if (state_) {
        state_->appendLog(tr("CMD_MODE %1").arg(mode));
    }
    emit workerCmdMode(mode);
}

void ConnectionController::cmdTaskGoto(const QString& taskId, double x, double y)
{
    if (state_ && !state_->connected()) {
        return;
    }
    if (state_) {
        state_->appendLog(tr("CMD_TASK goto id=%1 (%2,%3)").arg(taskId).arg(x).arg(y));
    }
    emit workerCmdTask(taskId, QStringLiteral("goto"), x, y, 30.0);
}

void ConnectionController::cmdTaskSimple(const QString& type)
{
    if (state_ && !state_->connected()) {
        return;
    }
    if (state_) {
        state_->appendLog(tr("CMD_TASK %1").arg(type));
    }
    emit workerCmdTask(QString(), type, 0.0, 0.0, 0.0);
}

void ConnectionController::cmdEstop(const QString& reason)
{
    if (state_ && !state_->connected()) {
        return;
    }
    if (state_) {
        state_->appendLog(tr("CMD_ESTOP %1").arg(reason));
    }
    emit workerCmdEstop(reason);
}

void ConnectionController::cmdReset()
{
    if (state_ && !state_->connected()) {
        return;
    }
    if (state_) {
        state_->appendLog(tr("CMD_RESET"));
    }
    emit workerCmdReset();
}

void ConnectionController::scheduleReconnect(const QString& why)
{
    if (!state_ || manual_disconnect_ || !state_->autoReconnect()) {
        return;
    }
    if (last_host_.isEmpty()) {
        return;
    }
    // Single-flight: do not stack timers / inflate backoff twice
    if (reconnect_pending_ && reconnect_timer_.isActive()) {
        return;
    }
    reconnect_pending_ = true;
    state_->setReconnectAttempts(state_->reconnectAttempts() + 1);
    const int delay = backoff_s_;
    backoff_s_ = qMin(backoff_s_ * 2, 10);
    state_->appendLog(tr("reconnect in %1s (%2) — %3")
                          .arg(delay)
                          .arg(state_->reconnectAttempts())
                          .arg(why));
    reconnect_timer_.start(delay * 1000);
}

void ConnectionController::onReconnectTimer()
{
    reconnect_pending_ = false;
    if (!state_ || manual_disconnect_) {
        return;
    }
    if (state_->connected()) {
        return;
    }
    state_->setConnecting(true);
    state_->setErrorString(QString());
    hello_ok_session_ = false;
    emit startWorker(last_host_, last_port_);
}

void ConnectionController::onConnected()
{
    if (!state_) {
        return;
    }
    // TCP up != protocol session up; backoff resets only after HELLO_ACK.
    reconnect_timer_.stop();
    reconnect_pending_ = false;
    hello_ok_session_ = false;
    state_->setConnecting(true);
    state_->setConnected(false);  // fail-closed until HELLO_ACK ok
    state_->setErrorString(QString());
    state_->appendLog(tr("TCP connected, waiting HELLO…"));
}

void ConnectionController::onDisconnected()
{
    if (!state_) {
        return;
    }
    const bool was_hello = hello_ok_session_;
    hello_ok_session_ = false;
    state_->setConnecting(false);
    state_->setConnected(false);
    state_->resetRemote();
    if (nodes_) {
        nodes_->setNodes({});
    }
    state_->appendLog(tr("disconnected"));
    if (!was_hello && !manual_disconnect_) {
        // TCP-level flapping without session — keep backoff growth
        scheduleReconnect(QStringLiteral("tcp/protocol disconnect"));
    } else {
        scheduleReconnect(QStringLiteral("disconnect"));
    }
}

void ConnectionController::onHelloAck(const QVariantMap& info)
{
    if (!state_) {
        return;
    }
    const bool ok = info.value(QStringLiteral("ok")).toBool();
    if (!ok) {
        const QString reason = info.value(QStringLiteral("reason")).toString();
        state_->setErrorString(reason);
        state_->appendLog(tr("HELLO rejected: %1").arg(reason));
        hello_ok_session_ = false;
        state_->setConnected(false);
        state_->setConnecting(false);
        // Fail closed: tear down socket, do not claim ONLINE
        manual_disconnect_ = false;  // allow auto-reconnect after handshake fail
        emit stopWorker();
        scheduleReconnect(QStringLiteral("hello rejected"));
        return;
    }
    state_->setHelloInfo(info.value(QStringLiteral("proto")).toString(),
                         info.value(QStringLiteral("server")).toString());
    hello_ok_session_ = true;
    reconnect_pending_ = false;
    reconnect_timer_.stop();
    state_->setReconnectAttempts(0);
    backoff_s_ = 1;  // reset only on successful session
    state_->setConnecting(false);
    state_->setConnected(true);
    state_->setErrorString(QString());
    state_->appendLog(tr("HELLO_ACK proto=%1 server=%2")
                          .arg(state_->proto(), state_->server()));
}

void ConnectionController::onState(const QVariantMap& state)
{
    if (state_) {
        state_->applyState(state);
    }
}

void ConnectionController::onNodes(const QVariantList& nodes)
{
    if (nodes_) {
        nodes_->setNodes(nodes);
    }
}

void ConnectionController::onRtt(int ms)
{
    if (state_) {
        state_->setHeartbeatRtt(ms);
    }
}

void ConnectionController::onCmdAck(const QVariantMap& ack)
{
    if (state_) {
        state_->setLastCmdAck(ack);
        const QString cmd = ack.value(QStringLiteral("cmd")).toString();
        const bool ok = ack.value(QStringLiteral("ok")).toBool();
        const QString reason = ack.value(QStringLiteral("reason")).toString();
        state_->appendLog(tr("ACK %1 %2%3")
                              .arg(cmd, ok ? QStringLiteral("OK") : QStringLiteral("NACK"),
                                   reason.isEmpty() ? QString() : (QStringLiteral(" ") + reason)));
    }
}

void ConnectionController::onError(const QString& message)
{
    if (!state_) {
        return;
    }
    hello_ok_session_ = false;
    state_->setConnecting(false);
    state_->setConnected(false);
    state_->setErrorString(message);
    state_->resetRemote();
    state_->appendLog(tr("error: %1").arg(message));
    // disconnect signal will schedule reconnect once — avoid double backoff
}
