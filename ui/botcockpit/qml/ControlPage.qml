import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: controlPage
    padding: 0
    background: Item {}
    readonly property var win: Window.window

    readonly property bool linkOk: robotState.connected
    readonly property bool canDrive: robotState.connected
                                     && robotState.controlEnabled
                                     && !robotState.estop
                                     && robotState.hasRobotState
    readonly property bool canEstop: robotState.connected

    property color phaseColor: {
        if (!robotState.connected) return "#5d6b80"
        if (robotState.estop) return "#c62828"
        if (robotState.phase === "RUNNING") return "#1565c0"
        if (robotState.phase === "FAULT" || robotState.phase === "DEGRADED") return "#b07000"
        if (robotState.phase === "IDLE" && robotState.controlEnabled) return "#0f8a4a"
        return "#1a6fd4"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 8

        Text {
            text: qsTr("Control")
            font.pixelSize: 22
            font.bold: true
            color: "#1c2430"
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            radius: 12
            color: controlPage.phaseColor
            Behavior on color { ColorAnimation { duration: 220 } }

            Rectangle {
                anchors.fill: parent
                radius: 12
                color: "#ffffff"
                opacity: 0
                visible: robotState.phase === "RUNNING" || robotState.estop
                SequentialAnimation on opacity {
                    running: visible
                    loops: Animation.Infinite
                    NumberAnimation { from: 0.18; to: 0.0; duration: 900; easing.type: Easing.OutQuad }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12
                Text {
                    Layout.fillWidth: true
                    text: {
                        if (!robotState.connected) return qsTr("OFFLINE")
                        return qsTr("PHASE: ") + robotState.phase
                               + (robotState.estop ? qsTr(" / ESTOP") : "")
                    }
                    color: "#ffffff"
                    font.pixelSize: 18
                    font.bold: true
                }
                Text {
                    text: {
                        var t = robotState.taskType.length ? robotState.taskType : "—"
                        return qsTr("TASK: ") + t + " · " + robotState.taskStatus
                    }
                    color: "#ffffff"
                    elide: Text.ElideRight
                    Layout.maximumWidth: 220
                }
                Text {
                    text: robotState.controlEnabled ? qsTr("ctl ON") : qsTr("ctl OFF")
                    color: "#ffffff"
                    font.bold: true
                }
                Text {
                    visible: robotState.navStatus !== "IDLE"
                    text: qsTr("NAV: ") + robotState.navStatus
                          + (robotState.navPathLen ? (" · " + robotState.navPathLen + " wp") : "")
                    color: "#ffffff"
                    font.bold: true
                    font.pixelSize: 13
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            NavMapView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 200
            }

            ColumnLayout {
                Layout.preferredWidth: 360
                Layout.fillHeight: true
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    radius: 10
                    color: "#ffffff"
                    border.color: "#cfd8e6"
                    implicitHeight: modeRow.implicitHeight + 20
                    RowLayout {
                        id: modeRow
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8
                        Text { text: qsTr("Mode"); color: "#5d6b80"; font.bold: true }
                        ToolBtn {
                            text: "TELEOP"
                            enabled: controlPage.canDrive || (controlPage.linkOk && robotState.mode === "TELEOP")
                            accent: robotState.mode === "TELEOP" ? "#0f8a4a" : "#1a6fd4"
                            onClicked: connection.cmdMode("TELEOP")
                        }
                        ToolBtn {
                            text: "AUTO"
                            enabled: controlPage.canDrive
                            accent: robotState.mode === "AUTO" ? "#0f8a4a" : "#1a6fd4"
                            onClicked: connection.cmdMode("AUTO")
                        }
                        ToolBtn {
                            text: "REMOTE"
                            enabled: controlPage.canDrive || (controlPage.linkOk && robotState.mode === "REMOTE")
                            accent: robotState.mode === "REMOTE" ? "#0f8a4a" : "#1a6fd4"
                            onClicked: connection.cmdMode("REMOTE")
                        }
                        Item { Layout.fillWidth: true }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 10
                    color: "#ffffff"
                    border.color: "#cfd8e6"
                    implicitHeight: taskGrid.implicitHeight + 20
                    GridLayout {
                        id: taskGrid
                        anchors.fill: parent
                        anchors.margins: 10
                        columns: 4
                        columnSpacing: 8
                        rowSpacing: 6

                        Text { text: "task_id"; color: "#5d6b80" }
                        TextField {
                            id: taskIdField
                            text: "T-001"
                            Layout.columnSpan: 3
                            enabled: controlPage.canDrive
                            color: "#1c2430"
                            background: Rectangle {
                                implicitHeight: 30
                                radius: 6
                                color: "#eef2f7"
                                border.color: "#cfd8e6"
                            }
                        }
                        Text { text: "x"; color: "#5d6b80" }
                        TextField {
                            id: xField
                            text: "18.0"
                            enabled: controlPage.canDrive
                            color: "#1c2430"
                            background: Rectangle {
                                implicitHeight: 30
                                radius: 6
                                color: "#eef2f7"
                                border.color: "#cfd8e6"
                            }
                        }
                        Text { text: "y"; color: "#5d6b80" }
                        TextField {
                            id: yField
                            text: "0.0"
                            enabled: controlPage.canDrive
                            color: "#1c2430"
                            background: Rectangle {
                                implicitHeight: 30
                                radius: 6
                                color: "#eef2f7"
                                border.color: "#cfd8e6"
                            }
                        }
                        ToolBtn {
                            text: qsTr("Send goto")
                            Layout.columnSpan: 2
                            enabled: controlPage.canDrive
                            onClicked: connection.cmdTaskGoto(taskIdField.text.trim(),
                                                              parseFloat(xField.text),
                                                              parseFloat(yField.text))
                        }
                        ToolBtn {
                            text: qsTr("Pause")
                            enabled: controlPage.linkOk && robotState.phase === "RUNNING"
                            accent: "#b07000"
                            onClicked: connection.cmdTaskSimple("pause")
                        }
                        ToolBtn {
                            text: qsTr("Resume")
                            enabled: controlPage.linkOk && !robotState.estop && robotState.taskType === "goto"
                            accent: "#0f8a4a"
                            onClicked: connection.cmdTaskSimple("resume")
                        }
                        ToolBtn {
                            text: qsTr("Cancel")
                            Layout.columnSpan: 2
                            enabled: controlPage.linkOk
                            onClicked: connection.cmdTaskSimple("cancel")
                        }
                        Text { text: "err"; color: "#5d6b80" }
                        Text {
                            Layout.columnSpan: 3
                            text: robotState.trackErr.toFixed(3) + " m"
                            font.bold: true
                            color: robotState.trackErr > 1.0 ? "#c62828" : "#1c2430"
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Button {
                        id: estopBtn
                        text: qsTr("SOFT E-STOP")
                        enabled: controlPage.canEstop
                        Layout.preferredWidth: 160
                        Layout.preferredHeight: 48
                        font.bold: true
                        contentItem: Text {
                            text: estopBtn.text
                            color: "#ffffff"
                            font: estopBtn.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 10
                            color: estopBtn.pressed ? "#b71c1c"
                                 : estopBtn.hovered ? "#e53935" : "#c62828"
                        }
                        onClicked: estopConfirm.open()
                    }
                    ToolBtn {
                        text: qsTr("Reset")
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        enabled: controlPage.linkOk && robotState.hasRobotState
                                 && (robotState.estop || robotState.phase === "FAULT"
                                     || robotState.phase === "DEGRADED"
                                     || robotState.phase === "IDLE")
                        accent: "#b07000"
                        onClicked: resetConfirm.open()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 10
                    color: "#ffffff"
                    border.color: robotState.lastCmdAckText.length === 0 ? "#cfd8e6"
                                 : (robotState.lastCmdOk ? "#0f8a4a" : "#c62828")
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        Text {
                            text: qsTr("Last ACK")
                            color: "#5d6b80"
                            font.bold: true
                        }
                        Text {
                            text: robotState.lastCmdAckText.length
                                  ? robotState.lastCmdAckText
                                  : qsTr("(none yet)")
                            color: robotState.lastCmdOk ? "#0f8a4a" : "#1c2430"
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.bold: true
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }

    Dialog {
        id: estopConfirm
        modal: true
        title: qsTr("Confirm soft E-STOP")
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay
        width: 440
        contentItem: Item {
            implicitWidth: 400
            implicitHeight: estopMsg.implicitHeight
            Text {
                id: estopMsg
                width: 400
                wrapMode: Text.WordWrap
                color: "#1c2430"
                text: qsTr("Send CMD_ESTOP? Motion stops immediately. Reset required to recover.")
            }
        }
        onAccepted: connection.cmdEstop("operator")
    }

    Dialog {
        id: resetConfirm
        modal: true
        title: qsTr("Confirm RESET")
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay
        width: 440
        contentItem: Item {
            implicitWidth: 400
            implicitHeight: resetMsg.implicitHeight
            Text {
                id: resetMsg
                width: 400
                wrapMode: Text.WordWrap
                color: "#1c2430"
                text: qsTr("Send CMD_RESET confirm=true? Clears ESTOP when no ERROR fault remains.")
            }
        }
        onAccepted: connection.cmdReset()
    }
}
