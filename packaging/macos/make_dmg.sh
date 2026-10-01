#!/usr/bin/env bash
# Builds the unsigned macOS disk image (DESIGN 11.6) from an existing Release build.
# Uses only tools that ship with macOS (hdiutil, codesign, tiffutil, osascript, ditto).
#
#   packaging/macos/make_dmg.sh                       # app and version from ./build
#   packaging/macos/make_dmg.sh --app path/VoiceBooth.app --version 0.1.0 --out build/installer
#
# Output: <out>/VoiceBooth-<version>-mac-universal.dmg
#   VoiceBooth.app + an /Applications link on the brand background (brand/out/installer/dmg-background*.png,
#   660x400, icons at VoiceBooth.app (165,200) / Applications (495,200)).
#
# The window layout is written by Finder (AppleScript). On a machine where Finder cannot be scripted
# (headless CI without the Automation permission, timeouts) the layout step is skipped with a warning and
# the DMG is still produced: same contents, default Finder window (no background picture / positions).
# Set VB_DMG_REQUIRE_LAYOUT=1 to make that a hard failure instead.
#
# Not notarized / not Developer-ID signed (DESIGN 19). The app is ad-hoc signed ("codesign -s -") so that
# Apple silicon accepts the bundle; Gatekeeper still asks the user to allow it once (see DESIGN 11.6).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
APP=""
VERSION=""
OUT_DIR=""
VOLNAME="VoiceBooth"

die() { echo "make_dmg: error: $*" >&2; exit 1; }
log() { echo "make_dmg: $*"; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --app)       APP="${2:?}"; shift 2 ;;
        --version)   VERSION="${2:?}"; shift 2 ;;
        --out)       OUT_DIR="${2:?}"; shift 2 ;;
        --build-dir) BUILD_DIR="${2:?}"; shift 2 ;;
        -h|--help)   sed -n '2,20p' "$0"; exit 0 ;;
        *)           die "unknown argument: $1" ;;
    esac
done

[[ "$(uname -s)" == "Darwin" ]] || die "run this on macOS (needs hdiutil)"

APP="${APP:-$BUILD_DIR/VoiceBooth_artefacts/Release/VoiceBooth.app}"
OUT_DIR="${OUT_DIR:-$BUILD_DIR/installer}"
if [[ -z "$VERSION" ]]; then
    CACHE="$BUILD_DIR/CMakeCache.txt"
    [[ -f "$CACHE" ]] || die "missing $CACHE (configure/build first, or pass --version)"
    VERSION="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$CACHE" | head -1)"
fi
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+(\.[0-9]+)?$ ]] || die "version must be numeric x.y.z (got '$VERSION')"

BG="$REPO_ROOT/brand/out/installer/dmg-background.png"
BG2X="$REPO_ROOT/brand/out/installer/dmg-background@2x.png"
[[ -d "$APP" ]] || die "missing app bundle: $APP (cmake --build build --config Release)"
[[ -x "$APP/Contents/MacOS/VoiceBooth" ]] || die "missing executable in $APP"
[[ -f "$BG" && -f "$BG2X" ]] || die "missing DMG background in brand/out/installer"

DMG_NAME="VoiceBooth-$VERSION-mac-universal.dmg"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
FINAL="$OUT_DIR/$DMG_NAME"

log "app:     $APP"
log "version: $VERSION"
log "archs:   $(lipo -archs "$APP/Contents/MacOS/VoiceBooth" 2>/dev/null || echo '?')"
lipo "$APP/Contents/MacOS/VoiceBooth" -verify_arch arm64 x86_64 \
    || log "warning: not a universal binary (arm64 + x86_64); the file name still says 'universal'"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/vb-dmg.XXXXXX")"
DEVICE=""
cleanup() {
    if [[ -n "$DEVICE" ]]; then hdiutil detach "$DEVICE" -force >/dev/null 2>&1 || true; fi
    rm -rf "$WORK"
}
trap cleanup EXIT

# --- 1. staging folder -------------------------------------------------------
STAGE="$WORK/stage"
mkdir -p "$STAGE/.background"
ditto "$APP" "$STAGE/VoiceBooth.app"
ln -s /Applications "$STAGE/Applications"
# 1x + 2x in one TIFF: Finder picks the sharp one on Retina screens
tiffutil -cathidpicheck "$BG" "$BG2X" -out "$STAGE/.background/background.tiff" >/dev/null 2>&1 \
    || cp "$BG" "$STAGE/.background/background.png"
BG_FILE="$(ls "$STAGE/.background")"

