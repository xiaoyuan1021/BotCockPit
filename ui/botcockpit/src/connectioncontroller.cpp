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
    if (state_->connected() || state_->connecting()) {
        return;
    }
    state_->setEndpoint(host, port);
    state_->setConnecting(true);
    state_->setErrorString(QString());
    emit startWorker(host, port);
}

void ConnectionController::disconnectFromServer()
{
    emit stopWorker();
}

void ConnectionController::cmdMode(const QString& mode)
{
    if (state_ && !state_->connected()) {
        return;
    }
    emit workerCmdMode(mode);
}

void ConnectionController::cmdTaskGoto(const QString& taskId, double x, double y)
{
    if (state_ && !state_->connected()) {
        return;
    }
    emit workerCmdTask(taskId, QStringLiteral("goto"), x, y, 30.0);
}

void ConnectionController::cmdTaskSimple(const QString& type)
{
    if (state_ && !state_->connected()) {
        return;
    }
    emit workerCmdTask(QString(), type, 0.0, 0.0, 0.0);
}

void ConnectionController::cmdEstop(const QString& reason)
{
    if (state_ && !state_->connected()) {
        return;
    }
    emit workerCmdEstop(reason);
}

void ConnectionController::cmdReset()
{
    if (state_ && !state_->connected()) {
        return;
    }
    emit workerCmdReset();
}

void ConnectionController::onConnected()
{
    if (!state_) {
        return;
    }
    state_->setConnecting(false);
    state_->setConnected(true);
    state_->setErrorString(QString());
}

void ConnectionController::onDisconnected()
{
    if (!state_) {
        return;
    }
    state_->setConnecting(false);
    state_->setConnected(false);
    state_->resetRemote();
    if (nodes_) {
        nodes_->setNodes({});
    }
}

void ConnectionController::onHelloAck(const QVariantMap& info)
{
    if (!state_) {
        return;
    }
    const bool ok = info.value(QStringLiteral("ok")).toBool();
    if (!ok) {
        state_->setErrorString(
            info.value(QStringLiteral("reason")).toString());
        return;
    }
    state_->setHelloInfo(info.value(QStringLiteral("proto")).toString(),
                         info.value(QStringLiteral("server")).toString());
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
    }
}

void ConnectionController::onError(const QString& message)
{
    if (!state_) {
        return;
    }
    state_->setConnecting(false);
    state_->setConnected(false);
    state_->setErrorString(message);
    state_->resetRemote();
}
