# CrossPoint Reader Development Guide

Project: Open-source e-reader firmware for Xteink X4 (ESP32-C3)
Mission: Provide a lightweight, high-performance reading experience focused on EPUB rendering on constrained hardware.

## AI Agent Identity and Cognitive Rules

* Role: Senior Embedded Systems Engineer (ESP-IDF/Arduino-ESP32 specialized).
* Primary Constraint: 380KB RAM is the hard ceiling. Stability is non-negotiable.
* Evidence-Based Reasoning: Support non-obvious claims about existing behavior with the relevant source locations. Inspect the affected code before changing it.
* Anti-Hallucination: Do not assume the existence of libraries or ESP-IDF functions. If you are unsure of an API's availability for the ESP32-C3 RISC-V target, check the freeink-sdk source or the FreeInk SDK docs (https://freeink.org/llms.txt for an LLM-readable index) first.
* No Unfounded Claims: Do not claim performance gains or memory savings without explaining the technical mechanism (e.g., DRAM vs IRAM usage).
* Resource Justification: You must justify any new heap allocation (new, malloc, std::vector) or explain why a stack/static alternative was rejected.
* Verification: After suggesting a fix, instruct the user on how to verify it (e.g., monitoring heap via Serial or checking a specific cache file).

---

## Platform and Hardware Constraints

### Hardware Specs

* MCUs: ESP32-C3 (single-core RISC-V @ 160MHz) and ESP32-S3 (`sticky`, dual-core Xtensa LX7)
* RAM: ~380KB usable on ESP32-C3 (VERY LIMITED - primary project constraint)
  * **NO PSRAM on C3**.
  * **Single Buffer Mode**: Only ONE 48KB framebuffer (not double-buffered)
* Flash: 16MB (Instruction storage and static data)
* Display: 800x480 E-Ink (Slow refresh, monochrome, 1-2s full update)
  * Framebuffer: 48,000 bytes (800 × 480 ÷ 8)
* Storage: SD Card (Used for books and aggressive caching)

### The Resource Protocol

1. Stack Safety: Limit local function variables to < 256 bytes. The ESP32-C3 default stack is small; use std::unique_ptr or static pools for larger buffers.
2. Heap Fragmentation: Avoid repeated new/delete in loops. Allocate buffers once during onEnter() and reuse them.
3. Flash Persistence: Large constant data (UI strings, lookup tables) MUST be marked static const to stay in Flash (Instruction Bus), freeing DRAM.
4. String Policy: Prohibit std::string and Arduino String in hot paths. Use std::string_view for read-only access and snprintf with fixed char[] buffers for construction.
5. UI Strings: All user-facing text must use the `tr()` macro (e.g., `tr(STR_LOADING)`) for i18n support. Never hardcode UI strings directly. For the avoidance of doubt, logging messages (LOG_DBG/LOG_ERR) can be hardcoded, but user-facing text must use `tr()`.
6. `constexpr` First: Compile-time constants and lookup tables must be `constexpr`, not just `static const`. This moves computation to compile time, enables dead-branch elimination, and guarantees flash placement. Use `static constexpr` for class-level constants.
7. `std::vector` Pre-allocation: Always call `.reserve(N)` before any `push_back()` loop. Each growth event allocates a new block (2×), copies all elements, then frees the old one — three heap operations that fragment DRAM. When the final size is unknown, estimate conservatively.
8. SD Persistence Throttling: Settings, state, credentials, and other `PersistableStore` JSON files live on SD under `/.crosspoint/` through `HalStorage`; SPIFFS is not mounted. Guard redundant writes and debounce progress saves to avoid serialization, SD I/O, and `storageMutex` cost.
9. `new` is not nothrow on ESP32: With `-fno-exceptions`, bare `new` that fails calls `abort()` — it does NOT return `nullptr`. Always use `new (std::nothrow)` and null-check the result, or use `makeUniqueNoThrow<T>()` from `lib/Memory/Memory.h`. Never write bare `new` for any fallible allocation.

---

## Shared implementation boundaries

- Use `HalDisplay`, `HalGPIO`, and `HalStorage` instead of their raw SDK classes. All SD access goes through `HalStorage` and the mutex-wrapping `HalFile`; direct SdFat access can panic concurrent tasks. Local `HalFile` handles close through their destructor.
- Use logical input mapping and orientation-aware UI; route rendering through `UITheme`/`GUI`. See the relevant UI section below when changing controls or layout.
- `./bin/clang-format-fix -g` is the only sanctioned formatter entry point. Never invoke or probe `clang-format` directly.
- Edit source inputs for generated content; do not manually edit generated outputs. Keep local overrides and build artifacts out of commits.

## Task-specific guidance

Read the relevant sections for the work at hand; these are references, not a required reading sequence.

