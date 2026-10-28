#!/bin/zsh
set -euo pipefail
cd "$(dirname "$0")"

APP_NAME="DrawingsGone"
BUILD="./build"
APP="$BUILD/$APP_NAME.app"

rm -rf "$BUILD"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

swiftc -O -swift-version 5 \
  -o "$APP/Contents/MacOS/$APP_NAME" \
  Sources/*.swift \
  -framework AppKit -framework Carbon -framework CoreGraphics -framework ServiceManagement -framework ScreenCaptureKit -framework AVFoundation -framework CoreMedia -framework QuartzCore

cp Resources/Info.plist "$APP/Contents/Info.plist"
if [[ -f Resources/AppIcon.icns ]]; then
  cp Resources/AppIcon.icns "$APP/Contents/Resources/AppIcon.icns"
fi

codesign --force --sign - "$APP" >/dev/null
echo "Built $APP"
