import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: controlPage

    // Local-only UI enablement; protocol safety is enforced in C++/robot.
    readonly property bool linkOk: robotState.connected
    readonly property bool canDrive: robotState.connected
                                     && robotState.controlEnabled
                                     && !robotState.estop
                                     && robotState.hasRobotState
    readonly property bool canEstop: robotState.connected

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Control")
                font.pixelSize: 20
                font.bold: true
                Layout.fillWidth: true
            }
            Label {
                text: robotState.phase + " · " + robotState.mode
                      + (robotState.estop ? " · ESTOP" : "")
                      + (robotState.controlEnabled ? "" : " · ctl off")
                opacity: 0.8
            }
        }

        GroupBox {
            title: qsTr("Mode")
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                spacing: 8
                Button {
                    text: "TELEOP"
                    enabled: controlPage.canDrive || (controlPage.linkOk && robotState.mode === "TELEOP")
                    onClicked: connection.cmdMode("TELEOP")
                }
                Button {
                    text: "AUTO"
                    enabled: controlPage.canDrive
                    onClicked: connection.cmdMode("AUTO")
                }
                Button {
                    text: "REMOTE"
                    enabled: controlPage.canDrive || (controlPage.linkOk && robotState.mode === "REMOTE")
                    onClicked: connection.cmdMode("REMOTE")
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: qsTr("current: ") + robotState.mode
                    opacity: 0.7
                }
            }
        }

        GroupBox {
            title: qsTr("Task")
            Layout.fillWidth: true
            GridLayout {
                columns: 6
                columnSpacing: 8
                rowSpacing: 8
                anchors.fill: parent

                Label { text: "task_id" }
                TextField {
                    id: taskIdField
                    text: "T-001"
                    Layout.preferredWidth: 100
                    enabled: controlPage.canDrive
                }
                Label { text: "x" }
                TextField {
                    id: xField
                    text: "2.0"
                    Layout.preferredWidth: 70
                    enabled: controlPage.canDrive
                }
                Label { text: "y" }
                TextField {
                    id: yField
                    text: "1.0"
                    Layout.preferredWidth: 70
                    enabled: controlPage.canDrive
                }

                Button {
                    text: qsTr("Send goto")
                    enabled: controlPage.canDrive
                    onClicked: {
                        connection.cmdTaskGoto(taskIdField.text.trim(),
                                                parseFloat(xField.text),
                                                parseFloat(yField.text))
                    }
                }
                Button {
                    text: qsTr("Pause")
                    enabled: controlPage.linkOk && robotState.phase === "RUNNING"
                    onClicked: connection.cmdTaskSimple("pause")
                }
                Button {
                    text: qsTr("Resume")
                    enabled: controlPage.linkOk && !robotState.estop
                             && robotState.taskType === "goto"
                    onClicked: connection.cmdTaskSimple("resume")
                }
                Button {
                    text: qsTr("Cancel")
                    enabled: controlPage.linkOk
                    onClicked: connection.cmdTaskSimple("cancel")
                }

                Label { text: qsTr("task"); opacity: 0.6 }
                Label {
                    text: (robotState.taskType.length ? robotState.taskType : "—")
                          + " · " + robotState.taskStatus
                          + (robotState.taskId.length ? (" · " + robotState.taskId) : "")
                    Layout.columnSpan: 3
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            Button {
                text: qsTr("SOFT E-STOP")
                Layout.preferredWidth: 160
                Layout.preferredHeight: 48
                enabled: controlPage.canEstop
                palette.button: "#c62828"
                palette.buttonText: "white"
                font.bold: true
                onClicked: estopConfirm.open()
            }

            Button {
                text: qsTr("Reset fault / ESTOP")
                Layout.preferredWidth: 180
                enabled: controlPage.linkOk && robotState.hasRobotState
                         && (robotState.estop || robotState.phase === "FAULT"
                             || robotState.phase === "DEGRADED"
                             || robotState.phase === "IDLE")
                onClicked: resetConfirm.open()
            }

            Item { Layout.fillWidth: true }
        }

        Frame {
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                Label { text: qsTr("Last command ACK"); font.bold: true }
                Label {
                    text: robotState.lastCmdAckText.length
                          ? robotState.lastCmdAckText
                          : qsTr("(none yet)")
                    color: robotState.lastCmdAckText.length === 0 ? palette.text
                         : (robotState.lastCmdOk ? "#1b7f3a" : "#8a1f1f")
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                Label {
                    visible: controlPage.linkOk && !robotState.hasRobotState
                    text: qsTr("Waiting for robot state — activate fake_robot + ensure bridge online")
                    color: "#9a6b00"
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
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
        width: Math.min(parent.width - 40, 420)
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
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
        width: Math.min(parent.width - 40, 420)
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Send CMD_RESET with confirm=true? This clears ESTOP when no ERROR fault source remains.")
        }
        onAccepted: connection.cmdReset()
    }
}
