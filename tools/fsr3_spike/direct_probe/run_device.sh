#!/bin/sh
set -eu
SERIAL=${1:-}
ADB=${ADB:-adb}
[ -n "$SERIAL" ] && ADB="$ADB -s $SERIAL"
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../../.." && pwd)
$ADB push "$ROOT/build/android-arm64-release/libmobileglues.so" /data/local/tmp/libmobileglues.so
$ADB push "$HERE/direct_probe_arm64" /data/local/tmp/direct_probe
$ADB shell 'mkdir -p /data/local/tmp/mgcfg && printf "%s\n" "{\"fsr3Setting\":1}" > /data/local/tmp/mgcfg/config.json && chmod 755 /data/local/tmp/direct_probe'
$ADB shell 'cd /data/local/tmp && MG_DIR_PATH=/data/local/tmp/mgcfg ./direct_probe /data/local/tmp/libmobileglues.so'
