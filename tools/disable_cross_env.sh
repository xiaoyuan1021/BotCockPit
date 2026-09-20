#!/usr/bin/env bash
# 备份并隔离 aarch64 交叉编译环境对 ROS2 host 构建的污染
# 用法（在 Ubuntu 编译机上）：
#   bash tools/disable_cross_env.sh
#   bash tools/disable_cross_env.sh --also-tar-sysroot   # 连 SYSROOT 一起打包备份
#   bash tools/disable_cross_env.sh --restore ~/.bashrc.bak-cross-YYYYMMDDHHMMSS

set -euo pipefail

BASHRC="${HOME}/.bashrc"
STAMP="$(date +%Y%m%d-%H%M%S)"
BACKUP_DIR="${HOME}/toolchain-backups"
mkdir -p "$BACKUP_DIR"

log() { echo "$*"; }

if [[ "${1:-}" == "--restore" ]]; then
  SRC="${2:-}"
  if [[ -z "$SRC" || ! -f "$SRC" ]]; then
    echo "ERROR: need backup file path" >&2
    exit 2
  fi
  cp -a "$SRC" "$BASHRC"
  log "Restored $BASHRC from $SRC"
  log "Open a new shell: exec bash"
  exit 0
fi

# --- 1) Backup .bashrc ---
BAK="${BACKUP_DIR}/bashrc.bak-cross-${STAMP}"
cp -a "$BASHRC" "$BAK"
log "Backup .bashrc -> $BAK"

# --- 2) Comment out cross-compile exports (idempotent) ---
python3 - <<'PY'
import os, re
from pathlib import Path
home = Path.home()
path = home / ".bashrc"
text = path.read_text(encoding="utf-8", errors="replace")
keys = [
    "CROSS_COMPILE=", "CC=", "CXX=", "AR=", "RANLIB=",
    "TARGET_TRIPLE=", "TARGET_ARCH=", "SYSROOT=", "PREFIX=",
]
# 仅处理 export 行，且值里含交叉相关；避免误伤
patterns = [
    r'^(export\s+CROSS_COMPILE=.*aarch64.*)$',
    r'^(export\s+CC=\$\{?CROSS_COMPILE\}?.*)$',
    r'^(export\s+CXX=\$\{?CROSS_COMPILE\}?.*)$',
    r'^(export\s+AR=\$\{?CROSS_COMPILE\}?.*)$',
    r'^(export\s+RANLIB=\$\{?CROSS_COMPILE\}?.*)$',
    r'^(export\s+TARGET_TRIPLE=.*aarch64.*)$',
    r'^(export\s+TARGET_ARCH=aarch64.*)$',
    r'^(export\s+SYSROOT=.*)$',
    r'^(export\s+PREFIX=\$\{?SYSROOT\}?.*)$',
]
lines = text.splitlines(keepends=True)
out = []
changed = 0
for line in lines:
    stripped = line.lstrip()
    if stripped.startswith("#"):
        out.append(line)
        continue
    matched = False
    body = stripped
    if "export" in body and any(k in body for k in keys):
        if ("CROSS_COMPILE" in body or "TARGET_TRIPLE" in body or "TARGET_ARCH" in body
            or "SYSROOT=" in body or "PREFIX=" in body
            or re.search(r"export\s+CC=", body) or re.search(r"export\s+CXX=", body)
            or re.search(r"export\s+AR=", body) or re.search(r"export\s+RANLIB=", body)):
            # 再收紧：必须是交叉相关或引用 CROSS_COMPILE/SYSROOT
            if ("aarch64" in body or "CROSS_COMPILE" in body or "SYSROOT" in body
                or "TARGET_" in body or "PREFIX" in body):
                matched = True
    if matched:
        # 保留原缩进
        indent = line[: len(line) - len(line.lstrip())]
        out.append(f"{indent}# [cross-disable {os.environ.get('STAMP','')}] {line.lstrip()}")
        changed += 1
    else:
        out.append(line)
path.write_text("".join(out), encoding="utf-8")
print(f"commented {changed} cross-export line(s) in {path}")
PY

# --- 3) Optionally tar SYSROOT (backup only, do not delete) ---
if [[ "${2:-}" == "--also-tar-sysroot" || "${1:-}" == "--also-tar-sysroot" ]]; then
  SYSROOT_VAL="$(grep -E '^#?\s*export SYSROOT=' "$BAK" | tail -n1 | sed -E 's/.*SYSROOT=//')"
  if [[ -n "$SYSROOT_VAL" && -d "$SYSROOT_VAL" ]]; then
    DEST="${BACKUP_DIR}/sysroot-$(basename "$SYSROOT_VAL")-${STAMP}.tar.gz"
    log "Packing SYSROOT $SYSROOT_VAL -> $DEST (this may take a while / large)"
    tar -czf "$DEST" -C "$(dirname "$SYSROOT_VAL")" "$(basename "$SYSROOT_VAL")"
    sha256sum "$DEST" | tee -a "${BACKUP_DIR}/checksums-${STAMP}.txt"
  else
    log "SYSROOT path missing or not a dir: ${SYSROOT_VAL:-<empty>} — skip tar"
  fi
fi

log ""
log "=== Verify in NEW shell (do not use current polluted shell) ==="
log "  exec bash"
log "  echo \"CC=\$CC CXX=\$CXX SYSROOT=\$SYSROOT\"   # 应为空"
log "  command -v gcc g++ cc c++"
log "  gcc -dumpmachine   # 应为 x86_64-linux-gnu"
log "  python3-config --includes"
log ""
log "=== Clean rebuild BotCockpit packages ==="
log "  source /opt/ros/humble/setup.bash"
log "  cd /home/xiaoyuan/ros2_ws"
log "  # 确认 bridge/sim 已在 src 下；若仓库在 Bot/Bot_Project，可："
log "  #   mkdir -p src && ln -sfn /home/xiaoyuan/ros2_ws/Bot/Bot_Project/bridge src/botcockpit_bridge"
log "  #   ln -sfn /home/xiaoyuan/ros2_ws/Bot/Bot_Project/sim    src/botcockpit_sim"
log "  rm -rf build install log   # 或至少删除报错包的 build 目录"
log "  colcon build --packages-select botcockpit_bridge botcockpit_sim"
log ""
log "Cross env DISABLED (commented). Backup: $BAK"
log "To restore later: bash tools/disable_cross_env.sh --restore $BAK"
log ""
log "Do NOT apt-remove aarch64 packages or rm sysroot until native build succeeds."
log "After success, optional delete example (only if you confirm backup):"
log "  # sudo rm -rf /home/xiaoyuan/rootfs/mount_dir.disabled-*"
log "  # sudo apt remove gcc-aarch64-linux-gnu g++-aarch64-linux-gnu   # 可选，可能影响其它板级工程"
