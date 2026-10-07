#!/bin/bash
# Build GlazeLabEditor (with the Glaze plugin). Prints errors and "Result:"; exit code 0 only on success.
OUT=$(cmd //c "D:\EGL\UE_5.8\Engine\Build\BatchFiles\Build.bat GlazeLabEditor Win64 Development -Project=C:\Users\intpo\Desktop\Glaze\GlazeLab\GlazeLab.uproject -WaitMutex -NoHotReloadFromIDE" 2>&1)
echo "$OUT" | grep -E "error|warning C|Result:" | head -60
echo "$OUT" | grep -q "Result: Succeeded"
