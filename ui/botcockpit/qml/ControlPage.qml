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
        if (!robotState.connected) return controlPage.win.colSurfaceAlt
        if (robotState.estop) return controlPage.win.colEstop
        if (robotState.phase === "RUNNING") return controlPage.win.colRunning
        if (robotState.phase === "FAULT" || robotState.phase === "DEGRADED") return controlPage.win.colWarn
        if (robotState.phase === "IDLE" && robotState.controlEnabled) return controlPage.win.colOk
        return controlPage.win.colSurfaceAlt
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 10

        Text {
            text: qsTr("Control")
            font.pixelSize: 22
            font.bold: true
            color: controlPage.win.colInk
        }

        // Animated phase banner
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            radius: 12
            color: Qt.darker(controlPage.phaseColor, 2.2)
            border.color: controlPage.phaseColor
            border.width: 2
            Behavior on color { ColorAnimation { duration: 220 } }
            Behavior on border.color { ColorAnimation { duration: 220 } }

            // pulse when RUNNING or ESTOP
            Rectangle {
                anchors.fill: parent
                radius: 12
                color: controlPage.phaseColor
                opacity: 0
                visible: robotState.phase === "RUNNING" || robotState.estop
                SequentialAnimation on opacity {
                    running: visible
                    loops: Animation.Infinite
                    NumberAnimation { from: 0.12; to: 0.0; duration: 900; easing.type: Easing.OutQuad }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 16
                Text {
                    text: {
                        if (!robotState.connected) return qsTr("OFFLINE")
                        return qsTr("PHASE: ") + robotState.phase
                               + (robotState.estop ? qsTr(" / ESTOP") : "")
                    }
                    color: "#ffffff"
                    font.pixelSize: 22
                    font.bold: true
                    Layout.fillWidth: true
                }
                Text {
                    text: {
                        var t = robotState.taskType.length ? robotState.taskType : "—"
                        var id = robotState.taskId.length ? robotState.taskId : ""
                        return qsTr("TASK: ") + t + " · " + robotState.taskStatus
                               + (id ? (" · " + id) : "")
                    }
                    color: "#ffffff"
                    opacity: 0.9
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
            color: controlPage.win.colMuted
            opacity: 1
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Mode
        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: controlPage.win.colSurface
            border.color: controlPage.win.colBorder
            implicitHeight: modeRow.implicitHeight + 24
            RowLayout {
                id: modeRow
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                Text { text: qsTr("Mode"); color: controlPage.win.colMuted; font.bold: true }
                ToolBtn {
                    text: "TELEOP"
                    enabled: controlPage.canDrive || (controlPage.linkOk && robotState.mode === "TELEOP")
                    accent: robotState.mode === "TELEOP" ? controlPage.win.colOk : controlPage.win.colAccent
                    onClicked: connection.cmdMode("TELEOP")
                }
                ToolBtn {
                    text: "AUTO"
                    enabled: controlPage.canDrive
                    accent: robotState.mode === "AUTO" ? controlPage.win.colOk : controlPage.win.colAccent
                    onClicked: connection.cmdMode("AUTO")
                }
                ToolBtn {
                    text: "REMOTE"
                    enabled: controlPage.canDrive || (controlPage.linkOk && robotState.mode === "REMOTE")
                    accent: robotState.mode === "REMOTE" ? controlPage.win.colOk : controlPage.win.colAccent
                    onClicked: connection.cmdMode("REMOTE")
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: qsTr("current: ") + robotState.mode
                    color: controlPage.win.colMuted
                }
            }
        }

        // Task
        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: controlPage.win.colSurface
            border.color: controlPage.win.colBorder
            implicitHeight: taskGrid.implicitHeight + 24
            GridLayout {
                id: taskGrid
                anchors.fill: parent
                anchors.margins: 12
                columns: 6
                columnSpacing: 8
                rowSpacing: 8

                Text { text: "task_id"; color: controlPage.win.colMuted }
                TextField {
                    id: taskIdField
                    text: "T-001"
                    Layout.preferredWidth: 100
                    enabled: controlPage.canDrive
                    color: controlPage.win.colInk
                    background: Rectangle {
                        implicitHeight: 32
                        radius: 6
                        color: controlPage.win.colSurfaceAlt
                        border.color: controlPage.win.colBorder
                    }
                }
                Text { text: "x"; color: controlPage.win.colMuted }
                TextField {
                    id: xField
                    text: "8.0"
                    Layout.preferredWidth: 70
                    enabled: controlPage.canDrive
                    color: controlPage.win.colInk
                    background: Rectangle {
                        implicitHeight: 32
                        radius: 6
                        color: controlPage.win.colSurfaceAlt
                        border.color: controlPage.win.colBorder
                    }
                }
                Text { text: "y"; color: controlPage.win.colMuted }
                TextField {
                    id: yField
                    text: "0.0"
                    Layout.preferredWidth: 70
                    enabled: controlPage.canDrive
                    color: controlPage.win.colInk
                    background: Rectangle {
                        implicitHeight: 32
                        radius: 6
                        color: controlPage.win.colSurfaceAlt
                        border.color: controlPage.win.colBorder
                    }
                }

                ToolBtn {
                    text: qsTr("Send goto")
                    enabled: controlPage.canDrive
                    accent: controlPage.win.colAccent
                    onClicked: connection.cmdTaskGoto(taskIdField.text.trim(),
                                                      parseFloat(xField.text),
                                                      parseFloat(yField.text))
                }
                ToolBtn {
                    text: qsTr("Pause")
                    enabled: controlPage.linkOk && robotState.phase === "RUNNING"
                    accent: controlPage.win.colWarn
                    onClicked: connection.cmdTaskSimple("pause")
                }
                ToolBtn {
                    text: qsTr("Resume")
                    enabled: controlPage.linkOk && !robotState.estop && robotState.taskType === "goto"
                    accent: controlPage.win.colOk
                    onClicked: connection.cmdTaskSimple("resume")
                }
                ToolBtn {
                    text: qsTr("Cancel")
                    enabled: controlPage.linkOk
                    accent: controlPage.win.colMuted
                    onClicked: connection.cmdTaskSimple("cancel")
                }
                Text { text: qsTr("task"); color: controlPage.win.colMuted }
                Text {
                    Layout.columnSpan: 3
                    text: (robotState.taskType.length ? robotState.taskType : "—")
                          + " · " + robotState.taskStatus
                          + (robotState.taskId.length ? (" · " + robotState.taskId) : "")
                    color: controlPage.win.colInk
                    font.bold: true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            // ESTOP — always linked, danger style + press flash
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
                    color: "#fff"
                    font: estopBtn.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalCenterAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 10
                    color: estopBtn.pressed ? "#b71c1c"
                         : estopBtn.hovered ? "#e53935" : "#c62828"
                    border.color: estopBtn.pressed ? "#ff8a80" : "#ff5c5c"
                    border.width: 2
                    Behavior on color { ColorAnimation { duration: 100 } }
                    scale: estopBtn.pressed ? 0.97 : 1.0
                    Behavior on scale { NumberAnimation { duration: 80 } }
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
                accent: controlPage.win.colWarn
                onClicked: resetConfirm.open()
            }

            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: controlPage.win.colSurface
            border.color: {
                if (!robotState.lastCmdAckText.length) return controlPage.win.colBorder
                return robotState.lastCmdOk ? controlPage.win.colOk : controlPage.win.colDanger
            }
            Behavior on border.color { ColorAnimation { duration: 200 } }
            implicitHeight: ackCol.implicitHeight + 20

            ColumnLayout {
                id: ackCol
                anchors.fill: parent
                anchors.margins: 12
                spacing: 4
                Text {
                    text: qsTr("Last command ACK")
                    color: controlPage.win.colMuted
                    font.bold: true
                }
                Text {
                    text: robotState.lastCmdAckText.length
                          ? robotState.lastCmdAckText
                          : qsTr("(none yet)")
                    color: robotState.lastCmdAckText.length === 0 ? controlPage.win.colMuted
                         : (robotState.lastCmdOk ? controlPage.win.colOk : controlPage.win.colDanger)
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.bold: true
                }
            }
        }

        Item { Layout.fillHeight: true }
    }

    Dialog {
        id: estopConfirm
        modal: true
        title: qsTr("Confirm soft E-STOP")
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay
        width: Math.min(controlPage.width - 40, 440)
        background: Rectangle {
            color: controlPage.win.colSurface
            border.color: controlPage.win.colEstop
            radius: 10
        }
        contentItem: Text {
            width: parent.width
            wrapMode: Text.WordWrap
            color: controlPage.win.colInk
            text: qsTr("Send CMD_ESTOP? Motion will stop immediately. Recovery requires Reset with no ERROR faults.")
        }
        onAccepted: connection.cmdEstop("operator")
    }

    Dialog {
        id: resetConfirm
        modal: true
        title: qsTr("Confirm RESET")
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay
        width: Math.min(controlPage.width - 40, 440)
        background: Rectangle {
            color: controlPage.win.colSurface
            border.color: controlPage.win.colWarn
            radius: 10
        }
        contentItem: Text {
            width: parent.width
            wrapMode: Text.WordWrap
            color: controlPage.win.colInk
            text: qsTr("Send CMD_RESET with confirm=true? This clears ESTOP when no ERROR fault source remains.")
        }
        onAccepted: connection.cmdReset()
    }
}
