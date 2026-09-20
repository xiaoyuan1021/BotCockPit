#include "robotstate.hpp"

RobotState::RobotState(QObject* parent) : QObject(parent) {}

void RobotState::setConnecting(bool v)
{
    if (connecting_ == v) {
        return;
    }
    connecting_ = v;
    emit connectingChanged();
}

void RobotState::setConnected(bool v)
{
    if (connected_ == v) {
        return;
    }
    connected_ = v;
    emit connectedChanged();
}

void RobotState::setErrorString(const QString& e)
{
    if (error_string_ == e) {
        return;
    }
    error_string_ = e;
    emit errorStringChanged();
}

void RobotState::setEndpoint(const QString& host, int port)
{
    if (host_ != host) {
        host_ = host;
        emit hostChanged();
    }
    if (port_ != port) {
        port_ = port;
        emit portChanged();
    }
}

void RobotState::setHeartbeatRtt(int ms)
{
    if (rtt_ms_ == ms) {
        return;
    }
    rtt_ms_ = ms;
    emit heartbeatRttMsChanged();
}

void RobotState::setHelloInfo(const QString& proto, const QString& server)
{
    if (proto_ != proto) {
        proto_ = proto;
        emit protoChanged();
    }
    if (server_ != server) {
        server_ = server;
        emit serverChanged();
    }
}

void RobotState::resetRemote()
{
    conn_ = QStringLiteral("OFFLINE");
    mode_ = QStringLiteral("TELEOP");
    phase_ = QStringLiteral("BOOT");
    estop_ = false;
    control_enabled_ = false;
    pose_x_ = 0.0;
    pose_y_ = 0.0;
    pose_yaw_ = 0.0;
    battery_ = 0.0;
    task_id_.clear();
    task_type_.clear();
    task_status_ = QStringLiteral("NONE");
    faults_summary_.clear();
    fault_count_ = 0;
    rtt_ms_ = 0;
    emit connChanged();
    emit modeChanged();
    emit phaseChanged();
    emit estopChanged();
    emit controlEnabledChanged();
    emit poseChanged();
    emit batteryChanged();
    emit taskChanged();
    emit faultsChanged();
    emit heartbeatRttMsChanged();
}

void RobotState::applyState(const QVariantMap& map)
{
    if (map.contains(QStringLiteral("conn"))) {
        const QString v = map.value(QStringLiteral("conn")).toString();
        if (conn_ != v) {
            conn_ = v;
            emit connChanged();
        }
    }
    if (map.contains(QStringLiteral("mode"))) {
        const QString v = map.value(QStringLiteral("mode")).toString();
        if (mode_ != v) {
            mode_ = v;
            emit modeChanged();
        }
    }
    if (map.contains(QStringLiteral("phase"))) {
        const QString v = map.value(QStringLiteral("phase")).toString();
        if (phase_ != v) {
            phase_ = v;
            emit phaseChanged();
        }
    }
    if (map.contains(QStringLiteral("estop"))) {
        const bool v = map.value(QStringLiteral("estop")).toBool();
        if (estop_ != v) {
            estop_ = v;
            emit estopChanged();
        }
    }
    if (map.contains(QStringLiteral("control_enabled"))) {
        const bool v = map.value(QStringLiteral("control_enabled")).toBool();
        if (control_enabled_ != v) {
            control_enabled_ = v;
            emit controlEnabledChanged();
        }
    }

    const QVariant pose = map.value(QStringLiteral("pose"));
    if (pose.canConvert<QVariantMap>()) {
        const QVariantMap pm = pose.toMap();
        bool changed = false;
        const double x = pm.value(QStringLiteral("x"), pose_x_).toDouble();
        const double y = pm.value(QStringLiteral("y"), pose_y_).toDouble();
        const double yaw = pm.value(QStringLiteral("yaw"), pose_yaw_).toDouble();
        if (x != pose_x_ || y != pose_y_ || yaw != pose_yaw_) {
            pose_x_ = x;
            pose_y_ = y;
            pose_yaw_ = yaw;
            changed = true;
        }
        if (changed) {
            emit poseChanged();
        }
    }

    if (map.contains(QStringLiteral("battery"))) {
        const double v = map.value(QStringLiteral("battery")).toDouble();
        if (!qFuzzyCompare(battery_ + 1.0, v + 1.0)) {
            battery_ = v;
            emit batteryChanged();
        }
    }

    const QVariant task = map.value(QStringLiteral("task"));
    if (task.canConvert<QVariantMap>()) {
        const QVariantMap tm = task.toMap();
        const QString id = tm.contains(QStringLiteral("id"))
                               ? tm.value(QStringLiteral("id")).toString()
                               : task_id_;
        const QString type = tm.contains(QStringLiteral("type"))
                                 ? tm.value(QStringLiteral("type")).toString()
                                 : task_type_;
        const QString status = tm.value(QStringLiteral("status"), task_status_)
                                   .toString();
        if (id != task_id_ || type != task_type_ || status != task_status_) {
            task_id_ = id;
            task_type_ = type;
            task_status_ = status;
            emit taskChanged();
        }
    }

    const QVariant faults = map.value(QStringLiteral("faults"));
    if (faults.canConvert<QVariantList>()) {
        const QVariantList fl = faults.toList();
        fault_count_ = fl.size();
        QStringList parts;
        for (const QVariant& item : fl) {
            const QVariantMap fm = item.toMap();
            parts << fm.value(QStringLiteral("code")).toString();
        }
        faults_summary_ = parts.join(QStringLiteral(", "));
        emit faultsChanged();
    }
}
