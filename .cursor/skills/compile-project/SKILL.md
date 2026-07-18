---
name: compile-project
description: Compile this PlatformIO/ESP-IDF project by running start.ps1 in PowerShell, then fix build errors and recompile in a loop until the build succeeds. Use when the user asks to compile, build, or when they invoke this skill explicitly.
disable-model-invocation: true
---

# Compile Project

Compile the project and keep fixing it until the build succeeds.

## Workflow

Repeat this loop until the build finishes with exit code 0:

1. Run the build script from the project root:

```powershell
powershell -ExecutionPolicy Bypass -File .\start.ps1
```

`start.ps1` sets the working directory to the project root and runs `pio run` (PlatformIO / ESP-IDF, board `esp32dev`).

2. Read the full output. If the build **succeeds**, report success and stop.

3. If the build **fails**:
   - Identify the root cause from the compiler/linker errors (file, line, and message).
   - Fix the actual source of the error in the code or configuration. Do not suppress or hide errors, and do not comment out failing code just to make it pass.
   - Re-run the build from step 1.

4. Never end your turn while the build is still failing. Keep iterating until it is green.

## Rules

- Always build via `start.ps1`; do not invent alternative build commands.
- Fix errors at their source, then re-verify by rebuilding.
- If the same error persists after a fix attempt, change approach rather than repeating the identical edit.
- If you are truly blocked (e.g. missing hardware, external dependency, or ambiguous requirement that only the user can resolve), stop and explain clearly what is blocking the build.
