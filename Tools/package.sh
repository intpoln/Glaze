#!/bin/bash
# Builds a drop-in copy of the plugin: packages/<engine>/Glaze -> copy to <Project>/Plugins/Glaze.
# usage: package.sh [engine version=5.8]   (engine expected at D:\EGL\UE_<version>)
# The output must stay outside GlazeLab: GlazeLab loads every plugin under it and would see Glaze twice.
# The previous package is replaced only after a successful build.
set -u
VER=${1:-5.8}
[[ "$VER" =~ ^[0-9]+\.[0-9]+$ ]] || { echo "bad engine version: $VER (expected e.g. 5.8)"; exit 2; }
UAT="D:\EGL\UE_$VER\Engine\Build\BatchFiles\RunUAT.bat"
[ -f "$(cygpath -u "$UAT")" ] || { echo "engine not found: $UAT"; exit 2; }
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DIR="$ROOT/packages/$VER"
OUT="$DIR/Glaze"
TMP="$DIR/Glaze.new"
mkdir -p "$DIR"
rm -rf "$TMP"
PLUGIN=$(cygpath -w "$ROOT/GlazeLab/Plugins/Glaze/Glaze.uplugin")
PKG=$(cygpath -w "$TMP")
case "$PLUGIN$PKG" in *"'"*|*"&"*) echo "paths with ' or & are not supported"; exit 2;; esac
# PowerShell passes each quoted argument intact (cmd //c with escaped quotes breaks under Git Bash).
powershell.exe -NoProfile -Command "& '$UAT' BuildPlugin '-Plugin=$PLUGIN' '-Package=$PKG' -TargetPlatforms=Win64 -Rocket; exit \$LASTEXITCODE" > "$DIR/package_log.txt" 2>&1
CODE=$?
tail -3 "$DIR/package_log.txt"
if [ $CODE -ne 0 ] || ! grep -q "BUILD SUCCESSFUL" "$DIR/package_log.txt" || [ ! -f "$TMP/Glaze.uplugin" ]; then
	echo "package FAILED (exit $CODE), previous package kept: $OUT"
	rm -rf "$TMP"
	exit 1
fi
rm -rf "$TMP/HostProject" || { echo "cannot remove $TMP/HostProject"; exit 1; }
# Swap: keep the old package until the new one is in place, restore it on failure.
OLD="$DIR/Glaze.old"
rm -rf "$OLD" || { echo "cannot remove $OLD"; exit 1; }
if [ -d "$OUT" ]; then
	mv -T "$OUT" "$OLD" || { echo "cannot move the previous package aside: $OUT"; exit 1; }
fi
if ! mv -T "$TMP" "$OUT"; then
	echo "cannot move the new package into place"
	[ -d "$OLD" ] && mv -T "$OLD" "$OUT" && echo "previous package restored"
	exit 1
fi
rm -rf "$OLD" || echo "warning: could not remove $OLD"
ls "$OUT" "$OUT/Binaries/Win64" || exit 1
du -sh "$OUT"
