"""Prints each QTimer::singleShot lambda that has no context object, with surrounding lines.

Without a context object Qt still runs the lambda after the objects it uses are destroyed (crash). Each site should pass
the object it depends on: QTimer::singleShot(0, &m_self, [this] { ... }). Sites that capture nothing or only copies are fine.

Usage: python3 tools/find_contextless_timers.py src
"""

import re
import sys
from pathlib import Path

site = re.compile(r"singleShot\([^,]+, *\[")
root = Path(sys.argv[1])
for path in sorted(list((root / "home").rglob("*.cpp")) + list((root / "ext/foundation").rglob("*.cpp"))):
    lines = path.read_text(errors="replace").splitlines()
    for i, line in enumerate(lines):
        if site.search(line):
            print(f"===== {path.relative_to(root)}:{i + 1}")
            for j in range(max(0, i - 3), min(len(lines), i + 6)):
                print(f"{j + 1:5}  {lines[j][:150]}")
