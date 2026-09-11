"""Package the opt-in X3 RSVP window A/B diagnostic."""

import hashlib
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path
from typing import Sequence

Import("env")  # noqa: F821 -- PlatformIO injects this symbol


def file_digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_deterministic_archive(archive: Path, files: Sequence[Path]) -> None:
    temporary = archive.with_suffix(".zip.tmp")
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as package:
        for path in sorted(files, key=lambda item: item.name):
            entry = zipfile.ZipInfo(path.name, date_time=(1980, 1, 1, 0, 0, 0))
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            entry.compress_type = zipfile.ZIP_DEFLATED
            package.writestr(entry, path.read_bytes(), compresslevel=9)
    temporary.replace(archive)


def package_firmware(source, target, env):
    del source, target
    version = None
    for define in env.get("CPPDEFINES", []):
        if isinstance(define, (tuple, list)) and len(define) == 2 and define[0] == "CROSSPOINT_VERSION":
            value = str(define[1]).replace(chr(92), "").replace('"', "")
            match = re.fullmatch(r"crossRSVP-v(\d+\.\d+\.\d+-windowtest\.\d+)-x3", value)
            if match:
                version = match.group(1)
    if env.subst("$PIOENV") != "rsvp_x3_window_test" or version is None:
        raise RuntimeError("Expected the X3 window-test profile and embedded windowtest version")

    project = Path(env.subst("$PROJECT_DIR"))
    output = project / "artifacts" / f"crossrsvp-x3-v{version}"
    output.mkdir(parents=True, exist_ok=True)
    image = output / f"crossrsvp-x3-v{version}.bin"
    shutil.copy2(Path(env.subst("$BUILD_DIR")) / "firmware.bin", image)
    readme = output / "README.md"
    fixture = output / "rsvp-window-test.epub"
    checksums = output / "SHA256SUMS"
    shutil.copy2(project / "docs/rsvp-x3-windowtest.md", readme)
    subprocess.run(
        [
            sys.executable,
            str(project / "scripts/build_rsvp_speed_fixture.py"),
            str(fixture),
            "--language",
            "en",
            "--words",
            "2400",
        ],
        check=True,
    )
    payload = [image, readme, fixture]
    checksums.write_text(
        "".join(f"{file_digest(path)}  {path.name}\n" for path in sorted(payload, key=lambda item: item.name)),
        encoding="ascii",
    )
    archive = output.parent / f"{output.name}.zip"
    write_deterministic_archive(archive, [*payload, checksums])
    print(f"Packaged X3 RSVP window test: {image}")
    print(f"Firmware SHA-256: {file_digest(image)}")
    print(f"Archive SHA-256: {file_digest(archive)}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", package_firmware)  # noqa: F821
