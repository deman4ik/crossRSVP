#!/usr/bin/env python3

import pathlib
import sys
import zipfile


def main() -> None:
    source = pathlib.Path(sys.argv[1])
    output = pathlib.Path(sys.argv[2])
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_STORED) as archive:
        archive.write(source / "mimetype", "mimetype")
        for path in sorted(source.rglob("*")):
            if path.is_file() and path.name != "mimetype":
                archive.write(path, path.relative_to(source).as_posix())


if __name__ == "__main__":
    main()
