import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1000
    height: 680
    visible: true
    title: qsTr("BotCockpit — ROS2 Robot Console")

    property int currentPage: 0

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12

            Label {
                text: "BotCockpit"
                font.bold: true
                font.pixelSize: 16
            }

            TabBar {
                id: tabBar
                Layout.fillWidth: true
                background: Item {}
                TabButton { text: qsTr("Connect") }
                TabButton { text: qsTr("Dashboard") }
                TabButton { text: qsTr("Control") }
                onCurrentIndexChanged: root.currentPage = currentIndex
            }

            Label {
                text: robotState.connected
                      ? (qsTr("ONLINE") + " · RTT " + robotState.heartbeatRttMs + " ms")
                      : (robotState.connecting ? qsTr("CONNECTING…") : qsTr("OFFLINE"))
                color: robotState.connected ? "#1b7f3a" : "#8a1f1f"
            }

            Label {
                text: robotState.estop ? qsTr("ESTOP")
                     : (robotState.phase + " · " + (robotState.controlEnabled
                        ? qsTr("ctl on") : qsTr("ctl off")))
                font.bold: true
                color: robotState.estop ? "#8a1f1f"
                     : (robotState.controlEnabled ? "#1b7f3a" : palette.text)
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.currentPage

        ConnectPage {}
        DashboardPage {}
        ControlPage {}
    }

    footer: ToolBar {
        Label {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 12
            text: robotState.connected
                  ? ("proto " + robotState.proto + " · " + robotState.server)
                  : (robotState.errorString !== "" ? robotState.errorString : qsTr("Ready"))
            opacity: 0.8
        }
    }
}
