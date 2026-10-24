"""Writes a tiny fb2 book archive and its INPX index for the OPDS server tests.

Usage: python3 make_fixture.py <archive folder>
"""

import sys
import zipfile
from pathlib import Path

# (file name, author last name, author first name, title, genre, series, series number)
BOOKS = [
    ("1.fb2", "Petrov", "Ivan", "First Test Book", "sf", "Test Saga", 1),
    ("2.fb2", "Petrov", "Ivan", "Second Test Book", "sf", "Test Saga", 2),
    ("3.fb2", "Smith", "Anna", "Detective Story", "det_classic", None, None),
]

TEMPLATE = """<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">
<description>
<title-info>
<genre>{genre}</genre>
<author><first-name>{first}</first-name><last-name>{last}</last-name></author>
<book-title>{title}</book-title>
<lang>en</lang>
{sequence}
</title-info>
<document-info><author><nickname>test</nickname></author><date>2026-01-01</date><id>{file}</id><version>1.0</version></document-info>
</description>
<body><section><p>Text of {title}.</p></section></body>
</FictionBook>
"""

ARCHIVE = "fb2-000001-000003"

# INPX line: fields in the parser's default order (AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;LIBRATE;KEYWORDS),
# separated by 0x04; authors and genres are ':'-terminated lists, author names are "last,first,middle"
FIELDS_SEPARATOR = "\x04"


def main() -> None:
    folder = Path(sys.argv[1])
    folder.mkdir(parents=True, exist_ok=True)

    lines = []
    with zipfile.ZipFile(folder / f"{ARCHIVE}.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        for lib_id, (file, last, first, title, genre, series, number) in enumerate(BOOKS, start=1):
            sequence = f'<sequence name="{series}" number="{number}"/>' if series else ""
            body = TEMPLATE.format(genre=genre, first=first, last=last, title=title, sequence=sequence, file=file)
            archive.writestr(file, body)
            fields = [f"{last},{first},:", f"{genre}:", title, series or "", str(number or ""), Path(file).stem, str(len(body.encode())), str(lib_id), "0", "fb2", "2026-01-01", "en", "0", ""]
            lines.append(FIELDS_SEPARATOR.join(fields) + FIELDS_SEPARATOR)

    with zipfile.ZipFile(folder / "test.inpx", "w", zipfile.ZIP_DEFLATED) as inpx:
        inpx.writestr(f"{ARCHIVE}.inp", "\r\n".join(lines) + "\r\n")
        inpx.writestr("collection.info", "FLibrary OPDS test collection\r\n")
        inpx.writestr("version.info", "20260101\r\n")


if __name__ == "__main__":
    main()
