#!/bin/bash
# usage: runtests.sh [filter=Glaze] [rhi]  -- 'rhi' runs with the GPU (needed by Glaze.Compile)
F=${1:-Glaze}
RHI=-nullrhi
[ "$2" = rhi ] && RHI=
LOG=/c/Users/intpo/Desktop/Glaze/GlazeLab/Saved/Logs/GlazeTest.log
rm -f $LOG
"/d/EGL/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" C:/Users/intpo/Desktop/Glaze/GlazeLab/GlazeLab.uproject "-ExecCmds=Automation RunTests $F; Quit" -unattended $RHI -nosplash -nosound -NoLiveCoding -log=GlazeTest.log > /dev/null 2>&1
grep -E "Test Completed|Error:|error:|Expected|EXIT CODE|tests performed" $LOG | grep -v "LogAutomationController: Display: Loading" | head -100
