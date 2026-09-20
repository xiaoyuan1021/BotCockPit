import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: pane

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Dashboard")
                font.pixelSize: 20
                font.bold: true
                Layout.fillWidth: true
            }

            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: robotState.connected ? "#1b7f3a" : "#8a1f1f"
            }
            Label {
                text: robotState.connected ? qsTr("LIVE") : qsTr("NO LINK")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Frame {
                Layout.fillWidth: true
                GridLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 8

                    Label { text: qsTr("conn"); opacity: 0.6 }
                    Label { text: robotState.conn; font.bold: true }
                    Label { text: qsTr("mode"); opacity: 0.6 }
                    Label { text: robotState.mode; font.bold: true }

                    Label { text: qsTr("phase"); opacity: 0.6 }
                    Label { text: robotState.phase; font.bold: true }
                    Label { text: qsTr("estop"); opacity: 0.6 }
                    Label {
                        text: robotState.estop ? qsTr("ACTIVE") : qsTr("false")
                        font.bold: true
                        color: robotState.estop ? "#8a1f1f" : palette.text
                    }

                    Label { text: qsTr("control"); opacity: 0.6 }
                    Label {
                        text: robotState.controlEnabled ? qsTr("enabled") : qsTr("disabled")
                        font.bold: true
                    }
                    Label { text: qsTr("battery"); opacity: 0.6 }
                    Label { text: robotState.battery.toFixed(1) + " %"; font.bold: true }
                }
            }

            Frame {
                Layout.fillWidth: true
                GridLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 8

                    Label { text: "pose.x"; opacity: 0.6 }
                    Label { text: robotState.poseX.toFixed(3) + " m" }
                    Label { text: "pose.y"; opacity: 0.6 }
                    Label { text: robotState.poseY.toFixed(3) + " m" }

                    Label { text: "pose.yaw"; opacity: 0.6 }
                    Label { text: robotState.poseYaw.toFixed(3) + " rad" }
                    Label { text: qsTr("RTT"); opacity: 0.6 }
                    Label { text: robotState.heartbeatRttMs + " ms" }

                    Label { text: qsTr("task"); opacity: 0.6 }
                    Label {
                        text: (robotState.taskType.length ? robotState.taskType : "—")
                              + " · " + robotState.taskStatus
                        Layout.columnSpan: 3
                    }
                }
            }
        }

        Label {
            text: qsTr("Nodes")
            font.bold: true
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: 0

            ListView {
                id: nodeView
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: nodeModel
                boundsBehavior: Flickable.StopAtBounds

                header: Rectangle {
                    width: nodeView.width
                    height: 28
                    color: "#e8eef5"
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 0
                        Label { width: parent.width * 0.4; text: qsTr("name"); font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                        Label { width: parent.width * 0.3; text: qsTr("status"); font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                        Label { width: parent.width * 0.3; text: qsTr("last_hb"); font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                    }
                }

                delegate: Rectangle {
                    width: nodeView.width
                    height: 32
                    color: index % 2 === 0 ? "#ffffff" : "#f6f8fb"
                    required property string name
                    required property string status
                    required property var lastHbMs
                    required property int index

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 0
                        Label {
                            width: parent.width * 0.4
                            text: name
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            width: parent.width * 0.3
                            text: status
                            anchors.verticalCenter: parent.verticalCenter
                            color: status === "OK" ? "#1b7f3a"
                                 : status === "WARN" ? "#9a6b00"
                                 : "#8a1f1f"
                            font.bold: true
                        }
                        Label {
                            width: parent.width * 0.3
                            text: lastHbMs
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: nodeView.count === 0
                    text: robotState.connected
                          ? qsTr("Waiting for state…")
                          : qsTr("Offline — connect to bridge")
                    opacity: 0.6
                }
            }
        }

        Label {
            text: qsTr("Faults: ") + (robotState.faultCount === 0
                                      ? qsTr("none")
                                      : (robotState.faultCount + " — " + robotState.faultsSummary))
            color: robotState.faultCount > 0 ? "#8a1f1f" : palette.text
        }
    }
}
