#!/usr/bin/env bash
# ============================================================================
# scripts/ios/setup_device_screenshot.sh
#
# 一键准备 iOS 17+ 真机截图环境（用于 atest.sh run -t amap -p ios 等场景）。
#
# 工作流程（每步先检测，不通才执行）：
#   1) pymobiledevice3 是否安装
#   2) iOS 设备是否连接并已配对
#   3) 设备开发者模式是否启用 (无法自动开，只能轮询等待)
#   4) DeveloperDiskImage 是否挂载 (未挂自动挂)
#   5) tunneld 是否在跑 (未跑自动后台启动，需要 sudo)
#   6) 实测一发 dvt screenshot 验证全链路
#
# 用法:
#   ./scripts/ios/setup_device_screenshot.sh           # 检测 + 自动修复
#   ./scripts/ios/setup_device_screenshot.sh --check   # 仅检测，不修复
#   ./scripts/ios/setup_device_screenshot.sh --verbose # 详细输出
# ============================================================================

set -euo pipefail

CHECK_ONLY=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --check)    CHECK_ONLY=true; shift ;;
    --verbose)  VERBOSE=true; shift ;;
    -h|--help)
      sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'
      exit 0 ;;
    *) echo "unknown arg: $1"; exit 2 ;;
  esac
done

# ---------- 颜色输出 ----------
_red()    { printf "\033[31m%s\033[0m\n" "$*" >&2; }
_green()  { printf "\033[32m%s\033[0m\n" "$*"; }
_yellow() { printf "\033[33m%s\033[0m\n" "$*" >&2; }
_cyan()   { printf "\033[36m%s\033[0m\n" "$*"; }

step()    { _cyan   "==> $*"; }
ok()      { _green  "  ✓ $*"; }
warn()    { _yellow "  ! $*"; }
fail()    { _red    "  ✗ $*"; }
die()     { _red    "[FATAL] $*"; exit 1; }

# pymobiledevice3 / inquirer3 在被脚本/subprocess 调用时若继承 stdin，
# 会试图 setcbreak 而抛 termios.error。统一用 < /dev/null。
PMD3() { pymobiledevice3 "$@" < /dev/null; }

TUNNELD_LOG="${TMPDIR:-/tmp}/agenui_ios_tunneld.log"
SCREENSHOT_TEST="${TMPDIR:-/tmp}/agenui_ios_setup_test.png"

# ============================================================================
# Step 1: pymobiledevice3
# ============================================================================
ensure_pymobiledevice3() {
  step "[1/6] 检查 pymobiledevice3"
  if command -v pymobiledevice3 >/dev/null 2>&1; then
    local v
    v=$(PMD3 version 2>/dev/null | head -1 || echo "unknown")
    ok "pymobiledevice3 已安装 ($v)"
    return 0
  fi
  fail "pymobiledevice3 未安装"
  $CHECK_ONLY && return 1

  step "  → 自动安装中..."
  if ! command -v pipx >/dev/null 2>&1; then
    if command -v brew >/dev/null 2>&1; then
      brew install pipx
    else
      python3 -m pip install --user pipx
    fi
    # 确保 pipx 路径可用
    pipx ensurepath >/dev/null 2>&1 || true
    export PATH="$HOME/.local/bin:$PATH"
  fi
  pipx install pymobiledevice3 || die "pipx install pymobiledevice3 失败"
  export PATH="$HOME/.local/bin:$PATH"
  command -v pymobiledevice3 >/dev/null 2>&1 || die "安装后仍找不到 pymobiledevice3，请检查 PATH"
  ok "pymobiledevice3 安装完成"
}

# ============================================================================
# Step 2: 设备连接 + 配对
# ============================================================================
DEVICE_UDID=""
DEVICE_IOS_VERSION=""
DEVICE_MODEL=""

# 从 JSON stdin 里抽字段；JSON 解析失败返回空。
_json_field() {
  python3 -c '
import json,sys
try:
    data=json.load(sys.stdin)
except Exception:
    sys.exit(0)
for k in sys.argv[1:]:
    if isinstance(data,list):
        if not data: sys.exit(0)
        data=data[0]
    if isinstance(data,dict):
        data=data.get(k,"")
    else:
        sys.exit(0)
print(data if isinstance(data,(str,int,float)) else "")
' "$@" 2>/dev/null || true
}

