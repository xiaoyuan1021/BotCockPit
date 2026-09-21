import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: settingsPage
    padding: 0
    background: Item {}
    readonly property var win: Window.window

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 12

        Text {
            text: qsTr("Settings")
            font.pixelSize: 22
            font.bold: true
            color: "#1c2430"
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: "#ffffff"
            border.color: "#cfd8e6"
            implicitHeight: connCol.implicitHeight + 28

            ColumnLayout {
                id: connCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Text { text: qsTr("Connection"); font.bold: true; color: "#1c2430" }

                GridLayout {
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8
                    Layout.fillWidth: true
                    Text { text: qsTr("Host"); color: "#5d6b80" }
                    TextField {
                        id: hostField
                        Layout.preferredWidth: 220
                        text: robotState.host
                        color: "#1c2430"
                        background: Rectangle {
                            implicitHeight: 32
                            radius: 6
                            color: "#eef2f7"
                            border.color: "#cfd8e6"
                        }
                    }
                    Text { text: qsTr("Port"); color: "#5d6b80" }
                    TextField {
                        id: portField
                        Layout.preferredWidth: 100
                        text: String(robotState.port)
                        validator: IntValidator { bottom: 1; top: 65535 }
                        color: "#1c2430"
                        background: Rectangle {
                            implicitHeight: 32
                            radius: 6
                            color: "#eef2f7"
                            border.color: "#cfd8e6"
                        }
                    }
                }

                RowLayout {
                    spacing: 10
                    CheckBox {
                        id: autoBox
                        text: qsTr("Auto reconnect (backoff 1→10s)")
                        checked: robotState.autoReconnect
                        onToggled: robotState.autoReconnect = checked
                    }
                    ToolBtn {
                        text: robotState.connected ? qsTr("Reconnect now") : qsTr("Connect")
                        accent: "#1a6fd4"
                        onClicked: {
                            if (robotState.connected || robotState.connecting)
                                connection.disconnectFromServer()
                            // disconnect is manual; clear flag then connect
                            Qt.callLater(function() {
                                connection.connectToServer(hostField.text.trim(),
                                                           parseInt(portField.text))
                            })
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: qsTr("reconnect attempts: ") + robotState.reconnectAttempts
                        color: "#5d6b80"
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: "#ffffff"
            border.color: "#cfd8e6"
            implicitHeight: infoCol.implicitHeight + 28

            ColumnLayout {
                id: infoCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6
                Text { text: qsTr("Session / protocol"); font.bold: true; color: "#1c2430" }
                Text { text: qsTr("Protocol: ") + (robotState.proto.length ? robotState.proto : "—"); color: "#5d6b80" }
                Text { text: qsTr("Server: ") + (robotState.server.length ? robotState.server : "—"); color: "#5d6b80" }
                Text { text: qsTr("RTT: ") + robotState.heartbeatRttMs + " ms"; color: "#5d6b80" }
                Text { text: qsTr("Robot state: ") + (robotState.hasRobotState ? qsTr("received") : qsTr("not received")); color: "#5d6b80" }
                Text {
                    text: qsTr("STATE_DELTA: full snapshot @ ~5Hz (week1 protocol allowance)")
                    color: "#5d6b80"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
