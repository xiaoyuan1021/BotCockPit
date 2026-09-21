import QtQuick
import QtQuick.Window
import QtQuick.Controls

Button {
    id: btn

    property color accent: "#1a6fd4"

    leftPadding: 14
    rightPadding: 14
    topPadding: 8
    bottomPadding: 8
    font.bold: true

    // Resolve colors safely (avoid TypeError when Window is not ready)
    function wColor(name, fallback) {
        var w = Window.window
        if (w && w[name] !== undefined)
            return w[name]
        return fallback
    }

    contentItem: Text {
        text: btn.text
        font: btn.font
        color: btn.enabled ? btn.wColor("colInk", "#1c2430") : btn.wColor("colMuted", "#5d6b80")
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitHeight: 34
        radius: 8
        color: {
            var alt = btn.wColor("colSurfaceAlt", "#e8eef6")
            if (!btn.enabled)
                return alt
            if (btn.pressed)
                return "#c5daf0"
            if (btn.hovered)
                return "#d7e6f8"
            return alt
        }
        border.color: btn.enabled ? btn.accent : btn.wColor("colBorder", "#cfd8e6")
        border.width: 1
    }

    scale: btn.pressed ? 0.97 : 1.0
    Behavior on scale { NumberAnimation { duration: 80 } }
}
