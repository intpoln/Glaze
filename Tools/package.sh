#!/bin/bash
# Builds a drop-in copy of the plugin: packages/<engine>/Glaze -> copy to <Project>/Plugins/Glaze.
# usage: package.sh [engine version=5.8]   (engine expected at D:\EGL\UE_<version>)
# The output must stay outside GlazeLab: GlazeLab loads every plugin under it and would see Glaze twice.
VER=${1:-5.8}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DIR="$ROOT/packages/$VER"
OUT="$DIR/Glaze"
mkdir -p "$DIR"
rm -rf "$OUT"
PLUGIN=$(cygpath -w "$ROOT/GlazeLab/Plugins/Glaze/Glaze.uplugin")
cmd //c "D:\EGL\UE_$VER\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin=$PLUGIN -Package=$(cygpath -w "$OUT") -TargetPlatforms=Win64 -Rocket" > "$DIR/package_log.txt" 2>&1
tail -3 "$DIR/package_log.txt"
rm -rf "$OUT/HostProject"
ls "$OUT" "$OUT/Binaries/Win64"
du -sh "$OUT"
