import QtQuick
import QtQuick.Window
import QtQuick.Layouts

// 2D corridor map for Plan A:
//   occupied + inflate(1) layer, A* planned path, remaining path, driven trail.
// Keep world frame in sync with sim/include/botcockpit_sim/nav.hpp
Rectangle {
    id: root
    implicitWidth: 420
    implicitHeight: 280
    Layout.minimumWidth: 280
    Layout.minimumHeight: 220
    Layout.fillWidth: true
    Layout.fillHeight: true
    color: "#fbfcfe"
    border.color: "#cfd8e6"
    radius: 8
    clip: true

    readonly property real worldX0: 0.0
    readonly property real worldY0: -5.0
    readonly property real worldX1: 20.0
    readonly property real worldY1: 5.0
    readonly property real gridRes: 0.5
    readonly property int gridW: 40
    readonly property int gridH: 20

    // Plot inset so axis labels / overlays are never clipped
    readonly property real padLeft: 30
    readonly property real padTop: 20
    readonly property real padRight: 10
    readonly property real padBottom: 12

    property var pathPts: []
    property var trailPts: []
    property int pathIdx: 0
    property real poseX: 1.25
    property real poseY: 0.0
    property real poseYaw: 0.0
    property real goalX: 0.0
    property real goalY: 0.0
    property bool hasGoal: false
    property var occ: []      // 1 = hard occupied (walls)
    property var infl: []     // 1 = inflate(1) halo (A* planning space)

    function plotW(cw) { return Math.max(40, cw - padLeft - padRight) }
    function plotH(ch) { return Math.max(40, ch - padTop - padBottom) }

    function worldToScreenX(wx, cw) {
        return padLeft + (wx - worldX0) / (worldX1 - worldX0) * plotW(cw)
    }
    function worldToScreenY(wy, ch) {
        return padTop + (1.0 - (wy - worldY0) / (worldY1 - worldY0)) * plotH(ch)
    }

    // Same occupancy as nav::GridMap::make_corridor_demo()
    function buildOcc() {
        var W = root.gridW, H = root.gridH
        var o = new Array(W * H)
        for (var i = 0; i < o.length; ++i)
            o[i] = 0
        function set(x, y) {
            if (x >= 0 && y >= 0 && x < W && y < H)
                o[y * W + x] = 1
        }
        for (var x = 0; x < W; ++x) {
            set(x, 0)
            set(x, H - 1)
        }
        for (var y = 0; y < H; ++y) {
            set(0, y)
            set(W - 1, y)
        }
        // Vertical wall, gap cy=7..11 (5 cells) — keep in sync with nav.hpp
        for (y = 2; y < H - 2; ++y) {
            if (y >= 7 && y <= 11)
                continue
            set(20, y)
        }
        set(5, 4)
        set(6, 4)
        return o
    }

    // inflate(1): 4-connected halo (dx^2+dy^2 <= 1), same as GridMap::inflate
    function buildInflated(occ) {
        var W = root.gridW, H = root.gridH
        var src = occ.slice()
        var out = occ.slice()
        function setv(x, y) {
            if (x >= 0 && y >= 0 && x < W && y < H)
                out[y * W + x] = 1
        }
        for (var y = 0; y < H; ++y) {
            for (var x = 0; x < W; ++x) {
                if (!src[y * W + x])
                    continue
                setv(x, y)
                setv(x + 1, y)
                setv(x - 1, y)
                setv(x, y + 1)
                setv(x, y - 1)
            }
        }
        return out
    }

    function listOfPairs(pl) {
        var pts = []
        if (!pl)
            return pts
        for (var i = 0; i < pl.length; ++i) {
            var p = pl[i]
            var x, y
            if (p && p.length >= 2) {
                x = Number(p[0])
                y = Number(p[1])
            } else if (p && p.x !== undefined) {
                x = Number(p.x)
                y = Number(p.y)
            }
            if (isFinite(x) && isFinite(y))
                pts.push([x, y])
        }
        return pts
    }

    function listOfFlat(flat) {
        var pts = []
        if (!flat)
            return pts
        var n = flat.length - (flat.length % 2)
        for (var i = 0; i + 1 < n; i += 2) {
            var x = Number(flat[i])
            var y = Number(flat[i + 1])
            if (isFinite(x) && isFinite(y))
                pts.push([x, y])
        }
        return pts
    }

    function syncFromState() {
        // Prefer flat arrays (unambiguous across QVariant→QML)
        var pts = listOfFlat(robotState.navPathFlat)
        if (pts.length < 1)
            pts = listOfPairs(robotState.navPath)
        pathPts = pts
        var tr = listOfFlat(robotState.navTrailFlat)
        if (tr.length < 1)
            tr = listOfPairs(robotState.navTrail)
        trailPts = tr
        pathIdx = robotState.navIdx
        poseX = robotState.poseX
        poseY = robotState.poseY
        poseYaw = robotState.poseYaw
        goalX = robotState.navGoalX
        goalY = robotState.navGoalY
        hasGoal = robotState.navPathLen > 0
        mapCanvas.requestPaint()
    }

    Component.onCompleted: {
        occ = buildOcc()
        infl = buildInflated(occ)
        syncFromState()
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
            var W = root.gridW
            var H = root.gridH
            var occ = root.occ
            var infl = root.infl

            function cellRect(cx, cy) {
                var x0 = root.worldToScreenX(root.worldX0 + cx * res, cw)
                var x1 = root.worldToScreenX(root.worldX0 + (cx + 1) * res, cw)
                var yTop = root.worldToScreenY(root.worldY0 + (cy + 1) * res, ch)
                var yBot = root.worldToScreenY(root.worldY0 + cy * res, ch)
                return [x0, yTop, Math.max(1, x1 - x0), Math.max(1, yBot - yTop)]
            }

            // 1) free background + inflate(1) halo (A* planning inflation)
            ctx.save()
            ctx.beginPath()
            ctx.rect(root.padLeft, root.padTop, root.plotW(cw), root.plotH(ch))
            ctx.clip()
            ctx.fillStyle = "#eef3f9"
            ctx.fillRect(root.padLeft, root.padTop, root.plotW(cw), root.plotH(ch))
            for (var y = 0; y < H; ++y) {
                for (var x = 0; x < W; ++x) {
                    if (!occ || occ[y * W + x])
                        continue
                    if (infl && infl[y * W + x]) {
                        // inflated free cell (body clearance)
                        ctx.fillStyle = "#d9e4f2"
                        var ir = cellRect(x, y)
                        ctx.fillRect(ir[0], ir[1], ir[2], ir[3])
                    }
                }
            }

            // 2) hard occupied cells
            ctx.fillStyle = "#7a8ba3"
            for (y = 0; y < H; ++y) {
                for (x = 0; x < W; ++x) {
                    if (occ && occ[y * W + x]) {
                        var r = cellRect(x, y)
                        ctx.fillRect(r[0], r[1], r[2], r[3])
                    }
                }
            }
            // keep map body + overlays clipped to plot until after trails
            // (clip restored before axis labels)

            // 3) grid: minor every cell, major every 5 cells (2.5 m)
            //    clipped to plot rect so labels sit in the margin
            var px0 = root.padLeft
            var py0 = root.padTop
            var px1 = root.padLeft + root.plotW(cw)
            var py1 = root.padTop + root.plotH(ch)
            ctx.save()
            ctx.beginPath()
            ctx.rect(px0, py0, px1 - px0, py1 - py0)
            ctx.clip()
            ctx.lineWidth = 1
            ctx.strokeStyle = "#e4eaf2"
            for (x = 0; x <= W; ++x) {
                var gx = root.worldToScreenX(root.worldX0 + x * res, cw)
                ctx.beginPath()
                ctx.moveTo(gx, py0)
                ctx.lineTo(gx, py1)
                ctx.stroke()
            }
            for (y = 0; y <= H; ++y) {
                var gy = root.worldToScreenY(root.worldY0 + y * res, ch)
                ctx.beginPath()
                ctx.moveTo(px0, gy)
                ctx.lineTo(px1, gy)
                ctx.stroke()
            }
            ctx.strokeStyle = "#c5d2e4"
            ctx.lineWidth = 1.2
            for (x = 0; x <= W; x += 5) {
                gx = root.worldToScreenX(root.worldX0 + x * res, cw)
                ctx.beginPath()
                ctx.moveTo(gx, py0)
                ctx.lineTo(gx, py1)
                ctx.stroke()
            }
            for (y = 0; y <= H; y += 5) {
                gy = root.worldToScreenY(root.worldY0 + y * res, ch)
                ctx.beginPath()
                ctx.moveTo(px0, gy)
                ctx.lineTo(px1, gy)
                ctx.stroke()
            }
            ctx.restore()

            // 4) axis labels in the margin (world meters)
            ctx.restore()
            ctx.fillStyle = "#5d6b80"
            ctx.font = "10px sans-serif"
            ctx.textAlign = "center"
            ctx.textBaseline = "top"
            for (x = 0; x <= 20; x += 5) {
                gx = root.worldToScreenX(x, cw)
                ctx.fillText(String(x), gx, 4)
            }
            ctx.textAlign = "right"
            ctx.textBaseline = "middle"
            for (var wy = -5; wy <= 5.01; wy += 2.5) {
                gy = root.worldToScreenY(wy, ch)
                var label = (Math.abs(wy) < 0.01) ? "0" : String(wy)
                ctx.fillText(label, root.padLeft - 6, gy)
            }

            // trails / path / markers (plot space; still inside previous restore)
            ctx.save()
            ctx.beginPath()
            ctx.rect(root.padLeft, root.padTop, root.plotW(cw), root.plotH(ch))
            ctx.clip()

            // 5) already-driven trail (orange, on top of plan)
            var trail = root.trailPts
            if (trail && trail.length > 1) {
                ctx.strokeStyle = "#e67e22"
                ctx.lineWidth = 4
                ctx.lineJoin = "round"
                ctx.lineCap = "round"
                ctx.globalAlpha = 0.95
                ctx.beginPath()
                for (var i = 0; i < trail.length; ++i) {
                    var tx = root.worldToScreenX(trail[i][0], cw)
                    var ty = root.worldToScreenY(trail[i][1], ch)
                    if (i === 0)
                        ctx.moveTo(tx, ty)
                    else
                        ctx.lineTo(tx, ty)
                }
                ctx.stroke()
                ctx.globalAlpha = 1
            }

            // 6) planned A* path: remaining (bold dashed) + consumed (faded)
            var pts = root.pathPts
            var idx = root.pathIdx
            if (pts && pts.length > 0) {
                // remaining (from cursor)
                if (pts.length > 1) {
                    ctx.strokeStyle = "#1565c0"
                    ctx.lineWidth = 3
                    ctx.setLineDash([7, 4])
                    ctx.beginPath()
                    var started = false
                    for (i = 0; i < pts.length; ++i) {
                        if (i < idx)
                            continue
                        var px = root.worldToScreenX(pts[i][0], cw)
                        var py = root.worldToScreenY(pts[i][1], ch)
                        if (!started) {
                            ctx.moveTo(px, py)
                            started = true
                        } else {
                            ctx.lineTo(px, py)
                        }
                    }
                    ctx.stroke()
                    ctx.setLineDash([])
                }

                // already-consumed plan (faded) so the full A* polyline stays visible
                if (idx > 0 && pts.length > 1) {
                    ctx.strokeStyle = "#8eb6e8"
                    ctx.lineWidth = 2
                    ctx.beginPath()
                    for (i = 0; i <= Math.min(idx, pts.length - 1); ++i) {
                        px = root.worldToScreenX(pts[i][0], cw)
                        py = root.worldToScreenY(pts[i][1], ch)
                        if (i === 0)
                            ctx.moveTo(px, py)
                        else
                            ctx.lineTo(px, py)
                    }
                    ctx.stroke()
                }

                // waypoint dots (A* cell centers)
                for (i = 0; i < pts.length; ++i) {
                    px = root.worldToScreenX(pts[i][0], cw)
                    py = root.worldToScreenY(pts[i][1], ch)
                    ctx.beginPath()
                    ctx.arc(px, py, i === idx ? 4 : 2.5, 0, Math.PI * 2)
                    ctx.fillStyle = i < idx ? "#8eb6e8" : "#1565c0"
                    ctx.fill()
                }
            }

            // 7) start marker (first path point)
            if (pts && pts.length > 0) {
                var sx = root.worldToScreenX(pts[0][0], cw)
                var sy = root.worldToScreenY(pts[0][1], ch)
                ctx.strokeStyle = "#5d6b80"
                ctx.lineWidth = 1.5
                ctx.beginPath()
                ctx.moveTo(sx, sy - 5)
                ctx.lineTo(sx + 5, sy)
                ctx.lineTo(sx, sy + 5)
                ctx.lineTo(sx - 5, sy)
                ctx.closePath()
                ctx.stroke()
            }

            // 8) goal cross
            if (root.hasGoal) {
                var gxs = root.worldToScreenX(root.goalX, cw)
                var gys = root.worldToScreenY(root.goalY, ch)
                ctx.strokeStyle = "#0f8a4a"
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(gxs - 9, gys)
                ctx.lineTo(gxs + 9, gys)
                ctx.moveTo(gxs, gys - 9)
                ctx.lineTo(gxs, gys + 9)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(gxs, gys, 5, 0, Math.PI * 2)
                ctx.stroke()
            }

            // 9) robot
            var rx = root.worldToScreenX(root.poseX, cw)
            var ry = root.worldToScreenY(root.poseY, ch)
            ctx.save()
            ctx.translate(rx, ry)
            ctx.rotate(-root.poseYaw)
            ctx.fillStyle = robotState.estop ? "#c62828" : "#1565c0"
            ctx.beginPath()
            ctx.moveTo(11, 0)
            ctx.lineTo(-7, 6)
            ctx.lineTo(-7, -6)
            ctx.closePath()
            ctx.fill()
            ctx.restore()
            ctx.restore()
        }

        Connections {
            target: robotState
            function onNavStatusChanged() { root.syncFromState() }
            function onNavTrailChanged() { root.syncFromState() }
            function onPoseChanged() { root.syncFromState() }
            function onEstopChanged() { root.syncFromState() }
            function onHasRobotStateChanged() { root.syncFromState() }
            function onTrackErrChanged() { mapCanvas.requestPaint() }
        }
    }

    // Title + live metrics (chip sits in plot corner, below axis labels)
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: root.padLeft + 4
        anchors.topMargin: root.padTop + 4
        radius: 6
        color: "#f0f4f9"
        border.color: "#cfd8e6"
        opacity: 0.94
        implicitWidth: titleCol.implicitWidth + 16
        implicitHeight: titleCol.implicitHeight + 12
        Column {
            id: titleCol
            anchors.centerIn: parent
            spacing: 2
            Text {
                text: qsTr("Nav map (A* corridor)")
                font.bold: true
                color: "#1c2430"
                font.pixelSize: 12
            }
            Text {
                text: robotState.navStatus
                      + "  |  wp " + robotState.navIdx + "/"
                      + Math.max(0, robotState.navPathLen - 1)
                      + "  |  err " + robotState.trackErr.toFixed(2) + " m"
                color: "#5d6b80"
                font.pixelSize: 11
            }
        }
    }

    // Legend chip (not on top of walls)
    Rectangle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 6
        radius: 6
        color: "#f0f4f9"
        border.color: "#cfd8e6"
        opacity: 0.94
        implicitWidth: legendRow.implicitWidth + 16
        implicitHeight: legendRow.implicitHeight + 10
        Row {
            id: legendRow
            anchors.centerIn: parent
            spacing: 10
            Text {
                text: qsTr("■ wall")
                color: "#5a6d84"
                font.pixelSize: 10
            }
            Text {
                text: qsTr("■ inflate")
                color: "#5b7fa6"
                font.pixelSize: 10
            }
            Text {
                text: qsTr("⋯ plan")
                color: "#1565c0"
                font.pixelSize: 10
                font.bold: true
            }
            Text {
                text: qsTr("━ trail")
                color: "#e67e22"
                font.pixelSize: 10
                font.bold: true
            }
            Text {
                text: qsTr("＋ goal")
                color: "#0f8a4a"
                font.pixelSize: 10
            }
        }
    }
}