# Ad-hoc signature over the whole bundle (the linker only signs the binary; an unsealed bundle shows
# "is damaged" on Apple silicon). No hardened runtime: it would need entitlements for the microphone.
xattr -cr "$STAGE/VoiceBooth.app"
codesign --force --deep --sign - "$STAGE/VoiceBooth.app"
codesign --verify --deep --strict "$STAGE/VoiceBooth.app"
log "ad-hoc signed: $(codesign -dv "$STAGE/VoiceBooth.app" 2>&1 | grep -E '^Signature' || echo ok)"

# --- 2. writable image -------------------------------------------------------
APP_MB="$(du -sm "$STAGE" | cut -f1)"
SIZE_MB=$(( APP_MB + 20 ))
RW="$WORK/rw.dmg"
# hdiutil on CI runners sometimes fails with "Resource busy": retry a few times
retry() {
    local n=1
    until "$@"; do
        (( n >= 4 )) && return 1
        log "retry $n: $*" >&2; sleep $(( n * 5 )); n=$(( n + 1 ))
    done
}
retry hdiutil create -srcfolder "$STAGE" -volname "$VOLNAME" -fs HFS+ -format UDRW -size "${SIZE_MB}m" -ov "$RW" \
    || die "hdiutil create failed"

ATTACH_OUT="$(retry hdiutil attach "$RW" -readwrite -noverify -noautoopen)" || die "hdiutil attach failed"
DEVICE="$(echo "$ATTACH_OUT" | grep -E '^/dev/' | head -1 | awk '{print $1}')"
MOUNT="$(echo "$ATTACH_OUT" | grep -E '/Volumes/' | tail -1 | sed -E 's|^.*(/Volumes/.*)$|\1|')"
[[ -n "$DEVICE" && -d "$MOUNT" ]] || die "could not find mounted volume in: $ATTACH_OUT"
DISK_NAME="$(basename "$MOUNT")"
log "mounted $DEVICE at $MOUNT"

# --- 3. Finder layout (best effort) -----------------------------------------
# Window content 660x400 (+ title bar). Positions are icon centers, same as brand/README.
layout() {
    osascript <<EOF
tell application "Finder"
    with timeout of 120 seconds
        tell disk "$DISK_NAME"
            open
            set current view of container window to icon view
            set toolbar visible of container window to false
            set statusbar visible of container window to false
            set the bounds of container window to {200, 120, 860, 548}
            set viewOptions to the icon view options of container window
            set arrangement of viewOptions to not arranged
            set icon size of viewOptions to 128
            set text size of viewOptions to 13
            set background picture of viewOptions to file ".background:$BG_FILE"
            set position of item "VoiceBooth.app" of container window to {165, 200}
            set position of item "Applications" of container window to {495, 200}
            close
            open
            update without registering applications
            delay 2
            close
        end tell
    end timeout
end tell
EOF
}
if layout; then
    # Finder writes .DS_Store asynchronously
    for _ in 1 2 3 4 5 6 7 8 9 10; do [[ -f "$MOUNT/.DS_Store" ]] && break; sleep 1; done
    if [[ -f "$MOUNT/.DS_Store" ]]; then log "Finder layout written"; else log "warning: .DS_Store not written; layout may be default"; fi
else
    [[ "${VB_DMG_REQUIRE_LAYOUT:-0}" == "1" ]] && die "Finder layout failed (VB_DMG_REQUIRE_LAYOUT=1)"
    log "warning: Finder layout failed (no Automation permission / headless?). Producing a plain DMG."
    if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
        echo "::warning title=DMG layout::Finder layout failed; the DMG has no custom background / icon positions"
    fi
fi
rm -rf "$MOUNT/.fseventsd" "$MOUNT/.Trashes" 2>/dev/null || true
chmod -Rf go-w "$MOUNT" 2>/dev/null || true
sync

retry hdiutil detach "$DEVICE" || hdiutil detach "$DEVICE" -force || die "hdiutil detach failed"
DEVICE=""

# --- 4. compressed read-only image -----------------------------------------
rm -f "$FINAL"
retry hdiutil convert "$RW" -format UDZO -imagekey zlib-level=9 -o "$FINAL" || die "hdiutil convert failed"
hdiutil verify "$FINAL" >/dev/null || die "hdiutil verify failed: $FINAL"

log "built: $FINAL ($(du -h "$FINAL" | cut -f1), unsigned / not notarized)"
if [[ -n "${GITHUB_OUTPUT:-}" ]]; then echo "dmg=$FINAL" >> "$GITHUB_OUTPUT"; fi
