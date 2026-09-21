import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: pane
    padding: 0
    background: Item {}
    readonly property var win: Window.window

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                text: qsTr("Dashboard")
                font.pixelSize: 22
                font.bold: true
                color: pane.win.colInk
                Layout.fillWidth: true
            }
            LiveDot { active: robotState.connected }
            Text {
                text: robotState.connected ? qsTr("LIVE") : qsTr("NO LINK")
                color: robotState.connected ? pane.win.colOk : pane.win.colDanger
                font.bold: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                radius: 10
                color: pane.win.colSurface
                border.color: pane.win.colBorder
                implicitHeight: keyGrid.implicitHeight + 24
                GridLayout {
                    id: keyGrid
                    anchors.fill: parent
                    anchors.margins: 12
                    columns: 4
                    columnSpacing: 18
                    rowSpacing: 8

                    Text { text: qsTr("conn"); color: pane.win.colMuted }
                    Text {
                        text: robotState.conn
                        color: robotState.conn === "ONLINE" ? pane.win.colOk
                             : robotState.estop ? pane.win.colEstop : pane.win.colDanger
                        font.bold: true
                    }
                    Text { text: qsTr("mode"); color: pane.win.colMuted }
                    Text { text: robotState.mode; color: pane.win.colInk; font.bold: true }

                    Text { text: qsTr("phase"); color: pane.win.colMuted }
                    Text {
                        text: robotState.phase
                        color: robotState.phase === "RUNNING" ? pane.win.colRunning
                             : robotState.phase === "ESTOP" ? pane.win.colEstop
                             : robotState.phase === "FAULT" ? pane.win.colDanger
                             : pane.win.colInk
                        font.bold: true
                        Behavior on color { ColorAnimation { duration: 200 } }
                    }
                    Text { text: qsTr("estop"); color: pane.win.colMuted }
                    Text {
                        text: robotState.estop ? qsTr("ACTIVE") : qsTr("false")
                        color: robotState.estop ? pane.win.colEstop : pane.win.colMuted
                        font.bold: true
                    }

                    Text { text: qsTr("control"); color: pane.win.colMuted }
                    Text {
                        text: robotState.controlEnabled ? qsTr("enabled") : qsTr("disabled")
                        color: robotState.controlEnabled ? pane.win.colOk : pane.win.colWarn
                        font.bold: true
                    }
                    Text { text: qsTr("battery"); color: pane.win.colMuted }
                    Text {
                        text: robotState.battery.toFixed(1) + " %"
                        color: robotState.battery < 20 ? pane.win.colWarn : pane.win.colInk
                        font.bold: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 10
                color: pane.win.colSurface
                border.color: pane.win.colBorder
                implicitHeight: keyGrid.implicitHeight + 24
                GridLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    columns: 4
                    columnSpacing: 18
                    rowSpacing: 8
                    Text { text: "pose.x"; color: pane.win.colMuted }
                    Text { text: robotState.poseX.toFixed(3) + " m"; color: pane.win.colInk }
                    Text { text: "pose.y"; color: pane.win.colMuted }
                    Text { text: robotState.poseY.toFixed(3) + " m"; color: pane.win.colInk }

                    Text { text: "pose.yaw"; color: pane.win.colMuted }
                    Text { text: robotState.poseYaw.toFixed(3) + " rad"; color: pane.win.colInk }
                    Text { text: qsTr("RTT"); color: pane.win.colMuted }
                    Text { text: robotState.heartbeatRttMs + " ms"; color: pane.win.colInk }

                    Text { text: qsTr("task"); color: pane.win.colMuted }
                    Text {
                        Layout.columnSpan: 3
                        text: (robotState.taskType.length ? robotState.taskType : "—")
                              + " · " + robotState.taskStatus
                        color: pane.win.colInk
                        font.bold: true
                    }
                }
            }
        }

        // battery bar
        Rectangle {
            Layout.fillWidth: true
            height: 6
            radius: 3
            color: pane.win.colSurfaceAlt
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, robotState.battery / 100.0))
                height: parent.height
                radius: 3
                color: robotState.battery < 20 ? pane.win.colWarn
                     : robotState.battery < 50 ? pane.win.colAccent
                     : pane.win.colOk
                Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutQuad } }
                Behavior on color { ColorAnimation { duration: 200 } }
            }
        }

        Text { text: qsTr("Nodes"); color: pane.win.colInk; font.bold: true }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 10
            color: pane.win.colSurface
            border.color: pane.win.colBorder
            clip: true

            ListView {
                id: nodeView
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                clip: true
                model: nodeModel
                boundsBehavior: Flickable.StopAtBounds

                header: Rectangle {
                    width: nodeView.width
                    height: 30
                    radius: 6
                    color: pane.win.colSurfaceAlt
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        Text { width: parent.width * 0.4; text: qsTr("name"); color: pane.win.colMuted; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                        Text { width: parent.width * 0.3; text: qsTr("status"); color: pane.win.colMuted; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                        Text { width: parent.width * 0.3; text: qsTr("last_hb"); color: pane.win.colMuted; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                    }
                }

                delegate: Rectangle {
                    width: nodeView.width
                    height: 34
                    radius: 6
                    color: index % 2 === 0 ? Qt.rgba(1,1,1,0.02) : "transparent"
                    required property string name
                    required property string status
                    required property var lastHbMs
                    required property int index
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        Text { width: parent.width * 0.4; text: name; color: pane.win.colInk; anchors.verticalCenter: parent.verticalCenter }
                        Text {
                            width: parent.width * 0.3
                            text: status
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                            color: status === "OK" ? pane.win.colOk
                                 : status === "WARN" ? pane.win.colWarn
                                 : pane.win.colDanger
                        }
                        Text {
                            width: parent.width * 0.3
                            text: lastHbMs
                            color: pane.win.colMuted
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: nodeView.count === 0
                    width: parent.width * 0.9
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: {
                        if (!robotState.connected)
                            return qsTr("Offline — connect to bridge")
                        if (!robotState.hasRobotState)
                            return qsTr("Bridge online — waiting for robot state.\nStart fake_robot, then lifecycle configure + activate")
                        return qsTr("Waiting for state…")
                    }
                    color: pane.win.colMuted
                }
            }
        }

        Text {
            text: robotState.connected && !robotState.hasRobotState
                  ? qsTr("Robot state: NOT RECEIVED — activate fake_robot")
                  : (qsTr("Faults: ") + (robotState.faultCount === 0
                                      ? qsTr("none")
                                      : (robotState.faultCount + " — " + robotState.faultsSummary)))
            color: (robotState.connected && !robotState.hasRobotState) || robotState.faultCount > 0
                   ? pane.win.colWarn : pane.win.colMuted
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
    }
}
