"""Package the opt-in X3 speed diagnostic without replacing release artifacts."""

import hashlib
import re
import shutil
import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821 -- PlatformIO


def package_firmware(source, target, env):
    del source, target
    version = None
    for define in env.get("CPPDEFINES", []):
        if isinstance(define, (tuple, list)) and len(define) == 2 and define[0] == "CROSSPOINT_VERSION":
            value = str(define[1]).replace(chr(92), "").replace('"', "")
            match = re.fullmatch(r"crossRSVP-v(\d+\.\d+\.\d+-speedtest\.\d+)-x3", value)
            if match:
                version = match.group(1)
    if env.subst("$PIOENV") != "rsvp_x3_speedtest" or version is None:
        raise RuntimeError("Expected the X3 speedtest profile and embedded version")
    project = Path(env.subst("$PROJECT_DIR"))
    output = project / "artifacts" / f"crossrsvp-x3-v{version}"
    output.mkdir(parents=True, exist_ok=True)
    image = output / f"crossrsvp-x3-v{version}.bin"
    shutil.copy2(Path(env.subst("$BUILD_DIR")) / "firmware.bin", image)
    digest = hashlib.sha256(image.read_bytes()).hexdigest()
    (output / "SHA256SUMS").write_text(f"{digest}  {image.name}\n", encoding="ascii")
    shutil.copy2(project / "docs/rsvp-x3-speedtest.md", output / "README.md")
    shutil.copy2(project / "scripts/analyze_rsvp_speed.py", output / "analyze_rsvp_speed.py")
    subprocess.run([sys.executable, str(project / "scripts/build_rsvp_speed_fixture.py"),
                    str(output / "rsvp-speed.epub"), "--language", "ru", "--words", "720"], check=True)
    print(f"Packaged X3 speedtest: {image}")
    print(f"SHA-256: {digest}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", package_firmware)  # noqa: F821
