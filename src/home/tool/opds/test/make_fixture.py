"""Writes a tiny book archive and its INPX index for the OPDS server and GUI tests.

Usage: python3 make_fixture.py <archive folder> [--update]
  --update  adds one more archive with one more book, as a daily update does, and indexes every archive
"""

import base64
import io
import struct
import sys
import zipfile
import zlib
from pathlib import Path

# (file name, author, title, genre, series, series number, language, keywords)
# An author is "last,first,middle"; a [nickname] suffix belongs to the name and is stored apart.
# The fourth book covers awkward data: an author with a middle name but no first name, and a file name with an apostrophe.
# Nikolaev has books in two series, one of them an ePub, for the tree-mode check marks and the ePub annotation.
# The desktop app offers a navigation mode only when it has at least two values, hence two series, languages and keywords
BOOKS = [
    ("1.fb2", "Petrov,Ivan,", "First Test Book", "sf", "Test Saga", 1, "en", "magic"),
    ("2.fb2", "Petrov,Ivan,", "Second Test Book", "sf", "Test Saga", 2, "en", ""),
    ("3.fb2", "Smith,Anna,", "Detective Story", "det_classic", "Case Files", 1, "en", ""),
    ("O'Brien - Story.fb2", "Brown,,Lee", "Lost Letters", "prose_classic", None, None, "de", "letters"),
    ("5.fb2", "Nikolaev [nick],Petr,", "Paper Pages", "sci_history", "Leaves", 1, "en", ""),
    ("6.epub", "Nikolaev [nick],Petr,", "Electronic Pages", "sci_history", "Pages", 1, "en", ""),
]

UPDATE_BOOKS = [
    ("7.fb2", "Nikolaev [nick],Petr,", "Late Pages", "sci_history", "Leaves", 2, "en", ""),
]

# The first book carries an annotation, a cover and publisher details for the annotation panel
ANNOTATED_BOOK = "1.fb2"
ANNOTATION = "Annotation of {title}."
PUBLISHER = "Test Press"
EPUB_PUBLISHER = "Test Publisher"

# Non-ASCII body text (Cyrillic "Test"), so an export in another encoding has bytes to convert
BODY_TEXT = "Text of {title}. Тест."

# The second book is stored in windows-1251, as many older fb2 files are, so a download in UTF-8 has bytes to convert
CP1251_BOOK = "2.fb2"

FB2_TEMPLATE = """<?xml version="1.0" encoding="{encoding}"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">
<description>
<title-info>
<genre>{genre}</genre>
<author><first-name>{first}</first-name><middle-name>{middle}</middle-name><last-name>{last}</last-name></author>
<book-title>{title}</book-title>
{annotation}
{coverpage}
<lang>{lang}</lang>
{sequence}
</title-info>
<document-info><author><nickname>test</nickname></author><date>2026-01-01</date><id>{file}</id><version>1.0</version></document-info>
{publish_info}
</description>
<body><section><p>{body}</p></section></body>
{binary}
</FictionBook>
"""

OPF_TEMPLATE = """<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id">
<metadata xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:opf="http://www.idpf.org/2007/opf">
<dc:title>{title}</dc:title>
<dc:creator opf:role="aut">{first} {last}</dc:creator>
<dc:language>{lang}</dc:language>
<dc:description>{annotation}</dc:description>
<dc:publisher>{publisher}</dc:publisher>
<dc:date>2021</dc:date>
<dc:identifier id="id" opf:scheme="ISBN">978-0-00-000000-2</dc:identifier>
</metadata>
<manifest><item id="text" href="text.xhtml" media-type="application/xhtml+xml"/></manifest>
<spine><itemref idref="text"/></spine>
</package>
"""

CONTAINER_XML = """<?xml version="1.0" encoding="utf-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
<rootfiles><rootfile full-path="content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>
"""

XHTML_TEMPLATE = """<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml"><head><title>{title}</title></head><body><p>{body}</p></body></html>
"""

# Two archives, so that the Archives navigation mode is offered: the ePub has one of its own
ARCHIVES = [("fb2-000001-000005", BOOKS[:5]), ("fb2-000006-000006", BOOKS[5:])]
UPDATE_ARCHIVE = ("fb2-000007-000007", UPDATE_BOOKS)

