#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

// QML-facing robot snapshot. No protocol decode here — ConnectionController
// pushes already-parsed fields via applyState().
class RobotState : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectingChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QString host READ host NOTIFY hostChanged)
    Q_PROPERTY(int port READ port NOTIFY portChanged)
    Q_PROPERTY(int heartbeatRttMs READ heartbeatRttMs NOTIFY heartbeatRttMsChanged)
    Q_PROPERTY(QString proto READ proto NOTIFY protoChanged)
    Q_PROPERTY(QString server READ server NOTIFY serverChanged)
    Q_PROPERTY(QString conn READ conn NOTIFY connChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(bool estop READ estop NOTIFY estopChanged)
    Q_PROPERTY(bool controlEnabled READ controlEnabled NOTIFY controlEnabledChanged)
    Q_PROPERTY(double poseX READ poseX NOTIFY poseChanged)
    Q_PROPERTY(double poseY READ poseY NOTIFY poseChanged)
    Q_PROPERTY(double poseYaw READ poseYaw NOTIFY poseChanged)
    Q_PROPERTY(double battery READ battery NOTIFY batteryChanged)
    Q_PROPERTY(bool hasRobotState READ hasRobotState NOTIFY hasRobotStateChanged)
    Q_PROPERTY(qint64 stateTsMs READ stateTsMs NOTIFY stateTsMsChanged)
    Q_PROPERTY(QString taskId READ taskId NOTIFY taskChanged)
    Q_PROPERTY(QString taskType READ taskType NOTIFY taskChanged)
    Q_PROPERTY(QString taskStatus READ taskStatus NOTIFY taskChanged)
    Q_PROPERTY(QString faultsSummary READ faultsSummary NOTIFY faultsChanged)
    Q_PROPERTY(int faultCount READ faultCount NOTIFY faultsChanged)

public:
    explicit RobotState(QObject* parent = nullptr);

    bool connected() const { return connected_; }
    bool connecting() const { return connecting_; }
    QString errorString() const { return error_string_; }
    QString host() const { return host_; }
    int port() const { return port_; }
    int heartbeatRttMs() const { return rtt_ms_; }
    QString proto() const { return proto_; }
    QString server() const { return server_; }
    QString conn() const { return conn_; }
    QString mode() const { return mode_; }
    QString phase() const { return phase_; }
    bool estop() const { return estop_; }
    bool controlEnabled() const { return control_enabled_; }
    double poseX() const { return pose_x_; }
    double poseY() const { return pose_y_; }
    double poseYaw() const { return pose_yaw_; }
    double battery() const { return battery_; }
    bool hasRobotState() const { return has_robot_state_; }
    qint64 stateTsMs() const { return state_ts_ms_; }
    QString taskId() const { return task_id_; }
    QString taskType() const { return task_type_; }
    QString taskStatus() const { return task_status_; }
    QString faultsSummary() const { return faults_summary_; }
    int faultCount() const { return fault_count_; }

    void setConnecting(bool v);
    void setConnected(bool v);
    void setErrorString(const QString& e);
    void setEndpoint(const QString& host, int port);
    void setHeartbeatRtt(int ms);
    void setHelloInfo(const QString& proto, const QString& server);
    void applyState(const QVariantMap& map);
    void resetRemote();

signals:
    void connectedChanged();
    void connectingChanged();
    void errorStringChanged();
    void hostChanged();
    void portChanged();
    void heartbeatRttMsChanged();
    void protoChanged();
    void serverChanged();
    void connChanged();
    void modeChanged();
    void phaseChanged();
    void estopChanged();
    void controlEnabledChanged();
    void poseChanged();
    void batteryChanged();
    void hasRobotStateChanged();
    void stateTsMsChanged();
    void taskChanged();
    void faultsChanged();

private:
    bool connected_ = false;
    bool connecting_ = false;
    QString error_string_;
    QString host_ = QStringLiteral("127.0.0.1");
    int port_ = 8765;
    int rtt_ms_ = 0;
    QString proto_;
    QString server_;
    QString conn_ = QStringLiteral("OFFLINE");
    QString mode_ = QStringLiteral("TELEOP");
    QString phase_ = QStringLiteral("BOOT");
    bool estop_ = false;
    bool control_enabled_ = false;
    double pose_x_ = 0.0;
    double pose_y_ = 0.0;
    double pose_yaw_ = 0.0;
    double battery_ = 0.0;
    bool has_robot_state_ = false;
    qint64 state_ts_ms_ = 0;
    QString task_id_;
    QString task_type_;
    QString task_status_ = QStringLiteral("NONE");
    QString faults_summary_;
    int fault_count_ = 0;
};
