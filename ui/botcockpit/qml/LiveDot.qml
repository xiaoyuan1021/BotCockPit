import QtQuick
import QtQuick.Window

Item {
    id: root
    property color base: "#3ddc97"
    property bool active: true
    readonly property var win: Window.window
    width: 12
    height: 12

    Rectangle {
        id: core
        anchors.centerIn: parent
        width: 8
        height: 8
        radius: 4
        color: root.active ? root.base : (win ? win.colMuted : "#8b93a7")
        opacity: root.active ? 0.95 : 0.4
    }
    Rectangle {
        anchors.centerIn: parent
        width: 8
        height: 8
        radius: 4
        color: core.color
        opacity: 0
        visible: root.active
        scale: 1
        SequentialAnimation on opacity {
            running: root.visible && root.active
            loops: Animation.Infinite
            NumberAnimation { from: 0.55; to: 0.0; duration: 1200; easing.type: Easing.OutQuad }
            PauseAnimation { duration: 200 }
        }
        SequentialAnimation on scale {
            running: root.visible && root.active
            loops: Animation.Infinite
            NumberAnimation { from: 1.0; to: 2.4; duration: 1200; easing.type: Easing.OutQuad }
        }
    }
}
