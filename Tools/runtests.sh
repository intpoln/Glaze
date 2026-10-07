#!/bin/bash
# usage: runtests.sh [filter=Glaze]
F=${1:-Glaze}
LOG=/c/Users/intpo/Desktop/Glaze/GlazeLab/Saved/Logs/GlazeTest.log
rm -f $LOG
"/d/EGL/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" C:/Users/intpo/Desktop/Glaze/GlazeLab/GlazeLab.uproject "-ExecCmds=Automation RunTests $F; Quit" -unattended -nullrhi -nosplash -nosound -NoLiveCoding -log=GlazeTest.log > /dev/null 2>&1
grep -E "Test Completed|Error:|error:|Expected|EXIT CODE|tests performed" $LOG | grep -v "LogAutomationController: Display: Loading" | head -100