ensure_device_connected() {
  step "[2/6] 检查 iOS 设备连接"
  local raw
  raw=$(PMD3 usbmux list 2>/dev/null || true)
  DEVICE_UDID=$(printf '%s' "$raw" | _json_field Identifier)
  [[ -z "$DEVICE_UDID" ]] && DEVICE_UDID=$(printf '%s' "$raw" | _json_field UniqueDeviceID)
  [[ -z "$DEVICE_UDID" ]] && DEVICE_UDID=$(printf '%s' "$raw" | _json_field Serial)
  if [[ -z "$DEVICE_UDID" ]]; then
    fail "未检测到 iOS 设备"
    warn "请确保: 1) 数据线连接 2) 解锁屏幕 3) 已点击「信任此电脑」"
    if $CHECK_ONLY; then return 1; fi
    local retry=0
    printf "  等待设备" >&2
    while (( retry < 30 )); do
      sleep 2
      raw=$(PMD3 usbmux list 2>/dev/null || true)
      DEVICE_UDID=$(printf '%s' "$raw" | _json_field Identifier)
      [[ -n "$DEVICE_UDID" ]] && break
      printf "." >&2
      retry=$((retry + 1))
    done
    echo "" >&2
    [[ -z "$DEVICE_UDID" ]] && die "等待设备连接超时（60s）"
  fi
  DEVICE_MODEL=$(printf '%s' "$raw" | _json_field ProductType)
  # ProductVersion 可能不在 usbmux list 里，补补一下 lockdown info
  local info
  info=$(PMD3 lockdown info 2>/dev/null || true)
  if [[ -n "$info" && "${info:0:1}" == "{" ]]; then
    DEVICE_IOS_VERSION=$(printf '%s' "$info" | _json_field ProductVersion)
    [[ -z "$DEVICE_MODEL" ]] && DEVICE_MODEL=$(printf '%s' "$info" | _json_field ProductType)
  fi
  ok "设备已连接: ${DEVICE_MODEL:-?} / iOS ${DEVICE_IOS_VERSION:-?}"
  ok "  UDID: $DEVICE_UDID"
}

# ============================================================================
# Step 3: 开发者模式（不能自动开）
# ============================================================================
ensure_developer_mode() {
  step "[3/6] 检查设备开发者模式"
  local status
  status=$(PMD3 amfi developer-mode-status 2>&1 || true)
  $VERBOSE && echo "    $status"
  if echo "$status" | grep -qiE '(true|enabled)'; then
    ok "开发者模式已启用"
    return 0
  fi
  fail "开发者模式未启用"
  $CHECK_ONLY && return 1
  cat >&2 <<EOF

  请在设备上手动操作（无法自动开启）:
    设置 → 隐私与安全 → 开发者模式 → 开启 → 重启设备
    重启后解锁，会弹窗确认开启开发者模式 → 选「打开」

  完成后脚本会自动继续，超时 5 分钟。
EOF
  local retry=0
  while (( retry < 60 )); do
    sleep 5
    status=$(PMD3 amfi developer-mode-status 2>&1 || true)
    if echo "$status" | grep -qiE '(true|enabled)'; then
      ok "开发者模式已启用"
      return 0
    fi
    retry=$((retry + 1))
    printf "."  >&2
  done
  echo "" >&2
  die "等待开发者模式开启超时"
}

# ============================================================================
# Step 4: DeveloperDiskImage
# ============================================================================
ensure_disk_image_mounted() {
  step "[4/6] 检查 DeveloperDiskImage 挂载状态"
  local mounted
  mounted=$(PMD3 mounter list 2>&1 || true)
  $VERBOSE && echo "    $mounted"
  # iOS 17+: 镜像名为 "Personalized"; 旧版可能是 "Developer"
  if echo "$mounted" | grep -qiE '(Personalized|Developer)'; then
    ok "DeveloperDiskImage 已挂载"
    return 0
  fi
  fail "DeveloperDiskImage 未挂载"
  $CHECK_ONLY && return 1

  step "  → 自动挂载中（可能需要下载镜像，首次较慢）..."
  if ! PMD3 mounter auto-mount 2>&1 | tee /tmp/.pmd_mount.log; then
    cat /tmp/.pmd_mount.log >&2 || true
    die "auto-mount 失败，参考日志：/tmp/.pmd_mount.log"
  fi
  ok "DeveloperDiskImage 挂载完成"
}

