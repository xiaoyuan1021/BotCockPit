import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1080
    height: 720
    visible: true
    title: qsTr("BotCockpit — ROS2 Robot Console")
    color: "#f3f5f9"

    // Light industrial lab palette
    property color colBg: "#f3f5f9"
    property color colSurface: "#ffffff"
    property color colSurfaceAlt: "#e8eef6"
    property color colBorder: "#cfd8e6"
    property color colInk: "#1c2430"
    property color colMuted: "#5d6b80"
    property color colAccent: "#1a6fd4"
    property color colOk: "#0f8a4a"
    property color colWarn: "#b07000"
    property color colDanger: "#c62828"
    property color colRunning: "#1565c0"
    property color colEstop: "#c62828"

    property int currentPage: 0

    font.family: "Segoe UI, Ubuntu, Noto Sans CJK SC, sans-serif"
    font.pixelSize: 13

    header: ToolBar {
        height: 54
        padding: 0
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
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12

            // Brand
            Row {
                spacing: 8
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: brand.implicitWidth + 30
                Rectangle {
                    width: 24
                    height: 24
                    radius: 6
                    color: root.colAccent
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        anchors.centerIn: parent
                        text: "BC"
                        color: "white"
                        font.bold: true
                        font.pixelSize: 10
                    }
                }
                Text {
                    id: brand
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
                Layout.alignment: Qt.AlignVCenter
                background: Item {}
                TabButton { text: qsTr("Connect"); width: implicitWidth + 24 }
                TabButton { text: qsTr("Dashboard"); width: implicitWidth + 24 }
                TabButton { text: qsTr("Control"); width: implicitWidth + 24 }
                onCurrentIndexChanged: {
                    root.currentPage = currentIndex
                    fadeAnim.restart()
                }
            }

            // Link status — keep on one line, not clipped
            Row {
                id: linkRow
                spacing: 8
                Layout.alignment: Qt.AlignVCenter
                Layout.minimumWidth: linkText.implicitWidth + 22
                LiveDot {
                    active: robotState.connected
                    base: robotState.estop ? root.colEstop
                         : (robotState.phase === "RUNNING" ? root.colRunning : root.colOk)
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    id: linkText
                    text: robotState.connected
                          ? (qsTr("ONLINE") + " · " + robotState.heartbeatRttMs + " ms")
                          : (robotState.connecting ? qsTr("CONNECTING…") : qsTr("OFFLINE"))
                    color: robotState.connected ? root.colOk : root.colDanger
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }

            // Phase chip — full text, no truncation
            Rectangle {
                id: phaseChip
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: phaseLabel.implicitWidth + 20
                Layout.maximumWidth: 220
                height: 34
                radius: 8
                color: root.colSurfaceAlt
                border.color: {
                    if (robotState.estop) return root.colEstop
                    if (!robotState.connected) return root.colBorder
                    return robotState.controlEnabled ? root.colOk : root.colWarn
                }
                border.width: 1
                Behavior on border.color { ColorAnimation { duration: 180 } }

                Text {
                    id: phaseLabel
                    anchors.centerIn: parent
                    width: parent.width - 16
                    horizontalAlignment: Text.AlignHCenter
                    text: {
                        if (!robotState.connected) return qsTr("OFFLINE")
                        if (robotState.estop) return qsTr("ESTOP")
                        // short form so chip always fits
                        var p = robotState.phase
                        return robotState.controlEnabled ? p : (p + " · ctl off")
                    }
                    color: {
                        if (!robotState.connected) return root.colMuted
                        if (robotState.estop) return root.colEstop
                        if (robotState.phase === "RUNNING") return root.colRunning
                        return root.colInk
                    }
                    font.bold: true
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }
        }
    }

    Item {
        id: contentHost
        anchors.fill: parent

        OpacityAnimator {
            id: fadeAnim
            target: contentHost
            from: 0.4
            to: 1.0
            duration: 150
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
        padding: 0
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
