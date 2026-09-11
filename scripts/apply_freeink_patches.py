#!/usr/bin/env python3
"""Apply CrossPoint's pinned FreeInk SDK integration patches idempotently."""

import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821 -- PlatformIO

project_dir = Path(env.subst("$PROJECT_DIR"))  # noqa: F821
sdk_dir = project_dir / "freeink-sdk"
patch_dir = project_dir / "scripts" / "freeink_patches"
PINNED_SDK_REVISION = "fde240faaeae6c340dacd435a4f77d2ef2f82dfd"


def git_apply_succeeds(patch: Path, reverse: bool = False) -> bool:
    command = ["git", "apply", "--check"]
    if reverse:
        command.append("--reverse")
    command.append(str(patch))
    return subprocess.run(command, cwd=sdk_dir, capture_output=True, text=True).returncode == 0


def patched_blobs_match(patch: Path, post_patch: bool) -> bool:
    """Return true when every path has the exact pre- or post-patch content."""
    expected_blobs = {}
    current_path = None
    for line in patch.read_text(encoding="utf-8").splitlines():
        if line.startswith("diff --git a/"):
            current_path = line.split(" b/", 1)[1]
        elif current_path is not None and line.startswith("index "):
            before_blob, after_blob = line.split()[1].split("..", 1)
            expected_blobs[current_path] = after_blob if post_patch else before_blob

    if not expected_blobs:
        return False

    for relative_path, expected_blob in expected_blobs.items():
        path = sdk_dir / relative_path
        if set(expected_blob) == {"0"}:
            if path.exists():
                return False
            continue
        if not path.is_file():
            return False
        actual_blob = subprocess.run(
            ["git", "hash-object", str(path)],
            cwd=sdk_dir,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
        if actual_blob != expected_blob:
            return False
    return True


def apply_patch(patch: Path) -> None:
    if git_apply_succeeds(patch, reverse=True):
        if patched_blobs_match(patch, post_patch=True):
            return
        sys.stderr.write(
            f"ERROR: FreeInk patch {patch.name} appears applied, but a patched file has diverged; "
            "preserving the current SDK worktree unchanged.\n"
        )
        raise SystemExit(1)
    if not patched_blobs_match(patch, post_patch=False):
        sys.stderr.write(
            f"ERROR: FreeInk patch {patch.name} source files have diverged; "
            "preserving the current SDK worktree unchanged.\n"
        )
        raise SystemExit(1)
    if not git_apply_succeeds(patch):
        result = subprocess.run(
            ["git", "apply", "--check", str(patch)], cwd=sdk_dir, capture_output=True, text=True
        )
        sys.stderr.write(
            f"ERROR: FreeInk patch {patch.name} does not apply cleanly; "
            "preserving the current SDK worktree unchanged:\n"
            f"{result.stdout}{result.stderr}\n"
        )
        raise SystemExit(1)
    subprocess.run(["git", "apply", str(patch)], cwd=sdk_dir, check=True)
    print(f"Applied FreeInk patch: {patch.name}")


if not (sdk_dir / ".git").exists():
    raise RuntimeError(f"FreeInk SDK checkout missing at {sdk_dir}")
if not patch_dir.is_dir():
    raise RuntimeError(f"FreeInk patch directory missing at {patch_dir}")

sdk_revision = subprocess.run(
    ["git", "rev-parse", "HEAD"], cwd=sdk_dir, capture_output=True, text=True, check=True
).stdout.strip()
if sdk_revision != PINNED_SDK_REVISION:
    raise RuntimeError(
        "FreeInk SDK revision mismatch: "
        f"expected {PINNED_SDK_REVISION}, found {sdk_revision}; preserving the SDK worktree unchanged"
    )

patches = sorted(patch_dir.glob("*.patch"))
if not patches:
    raise RuntimeError(f"FreeInk patches missing under {patch_dir}")
for patch_file in patches:
    apply_patch(patch_file)
