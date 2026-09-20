import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 960
    height: 640
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
                onCurrentIndexChanged: root.currentPage = currentIndex
            }

            Label {
                text: robotState.connected
                      ? (qsTr("ONLINE") + " · RTT " + robotState.heartbeatRttMs + " ms")
                      : (robotState.connecting ? qsTr("CONNECTING…") : qsTr("OFFLINE"))
                color: robotState.connected ? "#1b7f3a" : "#8a1f1f"
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.currentPage

        ConnectPage {}
        DashboardPage {}
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
