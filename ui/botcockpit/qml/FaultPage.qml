import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: faultPage
    padding: 0
    background: Item {}
    readonly property var win: Window.window

    property string levelFilter: ""

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("Faults & Logs")
                font.pixelSize: 22
                font.bold: true
                color: "#1c2430"
                Layout.fillWidth: true
            }
            ToolBtn {
                text: qsTr("All")
                accent: levelFilter === "" ? "#1a6fd4" : "#5d6b80"
                onClicked: { levelFilter = ""; faultModel.setLevelFilter("") }
            }
            ToolBtn {
                text: "ERROR"
                accent: levelFilter === "ERROR" ? "#c62828" : "#5d6b80"
                onClicked: { levelFilter = "ERROR"; faultModel.setLevelFilter("ERROR") }
            }
            ToolBtn {
                text: "WARN"
                accent: levelFilter === "WARN" ? "#b07000" : "#5d6b80"
                onClicked: { levelFilter = "WARN"; faultModel.setLevelFilter("WARN") }
            }
            ToolBtn {
                text: qsTr("Inject demo fault")
                accent: "#b07000"
                enabled: robotState.connected && robotState.hasRobotState
                onClicked: injectHint.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTr("Active faults: ") + robotState.faultCount
                      + (robotState.faultsSummary.length ? (" — " + robotState.faultsSummary) : "")
                color: robotState.faultCount > 0 ? "#c62828" : "#5d6b80"
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height * 0.45
            radius: 10
            color: "#ffffff"
            border.color: "#cfd8e6"
            clip: true

            ListView {
                id: faultView
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                model: faultModel
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                header: Rectangle {
                    width: faultView.width
                    height: 30
                    radius: 6
                    color: "#eef2f7"
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        Text { width: parent.width * 0.22; text: "code"; font.bold: true; color: "#5d6b80"; anchors.verticalCenter: parent.verticalCenter }
                        Text { width: parent.width * 0.14; text: "level"; font.bold: true; color: "#5d6b80"; anchors.verticalCenter: parent.verticalCenter }
                        Text { width: parent.width * 0.18; text: "node"; font.bold: true; color: "#5d6b80"; anchors.verticalCenter: parent.verticalCenter }
                        Text { width: parent.width * 0.46; text: "detail"; font.bold: true; color: "#5d6b80"; anchors.verticalCenter: parent.verticalCenter }
                    }
                }

                delegate: Rectangle {
                    width: faultView.width
                    height: 36
                    radius: 6
                    color: index % 2 === 0 ? "#f7f9fc" : "#ffffff"
                    required property string code
                    required property string level
                    required property string node
                    required property string detail
                    required property int index
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        Text { width: parent.width * 0.22; text: code; color: "#1c2430"; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                        Text {
                            width: parent.width * 0.14
                            text: level
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                            color: level === "ERROR" ? "#c62828" : level === "WARN" ? "#b07000" : "#5d6b80"
                        }
                        Text { width: parent.width * 0.18; text: node; color: "#5d6b80"; anchors.verticalCenter: parent.verticalCenter }
                        Text { width: parent.width * 0.46; text: detail; color: "#1c2430"; anchors.verticalCenter: parent.verticalCenter; elide: Text.ElideRight }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: faultView.count === 0
                    text: robotState.connected
                          ? qsTr("No faults (filter: %1)").arg(levelFilter.length ? levelFilter : qsTr("all"))
                          : qsTr("Offline — connect to see faults")
                    color: "#5d6b80"
                }
            }
        }

        Text { text: qsTr("Event log"); font.bold: true; color: "#1c2430" }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 10
            color: "#ffffff"
            border.color: "#cfd8e6"
            clip: true
            ListView {
                anchors.fill: parent
                anchors.margins: 8
                model: robotState.logLines
                clip: true
                delegate: Text {
                    required property string modelData
                    text: modelData
                    color: modelData.indexOf("NACK") >= 0 || modelData.indexOf("error") >= 0
                           ? "#c62828" : "#1c2430"
                    font.family: "monospace"
                    font.pixelSize: 12
                }
            }
        }
    }

    Dialog {
        id: injectHint
        modal: true
        title: qsTr("Fault injection")
        standardButtons: Dialog.Ok
        anchors.centerIn: Overlay.overlay
        width: 460
        contentItem: Item {
            implicitWidth: 420
            implicitHeight: hintMsg.implicitHeight
            Text {
                id: hintMsg
                width: 420
                wrapMode: Text.WordWrap
                color: "#1c2430"
                text: qsTr("Use tools on the robot host:\n  python3 tools/inject_fault.py\n  python3 tools/inject_fault.py --clear\n\nOr ros2 topic pub botcockpit/cmd (see WEEK3/WEEK2 docs).")
            }
        }
    }
}
