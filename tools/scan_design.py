"""Design-level scan (DI constructor width, single-implementation interfaces, embedded SQL, shared libraries...).
Usage: python3 tools/scan_design.py .   (run from the repository root)"""

import re
import sys
from collections import Counter
from pathlib import Path

repo = Path(sys.argv[1])
home = repo / "src/home"
fnd = repo / "src/ext/foundation"
sources = [p for p in list(home.rglob("*")) + list(fnd.rglob("*")) if p.suffix in (".cpp", ".h")]
texts = {p: p.read_text(errors="replace") for p in sources}

# constructor parameter counts of DI-built classes (shared_ptr parameters)
ctor_re = re.compile(r"^(\w+)::\1\(\s*\n?((?:[^()]|\([^()]*\))*)\)", re.M)
ctors = []
for p, t in texts.items():
    if p.suffix != ".cpp":
        continue
    for m in ctor_re.finditer(t):
        params = m.group(2)
        n = params.count("std::shared_ptr<")
        if n:
            ctors.append((n, m.group(1), str(p.relative_to(repo))))
ctors.sort(reverse=True)
print("constructors with most injected dependencies:", ctors[:8])
print("constructors with >= 10 injected dependencies:", sum(1 for c in ctors if c[0] >= 10), "of", len(ctors))

# interfaces vs implementations
iface_re = re.compile(r"class\s+(?:\w+_EXPORT\s+)?(I[A-Z]\w+)\b(?!;)")
ifaces = set()
for t in texts.values():
    ifaces.update(iface_re.findall(t))
impl_count = Counter()
for t in texts.values():
    for name in ifaces:
        impl_count[name] += len(re.findall(r"public\s+(?:virtual\s+)?(?:\w+::)*" + name + r"\b", t))
single = sorted(n for n in ifaces if impl_count[n] == 1)
none = sorted(n for n in ifaces if impl_count[n] == 0)
print(f"interfaces: {len(ifaces)}, with exactly one implementation: {len(single)}, with none found (observer/callback interfaces etc.): {len(none)}")

# pimpl usage
pimpl = sum(1 for t in texts.values() if re.search(r"\bstruct Impl;|\bclass Impl;", t))
print("headers declaring a pimpl Impl:", pimpl)
struct_impl = sum(len(re.findall(r"^struct \w+::Impl\b", t, re.M)) for t in texts.values())
class_impl = sum(len(re.findall(r"^class \w+::Impl\b", t, re.M)) for t in texts.values())
print(f"Impl defined as struct (public, plain member names): {struct_impl}, as class (m_ members): {class_impl}")

# Qt5 / Qt6 conditionals
qt5 = sum(len(re.findall(r"QT_VERSION|QT5|QT6|Qt5|QT_VERSION_CHECK", t)) for t in texts.values())
print("Qt version conditionals in sources:", qt5)
qt5_cmake = sum(len(re.findall(r"QT5|QT_MAJOR_VERSION", p.read_text(errors='replace'))) for p in list(repo.glob("CMakeLists.txt")) + list(home.rglob("*.cmake")) + list((repo / "src/ext/scripts/cmake").rglob("*.cmake")))
print("Qt version conditionals in cmake:", qt5_cmake)

# raw SQL text in C++ sources
sql_files = Counter()
for p, t in texts.items():
    n = len(re.findall(r"\b(select|SELECT)\b[^\"]{0,200}\b(from|FROM)\b", t))
    if n:
        sql_files[str(p.relative_to(repo))] = n
print("files with embedded SQL (select..from count):", sum(sql_files.values()), "in", len(sql_files), "files; top:", sql_files.most_common(6))

# X-macros
xm = Counter()
for p, t in texts.items():
    for m in re.findall(r"#define (\w*X_?MACRO\w*)", t):
        xm[m] += 1
print("X-macro lists defined:", len(xm))

# exported symbols macros (one per shared lib)
exports = sorted({m for t in texts.values() for m in re.findall(r"\b([A-Z]+_EXPORT)\b", t)})
print("export macros (shared library boundaries):", len(exports), exports)
