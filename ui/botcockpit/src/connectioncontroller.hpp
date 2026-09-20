#pragma once

#include <QObject>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>

class RobotState;
class NodeTableModel;
class SocketWorker;

// UI-thread facade. Socket lives in SocketWorker on a worker QThread.
class ConnectionController : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionController(RobotState* state, NodeTableModel* nodes,
                                  QObject* parent = nullptr);
    ~ConnectionController() override;

    Q_INVOKABLE void connectToServer(const QString& host, int port);
    Q_INVOKABLE void disconnectFromServer();

signals:
    void startWorker(const QString& host, int port);
    void stopWorker();

private slots:
    void onConnected();
    void onDisconnected();
    void onHelloAck(const QVariantMap& info);
    void onState(const QVariantMap& state);
    void onNodes(const QVariantList& nodes);
    void onRtt(int ms);
    void onError(const QString& message);

private:
    RobotState* state_ = nullptr;
    NodeTableModel* nodes_ = nullptr;
    QThread worker_thread_;
    SocketWorker* worker_ = nullptr;
};
