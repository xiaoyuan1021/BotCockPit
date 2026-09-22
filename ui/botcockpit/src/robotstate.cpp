#include "robotstate.hpp"

#include <QDateTime>

#include <cmath>
#include <limits>
#include <utility>

RobotState::RobotState(QObject* parent) : QObject(parent) {}

void RobotState::setAutoReconnect(bool v)
{
    if (auto_reconnect_ == v) {
        return;
    }
    auto_reconnect_ = v;
    emit autoReconnectChanged();
}

void RobotState::setReconnectAttempts(int n)
{
    if (reconnect_attempts_ == n) {
        return;
    }
    reconnect_attempts_ = n;
    emit reconnectAttemptsChanged();
}

void RobotState::appendLog(const QString& line)
{
    const QString stamped =
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")) + QLatin1String("  ") + line;
    log_lines_.prepend(stamped);
    while (log_lines_.size() > 80) {
        log_lines_.removeLast();
    }
    emit logLinesChanged();
}

void RobotState::setLastCmdAck(const QVariantMap& ack)
{
    last_cmd_ok_ = ack.value(QStringLiteral("ok")).toBool();
    const QString cmd = ack.value(QStringLiteral("cmd")).toString();
    const QString status = ack.value(QStringLiteral("status")).toString();
    const QString reason = ack.value(QStringLiteral("reason")).toString();
    QString text = cmd;
    if (!text.isEmpty()) {
        text += QLatin1Char(' ');
    }
    text += last_cmd_ok_ ? QStringLiteral("OK") : QStringLiteral("REJECTED");
    if (!status.isEmpty()) {
        text += QStringLiteral(" | ") + status;
    }
    if (!reason.isEmpty() && reason != QLatin1String("null")) {
        text += QStringLiteral(" | ") + reason;
    }
    const QVariant seq = ack.value(QStringLiteral("seq"));
    if (seq.isValid() && !seq.isNull()) {
        text += QStringLiteral(" | seq=") + seq.toString();
    }
    last_cmd_ack_text_ = text;
    emit lastCmdAckChanged();
}

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
    has_robot_state_ = false;
    state_ts_ms_ = 0;
    task_id_.clear();
    task_type_.clear();
    task_status_ = QStringLiteral("NONE");
    faults_summary_.clear();
    fault_count_ = 0;
    nav_path_.clear();
    nav_trail_.clear();
    nav_path_flat_.clear();
    nav_trail_flat_.clear();
    nav_idx_ = 0;
    nav_path_len_ = 0;
    rtt_ms_ = 0;
    emit navStatusChanged();
    emit navTrailChanged();
    emit connChanged();
    emit modeChanged();
    emit phaseChanged();
    emit estopChanged();
    emit controlEnabledChanged();
    emit poseChanged();
    emit batteryChanged();
    emit hasRobotStateChanged();
    emit stateTsMsChanged();
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

    // 机器人侧真实状态：优先看 conn/控制使能，避免 bridge 离线模板误判
    // （离线模板也含 battery/nodes/pose，但 conn 通常为 OFFLINE 且 control_enabled=false）
    bool robot_state = map.contains(QStringLiteral("battery")) ||
                       map.contains(QStringLiteral("nodes")) ||
                       map.contains(QStringLiteral("pose"));
    if (map.contains(QStringLiteral("conn"))) {
        const QString c = map.value(QStringLiteral("conn")).toString();
        if (c == QLatin1String("OFFLINE") &&
            !map.value(QStringLiteral("control_enabled")).toBool()) {
            // 仅当后续出现 ONLINE/ESTOP/DEGRADED 等再算作有效机器人状态
            robot_state = false;
        }
    }
    if (map.contains(QStringLiteral("phase"))) {
        const QString p = map.value(QStringLiteral("phase")).toString();
        if (p == QLatin1String("BOOT") && robot_state &&
            !map.value(QStringLiteral("control_enabled")).toBool()) {
            robot_state = false;
        }
    }
    if (robot_state != has_robot_state_) {
        has_robot_state_ = robot_state;
        emit hasRobotStateChanged();
    }
    if (map.contains(QStringLiteral("ts_ms"))) {
        const qint64 ts = static_cast<qint64>(map.value(QStringLiteral("ts_ms")).toLongLong());
        if (ts != state_ts_ms_) {
            state_ts_ms_ = ts;
            emit stateTsMsChanged();
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
        QVariantList copy;
        copy.reserve(fl.size());
        for (const QVariant& item : fl) {
            copy.append(item);
            const QVariantMap fm = item.toMap();
            parts << fm.value(QStringLiteral("code")).toString();
        }
        faults_summary_ = parts.join(QStringLiteral(", "));
        if (copy != faults_list_) {
            faults_list_ = copy;
            emit faultsListChanged();
        }
        emit faultsChanged();
    }

    const QVariant nav = map.value(QStringLiteral("nav"));
    if (nav.canConvert<QVariantMap>()) {
        const QVariantMap nm = nav.toMap();
        const QString st = nm.value(QStringLiteral("status")).toString();
        const int plen = nm.value(QStringLiteral("path_len")).toInt();
        const QVariantMap goal = nm.value(QStringLiteral("goal")).toMap();
        const double gx = goal.value(QStringLiteral("x")).toDouble();
        const double gy = goal.value(QStringLiteral("y")).toDouble();
        const double te = nm.value(QStringLiteral("track_err")).toDouble();
        const int idx = nm.value(QStringLiteral("idx")).toInt();

        auto parseXYList = [](const QVariant& flatSeq, const QVariant& nestedSeq) {
            QVariantList pairs;
            QVariantList flat;
            // Preferred: flat [x0,y0,x1,y1,...] of plain numbers
            const QVariantList fl = flatSeq.toList();
            if (fl.size() >= 2) {
                for (int i = 0; i + 1 < fl.size(); i += 2) {
                    const double x = fl.at(i).toDouble();
                    const double y = fl.at(i + 1).toDouble();
                    if (!std::isfinite(x) || !std::isfinite(y)) {
                        continue;
                    }
                    pairs.append(QVariantList{x, y});
                    flat.append(x);
                    flat.append(y);
                }
                if (!pairs.isEmpty()) {
                    return std::make_pair(pairs, flat);
                }
            }
            // Nested [[x,y],...] or [{x,y},...]
            const QVariantList pl = nestedSeq.toList();
            for (const QVariant& item : pl) {
                double x = std::numeric_limits<double>::quiet_NaN();
                double y = std::numeric_limits<double>::quiet_NaN();
                const QVariantList xy = item.toList();
                if (xy.size() >= 2) {
                    x = xy.at(0).toDouble();
                    y = xy.at(1).toDouble();
                } else {
                    const QVariantMap pm = item.toMap();
                    if (pm.contains(QStringLiteral("x")) &&
                        pm.contains(QStringLiteral("y"))) {
                        x = pm.value(QStringLiteral("x")).toDouble();
                        y = pm.value(QStringLiteral("y")).toDouble();
                    }
                }
                if (!std::isfinite(x) || !std::isfinite(y)) {
                    continue;
                }
                pairs.append(QVariantList{x, y});
                flat.append(x);
                flat.append(y);
            }
            return std::make_pair(pairs, flat);
        };

        const auto pathXY = parseXYList(nm.value(QStringLiteral("path_flat")),
                                        nm.value(QStringLiteral("path")));
        const auto trailXY = parseXYList(nm.value(QStringLiteral("trail_flat")),
                                         nm.value(QStringLiteral("trail")));
        const QVariantList pts = pathXY.first;
        const QVariantList trail = trailXY.first;

        const bool changed = st != nav_status_ || plen != nav_path_len_ ||
                             idx != nav_idx_ ||
                             gx != nav_goal_x_ || gy != nav_goal_y_ ||
                             te != track_err_ || pts.size() != nav_path_.size() ||
                             pathXY.second != nav_path_flat_;
        const bool trailDirty = trail.size() != nav_trail_.size() ||
                                trailXY.second != nav_trail_flat_;
        nav_status_ = st.isEmpty() ? QStringLiteral("IDLE") : st;
        nav_path_len_ = plen;
        nav_idx_ = idx;
        nav_path_ = pts;
        nav_path_flat_ = pathXY.second;
        nav_goal_x_ = gx;
        nav_goal_y_ = gy;
        if (te != track_err_) {
            track_err_ = te;
            emit trackErrChanged();
        }
        if (trailDirty) {
            nav_trail_ = trail;
            nav_trail_flat_ = trailXY.second;
            emit navTrailChanged();
        }
        if (changed) {
            emit navStatusChanged();
        }
    }
}
