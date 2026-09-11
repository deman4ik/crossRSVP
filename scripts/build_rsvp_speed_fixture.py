#!/usr/bin/env python3
"""Build a small, punctuation-free EPUB for RSVP speed measurements."""

import argparse
import zipfile
from pathlib import Path

ENGLISH = "home tree road book lake river light stone water field house green".split()
RUSSIAN = "дом лес река поле небо день свет книга мост вода трава слово".split()


def write_entry(archive, name, contents):
    entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
    entry.create_system = 3
    entry.external_attr = 0o100644 << 16
    entry.compress_type = zipfile.ZIP_STORED
    archive.writestr(entry, contents)


def build(output, language="both", words=720):
    vocab = []
    if language in ("en", "both"):
        vocab.extend(ENGLISH)
    if language in ("ru", "both"):
        vocab.extend(RUSSIAN)
    body_words = (vocab * ((words // len(vocab)) + 1))[:words]
    language_tag = "ru" if language == "ru" else "en"
    xhtml = """<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml"><head><title>RSVP Speed Fixture</title>
<style>body {{ margin: 0; padding: 0; }} p {{ margin: 0; padding: 0; }}</style></head><body><p>{}</p>
</body></html>""".format(" ".join(body_words))
    opf = """<?xml version="1.0" encoding="utf-8"?><package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="bookid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="bookid">rsvp-speed-fixture</dc:identifier><dc:title>RSVP Speed Fixture</dc:title><dc:language>{}</dc:language></metadata><manifest><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="chapter"/></spine></package>""".format(language_tag)
    container = "<?xml version=\"1.0\"?><container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\"><rootfiles><rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles></container>"
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_STORED) as archive:
        write_entry(archive, "mimetype", "application/epub+zip")
        write_entry(archive, "META-INF/container.xml", container)
        write_entry(archive, "OEBPS/content.opf", opf)
        write_entry(archive, "OEBPS/chapter.xhtml", xhtml)
        write_entry(archive, "instruction.txt", "Open the book and start RSVP speed mode")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--language", choices=("en", "ru", "both"), default="both")
    parser.add_argument("--words", type=int, default=720)
    args = parser.parse_args(argv)
    if args.words < 600:
        parser.error("--words must be at least 600")
    build(args.output, args.language, args.words)


if __name__ == "__main__":
    main()
