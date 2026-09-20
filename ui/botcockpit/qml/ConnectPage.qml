import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: pane

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 420)
        spacing: 16

        Label {
            text: qsTr("Connect to bridge")
            font.pixelSize: 20
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            text: qsTr("TCP protocol v0.1 · default 127.0.0.1:8765")
            opacity: 0.7
            Layout.alignment: Qt.AlignHCenter
        }

        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 10
            Layout.fillWidth: true

            Label { text: qsTr("Host") }
            TextField {
                id: hostField
                Layout.fillWidth: true
                text: "127.0.0.1"
                enabled: !robotState.connected && !robotState.connecting
            }

            Label { text: qsTr("Port") }
            TextField {
                id: portField
                Layout.fillWidth: true
                text: "8765"
                validator: IntValidator { bottom: 1; top: 65535 }
                enabled: !robotState.connected && !robotState.connecting
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12

            Button {
                text: robotState.connecting ? qsTr("Connecting…") : qsTr("Connect")
                enabled: !robotState.connected && !robotState.connecting
                onClicked: {
                    const port = parseInt(portField.text)
                    connection.connectToServer(hostField.text.trim(), port)
                }
            }

            Button {
                text: qsTr("Disconnect")
                enabled: robotState.connected || robotState.connecting
                onClicked: connection.disconnectFromServer()
            }
        }

        Frame {
            Layout.fillWidth: true
            padding: 12

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                Label {
                    text: qsTr("Session")
                    font.bold: true
                }

                Label { text: qsTr("State: ") + (robotState.connected ? qsTr("Connected") : qsTr("Disconnected")) }
                Label { text: qsTr("Endpoint: ") + robotState.host + ":" + robotState.port }
                Label { text: qsTr("Heartbeat RTT: ") + robotState.heartbeatRttMs + " ms" }
                Label { text: qsTr("Protocol: ") + (robotState.proto.length ? robotState.proto : "—") }
                Label { text: qsTr("Server: ") + (robotState.server.length ? robotState.server : "—") }
                Label {
                    text: qsTr("Error: ") + (robotState.errorString.length ? robotState.errorString : "—")
                    color: robotState.errorString.length ? "#8a1f1f" : palette.text
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }
    }
}
