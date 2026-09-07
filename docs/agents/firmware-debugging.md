# Debugging

Paths and shell commands in code blocks are relative to the repository root unless stated otherwise.

## Testing and Debugging

### Build Commands

**Via CLI**:

```bash
# Build firmware (default environment)
pio run

# Build and upload to device
pio run -t upload

# Build specific environment
pio run -e gh_release

# Clean build artifacts
pio run -t clean
```

**Via VS Code**:

* Use PlatformIO toolbar: Build (✓), Upload (→), Clean (🗑️)
* Or Command Palette: `PlatformIO: Build`, `PlatformIO: Upload`, etc.

### Monitoring and Debugging

```bash
# Enhanced monitor with color/logging (recommended)
python3 scripts/debugging_monitor.py

# Standard PlatformIO monitor
pio device monitor
```

**Via VS Code**: Click Monitor (🔌) button in PlatformIO toolbar

### Code Quality

```bash
# Static analysis (cppcheck)
pio check

# Format only Git-modified C/C++ files, on every host
./bin/clang-format-fix -g
```

Do not run raw `clang-format` or probe it with `command -v`; use the wrapper even for diagnostics.

### Debugging Crashes

**Common Crash Causes**:

1. **Out of Memory** (Most common):

   ```cpp
   LOG_DBG("MEM", "Free heap: %d bytes", ESP.getFreeHeap());
   ```

   - Monitor heap usage throughout activity lifecycle

   - Check if large allocations (>10KB) occur before crash

   - Verify buffers are freed in `onExit()`

2. **Stack Overflow**:

   ```cpp
   LOG_DBG("TASK", "Stack high water: %d", uxTaskGetStackHighWaterMark(taskHandle));
   ```

   - Occurs during deep recursion or large local variables

   - Increase task stack size in `xTaskCreate()` (2048 → 4096)

   - Move large buffers to heap with malloc

3. **Use-After-Free**:

   - Activity deleted but task still running

   - Always `vTaskDelete()` in `onExit()` BEFORE activity destruction

   - Set pointers to `nullptr` after `free()`

4. **Corrupt Cache Files**:

   - Delete `.crosspoint/` directory on SD card

   - Forces clean re-parse of all EPUBs

   - Check file format versions in [docs/file-formats.md](../../docs/file-formats.md)

5. **Watchdog Timeout**:

   - Loop/task blocked for >5 seconds

   - Add `vTaskDelay(1)` in tight loops

   - Check for blocking I/O operations

**Verification Steps**:

1. Check serial output for stack traces
2. Monitor heap with `ESP.getFreeHeap()` before/after operations
3. Verify task deletion with task list (`vTaskList()`)
4. Test with `LOG_LEVEL=2` (debug logging enabled)

---

## Serial Monitoring and Live Debugging

### Serial Monitor Options

1. **Enhanced**: `python3 scripts/debugging_monitor.py` (color-coded, recommended)
2. **Standard**: `pio device monitor` (basic, no colors)
3. **VS Code**: Monitor (🔌) button (IDE-integrated)

### Live Debugging Patterns

**Heap**: `LOG_DBG("MEM", "Free: %d", ESP.getFreeHeap());` (every 5s in loop)
**Stack**: `uxTaskGetStackHighWaterMark(nullptr)` (< 512 bytes → increase stack)
**Flush**: `logSerial.flush();` (force output before crash)

**Port Detection**: Windows: `mode` | Linux: `ls /dev/ttyUSB* /dev/ttyACM*` or `dmesg | grep tty`

---
