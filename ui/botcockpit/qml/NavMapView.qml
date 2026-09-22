import QtQuick
import QtQuick.Window
import QtQuick.Layouts

// 2D map: occupancy background (demo corridor), A* path, goal, live pose.
// Pure presentation — data comes from RobotState / context properties.
Rectangle {
    id: root
    // Explicit default size — required so RowLayout does not collapse to 0×0
    implicitWidth: 320
    implicitHeight: 220
    Layout.minimumWidth: 220
    Layout.minimumHeight: 180
    Layout.fillWidth: true
    Layout.fillHeight: true
    color: "#f7fafc"
    border.color: "#cfd8e6"
    radius: 8
    clip: true

    // World bounds matching nav::GridMap demo (40x20 cells @0.5m, origin 0,-5)
    property double worldX0: 0.0
    property double worldY0: -5.0
    property double worldX1: 20.0
    property double worldY1: 5.0
    property double gridRes: 0.5

    readonly property double spanX: worldX1 - worldX0
    readonly property double spanY: worldY1 - worldY0

    function wx(x) {
        return (x - worldX0) / spanX * width
    }
    function wy(y) {
        return (1.0 - (y - worldY0) / spanY) * height
    }

    Canvas {
        id: mapCanvas
        anchors.fill: parent
        anchors.margins: 1
        antialiasing: true

        property var path: []
        property real poseX: 0
        property real poseY: 0
        property real poseYaw: 0
        property real goalX: 0
        property real goalY: 0
        property bool hasGoal: false

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            // occupancy (corridor walls + gap + blocks) — world = origin + cell * res
            ctx.fillStyle = "#d9e2ec"
            var res = gridRes
            function cellRect(cx, cy) {
                // world rect for cell (cx,cy) with GridMap origin (0, -5)
                var wx0 = worldX0 + cx * res
                var wy0 = worldY0 + cy * res
                var x0 = root.wx(wx0)
                var y1 = root.wy(wy0)
                var x1 = root.wx(wx0 + res)
                var y0 = root.wy(wy0 + res)
                ctx.fillRect(x0, y0, x1 - x0, y1 - y0)
            }
            for (var x = 0; x < 40; ++x) {
                cellRect(x, 0)
                cellRect(x, 19)
            }
            for (var y = 0; y < 20; ++y) {
                cellRect(0, y)
                cellRect(39, y)
            }
            // wall at cx=20 with gap cy 8..10
            for (y = 2; y < 18; ++y) {
                if (y === 8 || y === 9 || y === 10)
                    continue
                cellRect(20, y)
            }
            cellRect(5, 4)
            cellRect(6, 4)

            // grid light
            ctx.strokeStyle = "#eef2f7"
            ctx.lineWidth = 1
            for (x = 0; x <= 40; x += 4) {
                ctx.beginPath()
                ctx.moveTo(root.wx(x * res), 0)
                ctx.lineTo(root.wx(x * res), height)
                ctx.stroke()
            }
            for (y = 0; y <= 20; y += 4) {
                ctx.beginPath()
                ctx.moveTo(0, root.wy(y * res))
                ctx.lineTo(width, root.wy(y * res))
                ctx.stroke()
            }

            // A* path
            if (path && path.length > 1) {
                ctx.strokeStyle = "#1a6fd4"
                ctx.lineWidth = 2
                ctx.beginPath()
                for (var i = 0; i < path.length; ++i) {
                    var p = path[i]
                    var px = root.wx(p[0])
                    var py = root.wy(p[1])
                    if (i === 0)
                        ctx.moveTo(px, py)
                    else
                        ctx.lineTo(px, py)
                }
                ctx.stroke()
                // waypoints
                ctx.fillStyle = "#1a6fd4"
                for (i = 0; i < path.length; i += 2) {
                    p = path[i]
                    ctx.beginPath()
                    ctx.arc(root.wx(p[0]), root.wy(p[1]), 2, 0, Math.PI * 2)
                    ctx.fill()
                }
            }

            // goal
            if (hasGoal) {
                ctx.strokeStyle = "#0f8a4a"
                ctx.lineWidth = 2
                var gx = root.wx(goalX)
                var gy = root.wy(goalY)
                ctx.beginPath()
                ctx.moveTo(gx - 8, gy)
                ctx.lineTo(gx + 8, gy)
                ctx.moveTo(gx, gy - 8)
                ctx.lineTo(gx, gy + 8)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(gx, gy, 6, 0, Math.PI * 2)
                ctx.stroke()
            }

            // pose arrow
            var rx = root.wx(poseX)
            var ry = root.wy(poseY)
            var yaw = poseYaw
            ctx.save()
            ctx.translate(rx, ry)
            ctx.rotate(-yaw)  // canvas y-down
            ctx.fillStyle = robotState.estop ? "#c62828" : "#1565c0"
            ctx.beginPath()
            ctx.moveTo(10, 0)
            ctx.lineTo(-6, 5)
            ctx.lineTo(-6, -5)
            ctx.closePath()
            ctx.fill()
            ctx.restore()
        }

        function syncFromState() {
            path = robotState.navPath
            poseX = robotState.poseX
            poseY = robotState.poseY
            poseYaw = robotState.poseYaw
            goalX = robotState.navGoalX
            goalY = robotState.navGoalY
            hasGoal = robotState.navPathLen > 0
            requestPaint()
        }

        Connections {
            target: robotState
            function onNavStatusChanged() { mapCanvas.syncFromState() }
            function onPoseChanged() { mapCanvas.syncFromState() }
            function onEstopChanged() { mapCanvas.syncFromState() }
        }
        Component.onCompleted: syncFromState()
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
            text: robotState.navStatus + " · err " + robotState.trackErr.toFixed(2) + " m"
            color: "#5d6b80"
            font.pixelSize: 11
        }
    }
}
