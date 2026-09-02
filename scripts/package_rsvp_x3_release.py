"""Package the experimental X3 qualification image after a successful build."""

from __future__ import annotations

import hashlib
import shutil
from pathlib import Path

Import("env")  # noqa: F821 -- provided by PlatformIO/SCons


def package_firmware(source, target, env) -> None:
    del source, target
    project_dir = Path(env.subst("$PROJECT_DIR"))
    firmware = Path(env.subst("$BUILD_DIR")) / f"{env.subst('$PROGNAME')}.bin"
    output_dir = project_dir / "artifacts" / "rsvp-x3-v0.1" / "firmware"
    output_dir.mkdir(parents=True, exist_ok=True)
    packaged = output_dir / "crossrsvp-x3-v0.1-experimental.bin"
    shutil.copy2(firmware, packaged)
    digest = hashlib.sha256(packaged.read_bytes()).hexdigest()
    (output_dir / "SHA256SUMS").write_text(f"{digest}  {packaged.name}\n", encoding="ascii")
    print(f"Packaged RSVP X3 candidate: {packaged}")
    print(f"SHA-256: {digest}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", package_firmware)  # noqa: F821
