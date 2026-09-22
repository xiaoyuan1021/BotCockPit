import QtQuick
import QtQuick.Window
import QtQuick.Layouts

// 2D corridor map: occupancy + A* path + goal + robot pose.
// All geometry is computed in *canvas* pixels to avoid root/item size mismatch.
Rectangle {
    id: root
    implicitWidth: 360
    implicitHeight: 240
    Layout.minimumWidth: 240
    Layout.minimumHeight: 200
    Layout.fillWidth: true
    Layout.fillHeight: true
    color: "#ffffff"
    border.color: "#cfd8e6"
    radius: 8
    clip: true

    // World frame for nav::GridMap demo (keep in sync with nav.hpp)
    readonly property real worldX0: 0.0
    readonly property real worldY0: -5.0
    readonly property real worldX1: 20.0
    readonly property real worldY1: 5.0
    readonly property real gridRes: 0.5
    readonly property int gridW: 40
    readonly property int gridH: 20

    // Last known nav/pose snapshot for canvas
    property var pathPts: []
    property real poseX: 1.25
    property real poseY: 0.0
    property real poseYaw: 0.0
    property real goalX: 0.0
    property real goalY: 0.0
    property bool hasGoal: false

    function worldToScreenX(wx, cw) {
        return (wx - worldX0) / (worldX1 - worldX0) * cw
    }
    function worldToScreenY(wy, ch) {
        // y-up world → y-down screen
        return (1.0 - (wy - worldY0) / (worldY1 - worldY0)) * ch
    }

    function syncFromState() {
        var pl = robotState.navPath
        // Copy to plain JS array so canvas repaints reliably
        var pts = []
        for (var i = 0; i < pl.length; ++i) {
            var p = pl[i]
            pts.push([Number(p[0]), Number(p[1])])
        }
        pathPts = pts
        poseX = robotState.poseX
        poseY = robotState.poseY
        poseYaw = robotState.poseYaw
        goalX = robotState.navGoalX
        goalY = robotState.navGoalY
        hasGoal = robotState.navPathLen > 0
        mapCanvas.requestPaint()
    }

    Canvas {
        id: mapCanvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            var cw = width
            var ch = height
            ctx.clearRect(0, 0, cw, ch)

            var res = root.gridRes
            ctx.fillStyle = "#dde5ee"
            function cellRect(cx, cy) {
                var x0 = root.worldToScreenX(root.worldX0 + cx * res, cw)
                var x1 = root.worldToScreenX(root.worldX0 + (cx + 1) * res, cw)
                // screen y grows down: top = (cy+1)*res, bottom = cy*res
                var yTop = root.worldToScreenY(root.worldY0 + (cy + 1) * res, ch)
                var yBot = root.worldToScreenY(root.worldY0 + cy * res, ch)
                ctx.fillRect(x0, yTop, x1 - x0, yBot - yTop)
            }
            // Borders
            for (var x = 0; x < root.gridW; ++x) {
                cellRect(x, 0)
                cellRect(x, root.gridH - 1)
            }
            for (var y = 0; y < root.gridH; ++y) {
                cellRect(0, y)
                cellRect(root.gridW - 1, y)
            }
            // Wall with gap at cy = 8,9,10 (nav::make_corridor_demo)
            for (y = 2; y < root.gridH - 2; ++y) {
                if (y === 8 || y === 9 || y === 10)
                    continue
                cellRect(20, y)
            }
            cellRect(5, 4)
            cellRect(6, 4)

            // Light grid
            ctx.strokeStyle = "#eef2f7"
            ctx.lineWidth = 1
            for (x = 0; x <= root.gridW; x += 4) {
                var gx = root.worldToScreenX(root.worldX0 + x * res, cw)
                ctx.beginPath()
                ctx.moveTo(gx, 0)
                ctx.lineTo(gx, ch)
                ctx.stroke()
            }
            for (y = 0; y <= root.gridH; y += 4) {
                var gy = root.worldToScreenY(root.worldY0 + y * res, ch)
                ctx.beginPath()
                ctx.moveTo(0, gy)
                ctx.lineTo(cw, gy)
                ctx.stroke()
            }

            // Path
            var pts = root.pathPts
            if (pts && pts.length > 1) {
                ctx.strokeStyle = "#1a6fd4"
                ctx.lineWidth = 2
                ctx.beginPath()
                for (var i = 0; i < pts.length; ++i) {
                    var px = root.worldToScreenX(pts[i][0], cw)
                    var py = root.worldToScreenY(pts[i][1], ch)
                    if (i === 0)
                        ctx.moveTo(px, py)
                    else
                        ctx.lineTo(px, py)
                }
                ctx.stroke()
            }

            // Goal cross
            if (root.hasGoal) {
                var gxs = root.worldToScreenX(root.goalX, cw)
                var gys = root.worldToScreenY(root.goalY, ch)
                ctx.strokeStyle = "#0f8a4a"
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(gxs - 8, gys)
                ctx.lineTo(gxs + 8, gys)
                ctx.moveTo(gxs, gys - 8)
                ctx.lineTo(gxs, gys + 8)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(gxs, gys, 5, 0, Math.PI * 2)
                ctx.stroke()
            }

            // Robot triangle
            var rx = root.worldToScreenX(root.poseX, cw)
            var ry = root.worldToScreenY(root.poseY, ch)
            ctx.save()
            ctx.translate(rx, ry)
            // world yaw CCW; canvas y-down → draw with -yaw
            ctx.rotate(-root.poseYaw)
            ctx.fillStyle = robotState.estop ? "#c62828" : "#1565c0"
            ctx.beginPath()
            ctx.moveTo(11, 0)
            ctx.lineTo(-7, 6)
            ctx.lineTo(-7, -6)
            ctx.closePath()
            ctx.fill()
            ctx.restore()
        }

        Connections {
            target: robotState
            function onNavStatusChanged() { root.syncFromState() }
            function onPoseChanged() { root.syncFromState() }
            function onEstopChanged() { root.syncFromState() }
            function onHasRobotStateChanged() { root.syncFromState() }
        }
        Component.onCompleted: root.syncFromState()
    }

    Column {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        spacing: 2
        Text {
            text: qsTr("Nav map (A* corridor)")
            font.bold: true
            color: "#1c2430"
            font.pixelSize: 12
        }
        Text {
            text: robotState.navStatus + " | err " + robotState.trackErr.toFixed(2) + " m"
            color: "#5d6b80"
            font.pixelSize: 11
        }
    }
}
