#pragma once

#include <QObject>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>

class RobotState;
class NodeTableModel;
class SocketWorker;

// UI-thread facade. Socket lives in SocketWorker on a worker QThread.
// Protocol encode/decode stays in C++ — QML only calls these invokables.
class ConnectionController : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionController(RobotState* state, NodeTableModel* nodes,
                                  QObject* parent = nullptr);
    ~ConnectionController() override;

    Q_INVOKABLE void connectToServer(const QString& host, int port);
    Q_INVOKABLE void disconnectFromServer();

    Q_INVOKABLE void cmdMode(const QString& mode);
    Q_INVOKABLE void cmdTaskGoto(const QString& taskId, double x, double y);
    Q_INVOKABLE void cmdTaskSimple(const QString& type);  // pause/resume/cancel
    Q_INVOKABLE void cmdEstop(const QString& reason);
    Q_INVOKABLE void cmdReset();

signals:
    void startWorker(const QString& host, int port);
    void stopWorker();
    void workerCmdMode(const QString& mode);
    void workerCmdTask(const QString& taskId, const QString& type, double x,
                       double y, double timeoutS);
    void workerCmdEstop(const QString& reason);
    void workerCmdReset();

private slots:
    void onConnected();
    void onDisconnected();
    void onHelloAck(const QVariantMap& info);
    void onState(const QVariantMap& state);
    void onNodes(const QVariantList& nodes);
    void onRtt(int ms);
    void onCmdAck(const QVariantMap& ack);
    void onError(const QString& message);

private:
    RobotState* state_ = nullptr;
    NodeTableModel* nodes_ = nullptr;
    QThread worker_thread_;
    SocketWorker* worker_ = nullptr;
};
