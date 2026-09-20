#!/usr/bin/env bash
# BotCockpit — 诊断 ROS2/CMake 工具链与 PythonLibs 冲突
# 在编译机（Ubuntu）上执行：
#   bash tools/diag_toolchain.sh
# 可选：备份候选工具链目录
#   bash tools/diag_toolchain.sh --backup /path/to/toolchain_dir

set -euo pipefail

REPORT_DIR="$(pwd)/_toolchain_diag"
mkdir -p "$REPORT_DIR"
REPORT="$REPORT_DIR/report-$(date +%Y%m%d-%H%M%S).txt"

log() { echo "$*" | tee -a "$REPORT"; }

log "=== BotCockpit toolchain diagnostic ==="
log "host: $(uname -a)"
log "date: $(date -Is)"
log "pwd:  $(pwd)"
log ""

log "=== 1) Environment (compile-related) ==="
env | grep -iE '^(CMAKE|CC|CXX|CPP|LD|PATH|PYTHON|ROS|AMENT|COLCON|CONDA|TOOLCHAIN|SYSROOT|PKG_CONFIG|QMAKE|QT)=' \
  | sort | tee -a "$REPORT" || true
log ""

log "=== 2) Shell rc hooks (possible toolchain exports) ==="
for f in "$HOME/.bashrc" "$HOME/.profile" "$HOME/.bash_profile" /etc/environment /etc/profile; do
  if [[ -f "$f" ]]; then
    log "--- $f ---"
    grep -n -iE 'toolchain|CMAKE_|cross|SYSROOT|aarch64|arm-|riscv|CC=|CXX=|conda|ROS_DISTRO' "$f" \
      | tee -a "$REPORT" || log "(no matches)"
  fi
done
log ""

log "=== 3) Active compilers / python ==="
log "which cc:  $(command -v cc || true)"
log "cc -v:     $(cc -v 2>&1 | tail -n 3 || true)"
log "which c++: $(command -v c++ || true)"
log "c++ -v:    $(c++ -v 2>&1 | tail -n 3 || true)"
log "CC=$CC CXX=$CXX CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE:-}"
log "which python3: $(command -v python3 || true)"
log "python3:   $(python3 -V 2>&1 || true)"
log "python3-config --includes: $(python3-config --includes 2>&1 || true)"
log "python3-config --ldflags:  $(python3-config --ldflags 2>&1 || true)"
log "dpkg python3-dev: $(dpkg -l python3-dev libpython3-dev 2>/dev/null | awk '/^ii/{print $2,$3}' || true)"
log ""

log "=== 4) Find CMAKE_TOOLCHAIN_FILE candidates on disk ==="
# 仅常见位置，避免全盘扫描过久
CANDIDATES=(
  "$HOME"
  "$HOME/opt"
  "$HOME/toolchains"
  "$HOME/cross"
  "$HOME/aarch64"
  "/opt"
  "/opt/toolchains"
  "/opt/cross"
  "/usr/local"
)
for base in "${CANDIDATES[@]}"; do
  [[ -d "$base" ]] || continue
  find "$base" -maxdepth 4 -type f \( -name '*toolchain*.cmake' -o -name 'toolchain.cmake' \) 2>/dev/null \
    | tee -a "$REPORT" || true
done
log ""

log "=== 5) Workspace CMakeCache / colcon.meta ==="
WS="${ROS_WS:-$HOME/botcockpit_ws}"
if [[ -d "$WS" ]]; then
  log "workspace: $WS"
  if [[ -f "$WS/colcon.meta" ]]; then
    log "--- colcon.meta ---"
    cat "$WS/colcon.meta" | tee -a "$REPORT"
  else
    log "(no colcon.meta)"
  fi
  # 扫描缓存里的工具链与 Python 线索
  while IFS= read -r cache; do
    log "--- $cache ---"
    grep -E 'CMAKE_TOOLCHAIN_FILE|CMAKE_C_COMPILER|CMAKE_CXX_COMPILER|CMAKE_SYSROOT|CMAKE_FIND_ROOT_PATH|PYTHON_|CMAKE_CROSSCOMPILING' \
      "$cache" 2>/dev/null | tee -a "$REPORT" || true
  done < <(find "$WS" -path '*/CMakeCache.txt' 2>/dev/null | head -n 20)
else
  log "workspace not found: $WS (set ROS_WS=... if different)"
fi
log ""

log "=== 6) Which package triggers rosidl_generate_interfaces? ==="
if [[ -d "$WS/src" ]]; then
  grep -Rsn --include='CMakeLists.txt' 'rosidl_generate_interfaces' "$WS/src" 2>/dev/null \
    | tee -a "$REPORT" || log "(none in \$WS/src)"
fi
log ""

log "Report written: $REPORT"
log "Next: read section 2/4/5 to locate the toolchain path, then backup it."

if [[ "${1:-}" == "--backup" ]]; then
  SRC="${2:-}"
  if [[ -z "$SRC" || ! -e "$SRC" ]]; then
    echo "ERROR: --backup needs an existing path, e.g. --backup /opt/cross/gcc-aarch64" >&2
    exit 2
  fi
  TS=$(date +%Y%m%d-%H%M%S)
  DEST="$HOME/toolchain-backups/$(basename "$SRC")-$TS.tar.gz"
  mkdir -p "$HOME/toolchain-backups"
  log "=== Backup $SRC -> $DEST ==="
  tar -czf "$DEST" -C "$(dirname "$SRC")" "$(basename "$SRC")"
  sha256sum "$DEST" | tee -a "$REPORT"
  log "Backup done. Prefer rename/disable over delete until colcon build succeeds."
fi
