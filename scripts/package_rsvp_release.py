"""Package a CrossRSVP release image after a successful PlatformIO build."""

from __future__ import annotations

import hashlib
import re
import shutil
from pathlib import Path

Import("env")  # noqa: F821 -- provided by PlatformIO/SCons


_DEVICE_BY_ENV = {
    "rsvp_x3_release": "x3",
    "rsvp_x4_release": "x4",
    "rsvp_x4pro_release": "x4pro",
}


def _embedded_version(build_env, expected_device: str) -> str:
    """Read and validate the model/version embedded in the image build flags."""
    for define in build_env.get("CPPDEFINES", []):
        if isinstance(define, (tuple, list)) and len(define) == 2 and define[0] == "CROSSPOINT_VERSION":
            value = str(define[1])
        elif isinstance(define, str) and define.startswith("CROSSPOINT_VERSION="):
            value = define.split("=", 1)[1]
        else:
            continue
        value = value.strip('\\"')
        match = re.fullmatch(r"crossRSVP-v(\d+\.\d+\.\d+)-([a-z0-9]+)", value)
        if match and match.group(2) == expected_device:
            return match.group(1)
    raise RuntimeError(f"Embedded CROSSPOINT_VERSION does not match CrossRSVP {expected_device} SemVer")


def package_firmware(source, target, env) -> None:
    del source, target
    project_dir = Path(env.subst("$PROJECT_DIR"))
    environment = env.subst("$PIOENV")
    try:
        device = _DEVICE_BY_ENV[environment]
    except KeyError as exc:
        raise RuntimeError(f"Unsupported CrossRSVP packaging environment: {environment}") from exc

    version = _embedded_version(env, device)
    firmware = Path(env.subst("$BUILD_DIR")) / f"{env.subst('$PROGNAME')}.bin"
    output_dir = project_dir / "artifacts" / f"crossrsvp-{device}-v{version}" / "firmware"
    output_dir.mkdir(parents=True, exist_ok=True)
    packaged = output_dir / f"crossrsvp-{device}-v{version}.bin"
    shutil.copy2(firmware, packaged)
    digest = hashlib.sha256(packaged.read_bytes()).hexdigest()
    (output_dir / "SHA256SUMS").write_text(f"{digest}  {packaged.name}\n", encoding="ascii")
    print(f"Packaged CrossRSVP {device} v{version}: {packaged}")
    print(f"SHA-256: {digest}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", package_firmware)  # noqa: F821