# ============================================================================
# Step 5: tunneld（必须 sudo 后台跑）
# ============================================================================
is_tunneld_running() {
  pgrep -f 'pymobiledevice3.*remote.*tunneld' >/dev/null 2>&1
}

ensure_tunneld_running() {
  step "[5/6] 检查 tunneld"
  if is_tunneld_running; then
    ok "tunneld 已在运行 (PID=$(pgrep -f 'pymobiledevice3.*remote.*tunneld' | head -1))"
    return 0
  fi
  fail "tunneld 未运行"
  $CHECK_ONLY && return 1

  step "  → 启动 tunneld（需 sudo）..."
  warn "  即将请求 sudo 密码，tunneld 会在后台持续运行（日志: $TUNNELD_LOG）"
  # 提前缓存 sudo 凭据，避免后台启动时找不到 tty
  if ! sudo -v; then
    die "sudo 授权失败，无法启动 tunneld"
  fi
  # 后台启动 + 完全脱离当前 shell
  : > "$TUNNELD_LOG"
  sudo nohup pymobiledevice3 remote tunneld </dev/null >>"$TUNNELD_LOG" 2>&1 &
  disown 2>/dev/null || true

  # 等就绪
  local retry=0
  while (( retry < 15 )); do
    sleep 1
    if is_tunneld_running; then
      ok "tunneld 已启动 (PID=$(pgrep -f 'pymobiledevice3.*remote.*tunneld' | head -1))"
      return 0
    fi
    retry=$((retry + 1))
  done
  die "tunneld 启动超时，参考日志: $TUNNELD_LOG"
}

# ============================================================================
# Step 6: 实测 screenshot
# ============================================================================
verify_screenshot() {
  step "[6/6] 实测 dvt screenshot"
  rm -f "$SCREENSHOT_TEST"
  local cmd=(pymobiledevice3 developer dvt screenshot "$SCREENSHOT_TEST")
  if [[ -n "$DEVICE_UDID" ]]; then
    cmd+=(--tunnel "$DEVICE_UDID")
  fi
  if "${cmd[@]}" </dev/null > /tmp/.pmd_ss.log 2>&1; then
    if [[ -s "$SCREENSHOT_TEST" ]]; then
      local size
      size=$(stat -f%z "$SCREENSHOT_TEST" 2>/dev/null || stat -c%s "$SCREENSHOT_TEST" 2>/dev/null || echo "?")
      ok "截图成功: $SCREENSHOT_TEST ($size bytes)"
      $VERBOSE && file "$SCREENSHOT_TEST" || true
      return 0
    fi
  fi
  fail "实测截图失败，错误日志:"
  sed 's/^/    /' /tmp/.pmd_ss.log >&2
  return 1
}

# ============================================================================
# main
# ============================================================================
main() {
  local mode_label="检测 + 自动修复"
  $CHECK_ONLY && mode_label="仅检测"
  _cyan "iOS 真机截图环境准备 ($mode_label)"
  echo ""

  # Step 1、Step 2 是后续所有检测的前提，任一失败则提前退出，避免后面一片误报。
  if ! ensure_pymobiledevice3; then
    $CHECK_ONLY && { _yellow "⚠️  环境未就绪，去掉 --check 让脚本自动修复"; exit 1; }
    _red "❌ pymobiledevice3 未安装，不能继续"; exit 1
  fi
  if ! ensure_device_connected; then
    $CHECK_ONLY && { _yellow "⚠️  设备未连接，去掉 --check 脚本会等待设备接入"; exit 1; }
    _red "❌ 设备未连接，不能继续"; exit 1
  fi

  local failed=0
  ensure_developer_mode        || failed=$((failed + 1))
  ensure_disk_image_mounted    || failed=$((failed + 1))
  ensure_tunneld_running       || failed=$((failed + 1))
  verify_screenshot            || failed=$((failed + 1))

  echo ""
  if (( failed == 0 )); then
    _green "✅ 全部就绪，可直接执行: ./test/atest.sh run -t amap -p ios -j <bundle.json>"
  elif $CHECK_ONLY; then
    _yellow "⚠️  $failed 项未通过；去掉 --check 让脚本自动修复"
    exit 1
  else
    _red "❌ 仍有 $failed 项未通过，请按上方提示处理"
    exit 1
  fi
}

main "$@"
