#!/bin/bash
# usage: runtests.sh [filter=Glaze] [rhi]  -- 'rhi' runs with the GPU (needed by Glaze.Compile)
# Exit code 0 only when every test passed.
F=${1:-Glaze}
RHI=-nullrhi
[ "${2:-}" = rhi ] && RHI=
ROOT=$(cd "$(dirname "$0")/.." && pwd)
LOG="$ROOT/GlazeLab/Saved/Logs/GlazeTest.log"
rm -f "$LOG"
"/d/EGL/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$(cygpath -w "$ROOT/GlazeLab/GlazeLab.uproject")" "-ExecCmds=Automation RunTests $F; Quit" -unattended $RHI -nosplash -nosound -NoLiveCoding -log=GlazeTest.log > /dev/null 2>&1
CODE=$?
[ -f "$LOG" ] || { echo "no log: $LOG (editor exit $CODE)"; exit 1; }
grep -E "Test Completed|Error:|error:|Expected|EXIT CODE|tests performed" "$LOG" | grep -v "LogAutomationController: Display: Loading" | head -100
grep -q "Test Completed" "$LOG" || { echo "no tests ran for filter '$F'"; exit 1; }
if grep -q "Test Completed. Result={Fail" "$LOG"; then exit 1; fi
# The editor must reach its final marker (a crash after some tests leaves none); its own exit code is not reliable here.
grep -q "TEST COMPLETE" "$LOG" || { echo "no TEST COMPLETE marker: the editor crashed or stopped early (editor exit $CODE)"; exit 1; }
grep "TEST COMPLETE" "$LOG" | grep -q "EXIT CODE: 0" || { echo "tests finished with a non-zero exit code in the log"; exit 1; }
exit 0
