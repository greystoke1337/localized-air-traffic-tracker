---
name: deploy-golf
description: Deploy Golf CircuitPython code to the Adafruit Matrix Portal M4. Use when the user says "deploy golf", "deploy it" after editing Golf code, "push to Golf", "copy to CIRCUITPY", or after making changes to tracker_golf/code.py.
---

# Deploy Golf (Adafruit Matrix Portal M4)

Copy code.py to the CIRCUITPY drive. The device auto-reloads on file save — no compilation or restart needed.

## Steps

### 1. Find the CIRCUITPY drive

```python
import subprocess
result = subprocess.run(['wmic', 'logicaldisk', 'get', 'DeviceID,VolumeName'], capture_output=True, text=True)
print(result.stdout)
```

Look for the drive with `VolumeName = CIRCUITPY`. If not found, tell the user to connect the device and try again.

### 2. Syntax check

```bash
python -c "import py_compile; py_compile.compile('tracker_golf/code.py', doraise=True)"
```

Run from `c:/Users/maxim/localized-air-traffic-tracker`. If this fails, stop and report the error. Do NOT deploy broken code.

### 3. Copy using shutil.copy2 (NEVER use cp — corrupts FAT32)

```python
import shutil
shutil.copy2('c:/Users/maxim/localized-air-traffic-tracker/tracker_golf/code.py', 'X:/code.py')
```

Replace `X:` with the CIRCUITPY drive letter found in step 1.

### 4. Report summary

Tell the user:
- The CIRCUITPY drive letter found
- Whether the syntax check passed
- Whether the copy succeeded
- That the device auto-reloads — no further action needed