# INPX line: fields in the parser's default order (AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;LIBRATE;KEYWORDS),
# separated by 0x04; authors and genres are ':'-terminated lists, author names are "last,first,middle"
FIELDS_SEPARATOR = "\x04"


def png(width: int, height: int, rgb: tuple[int, int, int]) -> bytes:
    """A plain one-colour PNG image."""

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    rows = b"".join(b"\x00" + bytes(rgb) * width for _ in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


def entry(name: str) -> zipfile.ZipInfo:
    """A zip entry with a fixed date, so that the archives are the same on every run."""
    info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    return info


def fb2(file: str, author: str, title: str, genre: str, series: str | None, number: int | None, lang: str) -> bytes:
    last, first, middle = author.split(",")
    last = last.split(" [")[0]
    annotated = file == ANNOTATED_BOOK
    encoding = "windows-1251" if file == CP1251_BOOK else "utf-8"
    cover = base64.b64encode(png(8, 12, (200, 40, 40))).decode()
    return FB2_TEMPLATE.format(
        encoding=encoding,
        genre=genre,
        first=first,
        middle=middle,
        last=last,
        title=title,
        lang=lang,
        file=file,
        body=BODY_TEXT.format(title=title),
        sequence=f'<sequence name="{series}" number="{number}"/>' if series else "",
        annotation=f"<annotation><p>{ANNOTATION.format(title=title)}</p></annotation>" if annotated else "",
        coverpage='<coverpage><image l:href="#cover.png"/></coverpage>' if annotated else "",
        publish_info=f"<publish-info><publisher>{PUBLISHER}</publisher><city>Test City</city><year>2020</year><isbn>978-0-00-000000-1</isbn></publish-info>" if annotated else "",
        binary=f'<binary id="cover.png" content-type="image/png">{cover}</binary>' if annotated else "",
    ).encode(encoding)


def epub(author: str, title: str, lang: str) -> bytes:
    last, first, _ = author.split(",")
    data = io.BytesIO()
    with zipfile.ZipFile(data, "w") as book:
        book.writestr(zipfile.ZipInfo("mimetype", date_time=(2026, 1, 1, 0, 0, 0)), "application/epub+zip")
        book.writestr(entry("META-INF/container.xml"), CONTAINER_XML)
        book.writestr(entry("content.opf"), OPF_TEMPLATE.format(title=title, first=first, last=last.split(" [")[0], lang=lang, annotation=ANNOTATION.format(title=title), publisher=EPUB_PUBLISHER))
        book.writestr(entry("text.xhtml"), XHTML_TEMPLATE.format(title=title, body=BODY_TEXT.format(title=title)))
    return data.getvalue()


def write_archive(folder: Path, archive_name: str, books: list, first_lib_id: int) -> str:
    """Writes the archive and returns the text of its .inp file."""
    lines = []
    with zipfile.ZipFile(folder / f"{archive_name}.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        for lib_id, (file, author, title, genre, series, number, lang, keywords) in enumerate(books, start=first_lib_id):
            ext = Path(file).suffix[1:]
            body = epub(author, title, lang) if ext == "epub" else fb2(file, author, title, genre, series, number, lang)
            archive.writestr(entry(file), body)
            fields = [f"{author}:", f"{genre}:", title, series or "", str(number or ""), Path(file).stem, str(len(body)), str(lib_id), "0", ext, "2026-01-01", lang, "0", keywords]
            lines.append(FIELDS_SEPARATOR.join(fields) + FIELDS_SEPARATOR)
    return "\r\n".join(lines) + "\r\n"


def main() -> None:
    folder = Path(sys.argv[1])
    update = "--update" in sys.argv[2:]
    folder.mkdir(parents=True, exist_ok=True)

    inp = {}
    lib_id = 1
    for name, books in ARCHIVES + ([UPDATE_ARCHIVE] if update else []):
        inp[name] = write_archive(folder, name, books, lib_id)
        lib_id += len(books)

    with zipfile.ZipFile(folder / "test.inpx", "w", zipfile.ZIP_DEFLATED) as inpx:
        for name, text in inp.items():
            inpx.writestr(entry(f"{name}.inp"), text)
        inpx.writestr(entry("collection.info"), "FLibrary OPDS test collection\r\n")
        inpx.writestr(entry("version.info"), "20260102\r\n" if update else "20260101\r\n")


if __name__ == "__main__":
    main()
