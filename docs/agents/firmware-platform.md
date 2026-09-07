# Platform

Paths and shell commands in code blocks are relative to the repository root unless stated otherwise.

## Development Environment Awareness

Use the known host platform to choose tools and commands. If the host is unknown or has changed, determine it before using platform-specific commands.

### Platform Detection

```bash
# Detect platform when the host is unknown
uname -s
# Returns: MINGW64_NT-* (Windows Git Bash), Linux, Darwin (macOS)
```

Run `uname -s` when reliable host information is not already available.

### Platform-Specific Behaviors

- **Windows (Git Bash)**: Unix commands, `C:\` paths in Windows but `/` in bash, limited glob (use `find`+`xargs`)
- **Linux/WSL**: Full bash, Unix paths, native glob support

**Cross-Platform Code Formatting**:

```bash
./bin/clang-format-fix -g
```

Never invoke or probe `clang-format` directly. The repository wrapper is the only sanctioned entry point.

---
