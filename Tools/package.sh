#!/bin/bash
# Builds a drop-in copy of the plugin for any UE 5.8 project: copy the result folder to <Project>/Plugins/Glaze.
# The output must stay outside Desktop/Glaze/GlazeLab: GlazeLab loads every plugin under it and would see Glaze twice.
OUT=${1:-'C:\Users\intpo\Desktop\GlazePackage\Glaze'}
mkdir -p "$(dirname "$(cygpath "$OUT")")"
PLUGIN=$(cd "$(dirname "$0")/.." && pwd -W)/GlazeLab/Plugins/Glaze/Glaze.uplugin
cmd //c "D:\EGL\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin=${PLUGIN//\//\\} -Package=$OUT -TargetPlatforms=Win64 -Rocket" > "$(dirname "$(cygpath "$OUT")")/package_log.txt" 2>&1
tail -3 "$(dirname "$(cygpath "$OUT")")/package_log.txt"
rm -rf "$(cygpath "$OUT")/HostProject"
ls "$(cygpath "$OUT")" "$(cygpath "$OUT")/Binaries/Win64"
du -sh "$(cygpath "$OUT")"
