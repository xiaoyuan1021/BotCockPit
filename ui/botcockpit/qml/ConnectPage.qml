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
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 440)
        spacing: 14

        Item { Layout.preferredHeight: 12 }

        Text {
            text: qsTr("Connect to bridge")
            font.pixelSize: 24
            font.bold: true
            color: pane.win.colInk
            Layout.alignment: Qt.AlignHCenter
        }
        Text {
            text: qsTr("TCP protocol v0.1 · default 127.0.0.1:8765")
            color: pane.win.colMuted
            Layout.alignment: Qt.AlignHCenter
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: formCol.implicitHeight + 32
            radius: 12
            color: pane.win.colSurface
            border.color: pane.win.colBorder

            ColumnLayout {
                id: formCol
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                Text { text: qsTr("Endpoint"); color: pane.win.colMuted; font.pixelSize: 12 }

                GridLayout {
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 10
                    Layout.fillWidth: true

                    Text { text: qsTr("Host"); color: pane.win.colInk }
                    TextField {
                        id: hostField
                        Layout.fillWidth: true
                        text: "127.0.0.1"
                        enabled: !robotState.connected && !robotState.connecting
                        color: pane.win.colInk
                        placeholderTextColor: pane.win.colMuted
                        background: Rectangle {
                            implicitHeight: 36
                            radius: 8
                            color: pane.win.colSurfaceAlt
                            border.color: hostField.activeFocus ? pane.win.colAccent : pane.win.colBorder
                            border.width: hostField.activeFocus ? 2 : 1
                            Behavior on border.color { ColorAnimation { duration: 120 } }
                        }
                    }
                    Text { text: qsTr("Port"); color: pane.win.colInk }
                    TextField {
                        id: portField
                        Layout.fillWidth: true
                        text: "8765"
                        validator: IntValidator { bottom: 1; top: 65535 }
                        enabled: !robotState.connected && !robotState.connecting
                        color: pane.win.colInk
                        background: Rectangle {
                            implicitHeight: 36
                            radius: 8
                            color: pane.win.colSurfaceAlt
                            border.color: portField.activeFocus ? pane.win.colAccent : pane.win.colBorder
                            border.width: portField.activeFocus ? 2 : 1
                            Behavior on border.color { ColorAnimation { duration: 120 } }
                        }
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 12

                    ToolBtn {
                        text: robotState.connecting ? qsTr("Connecting…") : qsTr("Connect")
                        enabled: !robotState.connected && !robotState.connecting
                        accent: pane.win.colAccent
                        onClicked: connection.connectToServer(hostField.text.trim(),
                                                              parseInt(portField.text))
                    }
                    ToolBtn {
                        text: qsTr("Disconnect")
                        enabled: robotState.connected || robotState.connecting
                        accent: pane.win.colDanger
                        onClicked: connection.disconnectFromServer()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 12
            color: pane.win.colSurface
            border.color: robotState.connected ? Qt.darker(pane.win.colOk, 1.4) : pane.win.colBorder
            Behavior on border.color { ColorAnimation { duration: 200 } }
            implicitHeight: sessCol.implicitHeight + 28

            ColumnLayout {
                id: sessCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6

                RowLayout {
                    spacing: 8
                    LiveDot { active: robotState.connected }
                    Text {
                        text: qsTr("Session")
                        color: pane.win.colInk
                        font.bold: true
                    }
                }

                Text { text: qsTr("State: ") + (robotState.connected ? qsTr("Connected") : qsTr("Disconnected")); color: pane.win.colMuted }
                Text { text: qsTr("Endpoint: ") + robotState.host + ":" + robotState.port; color: pane.win.colMuted }
                Text {
                    text: qsTr("Heartbeat RTT: ") + robotState.heartbeatRttMs + " ms"
                    color: pane.win.colInk
                    font.bold: true
                }
                Text { text: qsTr("Protocol: ") + (robotState.proto.length ? robotState.proto : "—"); color: pane.win.colMuted }
                Text { text: qsTr("Server: ") + (robotState.server.length ? robotState.server : "—"); color: pane.win.colMuted }
                Text {
                    text: qsTr("Error: ") + (robotState.errorString.length ? robotState.errorString : "—")
                    color: robotState.errorString.length ? pane.win.colDanger : pane.win.colMuted
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
