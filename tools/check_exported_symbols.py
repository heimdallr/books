"""Checks that our binaries do not export third-party library symbols (Linux).

A shared library that re-exports a statically linked libpng, freetype, libjpeg... lets system libraries loaded into the
same process (GTK's file dialog, gdk-pixbuf, Qt plugins) bind to those copies instead of their own, which crashed the app
("munmap_chunk(): invalid pointer") and broke image loading ("Wrong JPEG library version"). The build prevents it with
-Wl,--exclude-libs,ALL; this check catches a regression.

Usage: python3 tools/check_exported_symbols.py <build dir>
Exit code 0 when clean, 1 when a binary exports a third-party library it does not own.
"""

import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

PREFIXES = {
    "freetype": r"FT_", "libpng": r"png_", "libjpeg": r"jpeg_", "libtiff": r"TIFF", "libxml2": r"(xml[A-Z]|html[A-Z])",
    "zlib": r"(inflate|deflate|crc32$|adler32$|compress2?$|uncompress)", "lcms2": r"cms[A-Z]", "openjpeg": r"opj_",
    "bzip2": r"BZ2_", "xz": r"lzma_", "harfbuzz": r"hb_", "icu": r"(u_|ucnv_|ubrk_|uloc_)", "libjxl": r"Jxl",
    "brotli": r"Brotli", "djvulibre": r"ddjvu_", "poppler-glib": r"poppler_", "fontconfig": r"Fc[A-Z]", "cairo": r"cairo_",
}
pattern = {name: re.compile("^" + p) for name, p in PREFIXES.items()}

# Libraries that are those third-party libraries and legitimately export their own API
OWN_API = {"libicu": "icu", "libdjvulibre": "djvulibre"}

build = Path(sys.argv[1])
targets = [p for p in sorted((build / "lib").glob("*.so*")) if not p.is_symlink()]
targets += [p for p in (build / "bin/FLibrary", build / "bin/opds") if p.exists()]

failed = False
for path in targets:
    out = subprocess.run(["nm", "-D", "--defined-only", str(path)], capture_output=True, text=True, check=True).stdout
    counts = Counter()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in "TDBRWV":
            for name, rx in pattern.items():
                if rx.match(parts[2]):
                    counts[name] += 1

    own = next((api for prefix, api in OWN_API.items() if path.name.startswith(prefix)), None)
    foreign = {name: n for name, n in counts.items() if name != own}
    if foreign:
        failed = True
        print(f"FAIL {path.relative_to(build)} exports " + ", ".join(f"{k} ({v} symbols)" for k, v in sorted(foreign.items())))

if failed:
    sys.exit(1)
print(f"ok: {len(targets)} binaries export no third-party symbols")
