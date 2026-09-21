import QtQuick
import QtQuick.Window
import QtQuick.Controls

Button {
    id: btn
    property color accent: (Window.window && Window.window.colAccent) ? Window.window.colAccent : "#3aa0ff"
    leftPadding: 14
    rightPadding: 14
    topPadding: 8
    bottomPadding: 8
    font.bold: true
    contentItem: Text {
        text: btn.text
        font: btn.font
        color: btn.enabled
               ? ((Window.window && Window.window.colInk) ? Window.window.colInk : "#e8ecf4")
               : ((Window.window && Window.window.colMuted) ? Window.window.colMuted : "#8b93a7")
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        opacity: btn.down ? 0.85 : 1.0
        Behavior on opacity { NumberAnimation { duration: 90 } }
    }
    background: Rectangle {
        implicitHeight: 34
        radius: 8
        color: {
            var alt = (Window.window && Window.window.colSurfaceAlt) ? Window.window.colSurfaceAlt : "#222938"
            var border = (Window.window && Window.window.colBorder) ? Window.window.colBorder : "#2e3648"
            if (!btn.enabled) return alt
            if (btn.down) return Qt.darker(btn.accent, 1.25)
            if (btn.hovered) return Qt.darker(btn.accent, 1.1)
            return alt
        }
        border.color: btn.enabled ? Qt.darker(btn.accent, 0.85)
                    : ((Window.window && Window.window.colBorder) ? Window.window.colBorder : "#2e3648")
        border.width: 1
        Behavior on color { ColorAnimation { duration: 120 } }
    }
    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutQuad } }
    scale: down ? 0.97 : 1.0
}