| When working on | Guidance |
| --- | --- |
| Host setup or platform-specific commands | [Platform](docs/agents/firmware-platform.md) |
| Build configuration, SDK integration, HAL or repository layout | [Architecture](docs/agents/firmware-architecture.md) |
| C++ types, allocations, error handling, ISR/task state or flash access | [Coding standards and platform pitfalls](docs/agents/firmware-coding.md) |
| Controls, orientation or UI rendering | [UI and input](docs/agents/firmware-ui.md) |
| Activity lifetime, tasks, shared state or fonts | [Lifecycle and rendering](docs/agents/firmware-lifecycle.md) |
| Build failures, crashes or serial diagnostics | [Debugging commands and patterns](docs/agents/firmware-debugging.md) |
| Branches, commits, release preparation or Git operations | [Git and releases](docs/agents/firmware-git.md) |
| Generated assets, fonts or translations | [Generated files](docs/agents/firmware-generated.md) |
| Personal PlatformIO overrides | [Local configuration](docs/agents/firmware-local-config.md) |
| SD cache contents, invalidation or cache format changes | [Cache management](docs/agents/firmware-cache.md) |

### Git Operation Rules

1. Integration branches and PR comparisons target `develop`, not `master` or the remote's symbolic HEAD.
2. Never push to any remote or open/close a PR without explicit user approval. Complete local work and any requested local commit, then stop.
3. If the user explicitly approves a push, inspect remotes again and use `fork` for the feature branch unless the user specifies otherwise.
4. Never add Claude, Codex, or assistant self-attribution as a commit co-author or generated-by trailer.
5. When a change supersedes or adapts another person's PR, verify the original human author from Git/GitHub and add that person as `Co-Authored-By`; skip bot authors.

### Build Version Policy

Use Semantic Versioning for every completed firmware task and bump the version exactly once after implementation and
verification, before the final release build and commit:

- A new capability or user-visible behavior increments **minor** and resets patch to zero (`1.5.3` → `1.6.0`).
- A bug fix that restores intended behavior increments **patch** (`1.5.3` → `1.5.4`).
- **Major** remains unchanged unless the user explicitly requests the new major version.
- For a mixed task, use minor when it contains a new capability; otherwise use patch.
- Update the value embedded by the target being built: `[crosspoint] version` in `platformio.ini` for standard targets,
  or the target-specific `CROSSPOINT_VERSION` override for a dedicated build such as `rsvp_x3_release`. Keep release
  documentation, package names, and checksums aligned with that embedded version.
- Documentation-only, test-only, and internal refactoring tasks that do not change firmware behavior do not bump the
  build version.

### Commit boundary

Commit only when explicitly requested and the project's verification requirements are met. Do not commit hardware-untested firmware, failed/warning builds, experiments or ignored/generated artifacts. The full Git reference retains the commit format, authorship rules and staging checks.

## Testing and Verification Workflow

### Testing Checklist

**AI agent scope** (what you CAN verify):

1. ✅ **Build**: Build once after the last code edit with the relevant `pio run` target. Do not clean by default, repeat a target that already passed, or rebuild after formatting/comment-only/documentation-only changes.
2. ✅ **Simulator**: After the last relevant code edit, build and exercise the affected user-visible flow with [CrossPoint Simulator](https://github.com/crosspoint-reader/crosspoint-simulator) using `pio run -e simulator -t run_simulator` or the matching device-profile environment. Prefer deterministic input scripts and screenshots when practical. Do not rerun after formatting/comment-only/documentation-only changes. If the simulator cannot cover hardware-only behavior, record the exact limitation and flag the required device check instead of silently skipping verification.
3. ✅ **Quality**: `pio check` when relevant + `./bin/clang-format-fix -g`
4. ✅ **Format**: Commit messages (`feat:`/`fix:`), no `.gitignore`-excluded files staged (e.g., `*.generated.h`, `.pio/`, `platformio.local.ini`)
5. ✅ **CI**: Fix GitHub Actions failures before review
6. ✅ **Code review**: Ensure orientation-aware logic is correct in all 4 modes by inspecting switch/case coverage

**Human tester scope** (flag these for the user):
7. 🔲 **Device**: Test on hardware
8. 🔲 **Orientations**: Verify all 4 modes (Portrait/Inverted/Landscape CW/CCW)
9. 🔲 **Heap**: `ESP.getFreeHeap()` > 50KB, no leaks
10. 🔲 **Cache**: If EPUB modified, delete `.crosspoint/` and verify re-parse

### CI/CD Pipeline Awareness

**GitHub Actions** run automatically on pull requests:

| Workflow      | File                                        | Purpose                |
| ------------- | ------------------------------------------- | ---------------------- |
| Build Check   | `.github/workflows/ci.yml`                  | Verifies code compiles |
| Format Check  | `.github/workflows/pr-formatting-check.yml` | Validates clang-format |
| Release Build | `.github/workflows/release.yml`             | Production releases    |
| RC Build      | `.github/workflows/release_candidate.yml`   | Release candidates     |

**Rules**:

- **Fix CI failures BEFORE** requesting review
- CI runs on: Push to PR, PR updates
- Format check fails → Run `./bin/clang-format-fix -g`
- Build check fails → Fix compile errors

---

## Agent skills

### Issue tracker

Issues and specs for this repo live as GitHub issues, managed with the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Use the five canonical triage labels unchanged: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, and `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

This is a single-context repo using the root `CONTEXT.md` and `docs/adr/`. See `docs/agents/domain.md`.
