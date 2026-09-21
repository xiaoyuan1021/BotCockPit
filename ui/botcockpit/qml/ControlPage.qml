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
        spacing: 10

        Text {
            text: qsTr("Control")
            font.pixelSize: 22
            font.bold: true
            color: controlPage.win ? controlPage.win.colInk : "#1c2430"
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
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
                anchors.margins: 12
                spacing: 16
                Text {
                    Layout.fillWidth: true
                    text: {
                        if (!robotState.connected) return qsTr("OFFLINE")
                        return qsTr("PHASE: ") + robotState.phase
                               + (robotState.estop ? qsTr(" / ESTOP") : "")
                    }
                    color: "#ffffff"
                    font.pixelSize: 20
                    font.bold: true
                }
                Text {
                    text: {
                        var t = robotState.taskType.length ? robotState.taskType : "—"
                        var id = robotState.taskId.length ? robotState.taskId : ""
                        return qsTr("TASK: ") + t + " · " + robotState.taskStatus
                               + (id ? (" · " + id) : "")
                    }
                    color: "#ffffff"
                    elide: Text.ElideRight
                    Layout.maximumWidth: 300
                }
                Text {
                    text: robotState.controlEnabled ? qsTr("ctl ON") : qsTr("ctl OFF")
                    color: "#ffffff"
                    font.bold: true
                }
            }
        }

        Text {
            visible: robotState.connected && robotState.phase === "RUNNING"
            text: qsTr("Task running — pose is moving; phase returns to IDLE when goal is reached (DONE).")
            color: controlPage.win ? controlPage.win.colMuted : "#5d6b80"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: controlPage.win ? controlPage.win.colSurface : "#ffffff"
            border.color: controlPage.win ? controlPage.win.colBorder : "#cfd8e6"
            implicitHeight: modeRow.implicitHeight + 24
            RowLayout {
                id: modeRow
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                Text {
                    text: qsTr("Mode")
                    color: controlPage.win ? controlPage.win.colMuted : "#5d6b80"
                    font.bold: true
                }
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
                Text {
                    text: qsTr("current: ") + robotState.mode
                    color: controlPage.win ? controlPage.win.colMuted : "#5d6b80"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: controlPage.win ? controlPage.win.colSurface : "#ffffff"
            border.color: controlPage.win ? controlPage.win.colBorder : "#cfd8e6"
            implicitHeight: taskGrid.implicitHeight + 24
            GridLayout {
                id: taskGrid
                anchors.fill: parent
                anchors.margins: 12
                columns: 6
                columnSpacing: 8
                rowSpacing: 8

                Text { text: "task_id"; color: "#5d6b80" }
                TextField {
                    id: taskIdField
                    text: "T-001"
                    Layout.preferredWidth: 100
                    enabled: controlPage.canDrive
                    color: "#1c2430"
                    background: Rectangle {
                        implicitHeight: 32
                        radius: 6
                        color: "#eef2f7"
                        border.color: "#cfd8e6"
                    }
                }
                Text { text: "x"; color: "#5d6b80" }
                TextField {
                    id: xField
                    text: "8.0"
                    Layout.preferredWidth: 70
                    enabled: controlPage.canDrive
                    color: "#1c2430"
                    background: Rectangle {
                        implicitHeight: 32
                        radius: 6
                        color: "#eef2f7"
                        border.color: "#cfd8e6"
                    }
                }
                Text { text: "y"; color: "#5d6b80" }
                TextField {
                    id: yField
                    text: "0.0"
                    Layout.preferredWidth: 70
                    enabled: controlPage.canDrive
                    color: "#1c2430"
                    background: Rectangle {
                        implicitHeight: 32
                        radius: 6
                        color: "#eef2f7"
                        border.color: "#cfd8e6"
                    }
                }

                ToolBtn {
                    text: qsTr("Send goto")
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
                    enabled: controlPage.linkOk
                    accent: "#5d6b80"
                    onClicked: connection.cmdTaskSimple("cancel")
                }
                Text { text: qsTr("task"); color: "#5d6b80" }
                Text {
                    Layout.columnSpan: 3
                    text: (robotState.taskType.length ? robotState.taskType : "—")
                          + " · " + robotState.taskStatus
                          + (robotState.taskId.length ? (" · " + robotState.taskId) : "")
                    color: "#1c2430"
                    font.bold: true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Button {
                id: estopBtn
                text: qsTr("SOFT E-STOP")
                enabled: controlPage.canEstop
                Layout.preferredWidth: 180
                Layout.preferredHeight: 52
                font.pixelSize: 16
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
                    border.color: "#8e0000"
                    border.width: 1
                }
                onClicked: estopConfirm.open()
            }

            ToolBtn {
                text: qsTr("Reset fault / ESTOP")
                Layout.preferredWidth: 190
                Layout.preferredHeight: 52
                enabled: controlPage.linkOk && robotState.hasRobotState
                         && (robotState.estop || robotState.phase === "FAULT"
                             || robotState.phase === "DEGRADED"
                             || robotState.phase === "IDLE")
                accent: "#b07000"
                onClicked: resetConfirm.open()
            }

            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: controlPage.win ? controlPage.win.colSurface : "#ffffff"
            border.color: {
                if (!robotState.lastCmdAckText.length)
                    return controlPage.win ? controlPage.win.colBorder : "#cfd8e6"
                return robotState.lastCmdOk ? "#0f8a4a" : "#c62828"
            }
            implicitHeight: ackCol.implicitHeight + 20

            ColumnLayout {
                id: ackCol
                anchors.fill: parent
                anchors.margins: 12
                spacing: 4
                Text {
                    text: qsTr("Last command ACK")
                    color: "#5d6b80"
                    font.bold: true
                }
                Text {
                    text: robotState.lastCmdAckText.length
                          ? robotState.lastCmdAckText
                          : qsTr("(none yet)")
                    color: robotState.lastCmdAckText.length === 0 ? "#5d6b80"
                         : (robotState.lastCmdOk ? "#0f8a4a" : "#c62828")
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.bold: true
                }
            }
        }

        Item { Layout.fillHeight: true }
    }

    // Fixed-size content item avoids Dialog implicitHeight binding loops
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
                text: qsTr("Send CMD_ESTOP? Motion will stop immediately. Recovery requires Reset with no ERROR faults.")
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
                text: qsTr("Send CMD_RESET with confirm=true? This clears ESTOP when no ERROR fault source remains.")
            }
        }
        onAccepted: connection.cmdReset()
    }
}
