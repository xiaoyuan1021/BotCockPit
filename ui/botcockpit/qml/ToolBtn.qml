import QtQuick
import QtQuick.Window
import QtQuick.Controls

Button {
    id: btn
    property color accent: (Window.window && Window.window.colAccent) ? Window.window.colAccent : "#1a6fd4"
    leftPadding: 14
    rightPadding: 14
    topPadding: 8
    bottomPadding: 8
    font.bold: true

    readonly property color ink: (Window.window && Window.window.colInk) ? Window.window.colInk : "#1c2430"
    readonly property color muted: (Window.window && Window.window.colMuted) ? Window.window.colMuted : "#5d6b80"
    readonly property color alt: (Window.window && Window.window.colSurfaceAlt) ? Window.window.colSurfaceAlt : "#e8eef6"
    readonly property color borderCol: (Window.window && Window.window.colBorder) ? Window.window.colBorder : "#cfd8e6"

    contentItem: Text {
        text: btn.text
        font: btn.font
        color: btn.enabled ? btn.ink : btn.muted
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        opacity: btn.down ? 0.85 : 1.0
        Behavior on opacity { NumberAnimation { duration: 90 } }
    }
    background: Rectangle {
        implicitHeight: 34
        radius: 8
        color: {
            if (!btn.enabled) return btn.alt
            if (btn.down) return Qt.darker(btn.accent, 1.2)
            if (btn.hovered) return Qt.light(btn.accent, 1.55)
            return btn.alt
        }
        border.color: btn.enabled ? btn.accent : btn.borderCol
        border.width: 1
        Behavior on color { ColorAnimation { duration: 120 } }
    }
    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutQuad } }
    scale: down ? 0.97 : 1.0
}
