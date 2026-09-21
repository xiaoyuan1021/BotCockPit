import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.impl

ApplicationWindow {
    id: root
    width: 1080
    height: 720
    visible: true
    title: qsTr("BotCockpit — ROS2 Robot Console")
    color: "#0f1218"

    // Industrial debug palette (Foxglove / HMI inspired)
    property color colBg: "#0f1218"
    property color colSurface: "#1a1f2a"
    property color colSurfaceAlt: "#222938"
    property color colBorder: "#2e3648"
    property color colInk: "#e8ecf4"
    property color colMuted: "#8b93a7"
    property color colAccent: "#3aa0ff"
    property color colOk: "#3ddc97"
    property color colWarn: "#f0b429"
    property color colDanger: "#ff5c5c"
    property color colRunning: "#4cc2ff"
    property color colEstop: "#ff3b3b"

    property int currentPage: 0

    font.family: "Segoe UI, Ubuntu, Noto Sans CJK SC, sans-serif"
    font.pixelSize: 13

    // Shared components live in LiveDot.qml / ToolBtn.qml (qrc)

    header: ToolBar {
        height: 52
        background: Rectangle {
            color: root.colSurface
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: root.colBorder
            }
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 14

            Row {
                spacing: 8
                anchors.verticalCenter: parent.verticalCenter
                Rectangle {
                    width: 22
                    height: 22
                    radius: 6
                    color: root.colAccent
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        anchors.centerIn: parent
                        text: "BC"
                        color: "#061018"
                        font.bold: true
                        font.pixelSize: 10
                    }
                }
                Text {
                    text: "BotCockpit"
                    color: root.colInk
                    font.bold: true
                    font.pixelSize: 15
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            TabBar {
                id: tabBar
                Layout.fillWidth: true
                background: Item {}
                TabButton {
                    text: qsTr("Connect")
                    width: implicitWidth + 20
                }
                TabButton {
                    text: qsTr("Dashboard")
                    width: implicitWidth + 20
                }
                TabButton {
                    text: qsTr("Control")
                    width: implicitWidth + 20
                }
                onCurrentIndexChanged: {
                    root.currentPage = currentIndex
                    fadeAnim.restart()
                }
            }

            Row {
                spacing: 8
                anchors.verticalCenter: parent.verticalCenter
                LiveDot {
                    active: robotState.connected
                    base: robotState.estop ? root.colEstop
                         : (robotState.phase === "RUNNING" ? root.colRunning : root.colOk)
                }
                Text {
                    text: robotState.connected
                          ? (qsTr("ONLINE") + " · " + robotState.heartbeatRttMs + " ms")
                          : (robotState.connecting ? qsTr("CONNECTING…") : qsTr("OFFLINE"))
                    color: robotState.connected ? root.colOk : root.colDanger
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Rectangle {
                radius: 8
                color: root.colSurfaceAlt
                border.color: {
                    if (robotState.estop) return root.colEstop
                    if (!robotState.connected) return root.colBorder
                    return robotState.controlEnabled ? root.colOk : root.colWarn
                }
                border.width: 1
                height: 32
                width: phaseChip.implicitWidth + 18
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    id: phaseChip
                    anchors.centerIn: parent
                    text: {
                        if (!robotState.connected) return qsTr("OFFLINE")
                        if (robotState.estop) return qsTr("ESTOP")
                        return robotState.phase + (robotState.controlEnabled ? "" : " · ctl off")
                    }
                    color: {
                        if (!robotState.connected) return root.colMuted
                        if (robotState.estop) return root.colEstop
                        if (robotState.phase === "RUNNING") return root.colRunning
                        return root.colInk
                    }
                    font.bold: true
                    font.pixelSize: 12
                }
                Behavior on border.color { ColorAnimation { duration: 180 } }
            }
        }
    }

    // Tab content with crossfade
    Item {
        id: contentHost
        anchors.fill: parent

        OpacityAnimator {
            id: fadeAnim
            target: contentHost
            from: 0.35
            to: 1.0
            duration: 160
            easing.type: Easing.OutQuad
        }

        StackLayout {
            anchors.fill: parent
            anchors.margins: 12
            currentIndex: root.currentPage

            ConnectPage {}
            DashboardPage {}
            ControlPage {}
        }
    }

    footer: ToolBar {
        height: 30
        background: Rectangle { color: root.colSurface }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 14
            text: robotState.connected
                  ? ("proto " + robotState.proto + " · " + robotState.server)
                  : (robotState.errorString !== "" ? robotState.errorString : qsTr("Ready"))
            color: root.colMuted
            font.pixelSize: 12
        }
    }
}
