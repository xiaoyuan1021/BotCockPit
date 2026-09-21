import QtQuick
import QtQuick.Window
import QtQuick.Controls

Item {
    id: root
    property color base: "#0f8a4a"
    property bool active: true
    width: 12
    height: 12

    function idleColor() {
        var w = Window.window
        return (w && w.colMuted !== undefined) ? w.colMuted : "#5d6b80"
    }

    Rectangle {
        id: core
        anchors.centerIn: parent
        width: 8
        height: 8
        radius: 4
        color: root.active ? root.base : root.idleColor()
        opacity: root.active ? 1.0 : 0.45
    }
    Rectangle {
        anchors.centerIn: parent
        width: 8
        height: 8
        radius: 4
        color: core.color
        opacity: 0
        visible: root.active
        scale: 1.0
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
