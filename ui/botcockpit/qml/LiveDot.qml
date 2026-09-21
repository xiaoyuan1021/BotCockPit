import QtQuick
import QtQuick.Window
import QtQuick.Controls

Item {
    id: root
    property color base: "#0f8a4a"
    property bool active: true
    readonly property var win: Window.window
    width: 12
    height: 12

    function idleColor() {
        return (win && win.colMuted) ? win.colMuted : "#5d6b80"
    }

    Rectangle {
        id: core
        anchors.centerIn: parent
        width: 8
        height: 8
        radius: 4
        color: root.active ? root.base : root.idleColor()
        opacity: root.active ? 1.0 : 0.45
        border.color: (win && win.colBorder) ? win.colBorder : "#cfd8e6"
        border.width: root.active ? 0 : 1
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
            NumberAnimation { from: 0.45; to: 0.0; duration: 1100; easing.type: Easing.OutQuad }
            PauseAnimation { duration: 250 }
        }
        SequentialAnimation on scale {
            running: root.visible && root.active
            loops: Animation.Infinite
            NumberAnimation { from: 1.0; to: 2.2; duration: 1100; easing.type: Easing.OutQuad }
        }
    }
}
