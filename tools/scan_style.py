"""Naming and formatting scan of the first-party sources. Usage: python3 tools/scan_style.py src/home"""

import re
import sys
from collections import Counter
from pathlib import Path

root = Path(sys.argv[1])
files = sorted(p for p in root.rglob("*") if p.suffix in (".cpp", ".h"))

bom = crlf = long_lines = cyr_files = 0
long_files = Counter()
pragma_missing = []
lower_files = set()
dirs_upper, dirs_lower = set(), set()
member_styles = Counter()
impl_plain_members = []
typos = Counter()
macros_x = 0
todo = 0
typo_re = re.compile(
    r"\b\w*(Recie|recie|Seperat|seperat|Lenght|lenght|Occured|occured|Adress|adress|Neccess|neccess|Existan|Dependan|Unkown|unkown|Paramter|paramter|Retreiv|retreiv|Sucess|sucess|Succesf|succesf|Initialz|lirary|Lirary|Tempor[^a]|Recod(?!e)|Guid(?!e|ance)|AdDeleted|Ooб)\w*"
)
member_decl = re.compile(r"^\s+(?:const\s+|mutable\s+|static\s+)*[A-Za-z_][\w:<>,\s\*&]*?[\s\*&]((?:m_)?[a-zA-Z_]\w*)\s*(?:\{[^}]*\}|=[^;]*)?;\s*(?://.*)?$")

for f in files:
    raw = f.read_bytes()
    text = raw.decode("utf-8", errors="replace")
    if raw.startswith(b"\xef\xbb\xbf"):
        bom += 1
    if b"\r\n" in raw:
        crlf += 1
    lines = text.splitlines()
    n_long = sum(1 for l in lines if len(l.expandtabs(4)) > 160)
    long_lines += n_long
    if n_long:
        long_files[str(f.relative_to(root))] = n_long
    if re.search(r"[Ѐ-ӿ]", text):
        cyr_files += 1
    if f.suffix == ".h" and "#pragma once" not in text[:400]:
        pragma_missing.append(str(f.relative_to(root)))
    if f.name[0].islower():
        lower_files.add(f.name)
    for m in typo_re.finditer(text):
        typos[m.group(0)] += 1
    macros_x += len(re.findall(r"#define \w+_X_MACRO|_XMACRO\b", text))
    todo += len(re.findall(r"\b(TODO|FIXME|HACK)\b", text))

    # member naming inside class bodies: "m_name" vs plain "name"
    in_struct_impl = False
    for line in lines:
        if re.match(r"\s*(struct|class)\s+\w*Impl\b", line):
            in_struct_impl = True
        if in_struct_impl and re.match(r"^};", line):
            in_struct_impl = False
        m = member_decl.match(line)
        if m and not re.match(r"\s*(return|using|typedef|friend|case|delete|throw|goto|emit|co_\w+)\b", line) and "(" not in line:
            name = m.group(1)
            style = "m_" if name.startswith("m_") else "plain"
            member_styles[style] += 1
            if in_struct_impl and style == "plain":
                impl_plain_members.append(str(f.relative_to(root)))

for d in root.rglob("*"):
    if d.is_dir():
        (dirs_upper if d.name[0].isupper() else dirs_lower).add(d.name)

print(f"files: {len(files)}")
print(f"UTF-8 BOM: {bom}, CRLF: {crlf}")
print(f"lines > 160 columns: {long_lines} in {len(long_files)} files; top: {long_files.most_common(5)}")
print(f"files containing Cyrillic (comments/strings): {cyr_files}")
print(f"headers without #pragma once near the top: {pragma_missing[:10]} ({len(pragma_missing)})")
print(f"lower-case source file names: {sorted(lower_files)}")
print(f"directories PascalCase: {sorted(dirs_upper)}")
print(f"directories lower-case: {sorted(dirs_lower)}")
print(f"member-like declarations: {dict(member_styles)}")
print(f"Impl structs with plain (non m_) members, by file: {Counter(impl_plain_members).most_common(8)}")
print(f"possible misspellings: {typos.most_common(25)}")
print(f"X-macro definitions/uses: {macros_x}, TODO/FIXME/HACK: {todo}")
