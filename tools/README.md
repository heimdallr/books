# Developer tools

Run from the repository root.

| Tool | What it does | Usage |
|---|---|---|
| `check_exported_symbols.py` | Fails if one of our binaries exports a third-party library (libpng, freetype, libjpeg, libxml2...). Such exports crash GTK's file dialog and break JPEG loading. | `python3 tools/check_exported_symbols.py build` |
| `find_contextless_timers.py` | Lists `QTimer::singleShot` lambdas without a context object, which can run after their object is destroyed. | `python3 tools/find_contextless_timers.py src` |
| `scan_style.py` | Naming and formatting metrics: long lines, BOMs, file and folder name case, `m_` members, misspellings. | `python3 tools/scan_style.py src/home` |
| `scan_design.py` | Design metrics: injected dependencies per constructor, single-implementation interfaces, embedded SQL, shared-library count. | `python3 tools/scan_design.py .` |

## Reproducers (`repro/`)

Small programs that load FLibrary's libraries into a process the way the app does, to show symbol clashes without the GUI.
Build them against the Debug or Release `lib/` folder. Set `QT_PREFIX` to your Qt installation (the folder that contains
`lib/cmake/Qt6`):

```
Q=$QT_PREFIX; L=$PWD/build/lib
g++-14 -std=c++17 -fPIC tools/repro/dialog_repro.cpp -I$Q/include -I$Q/include/QtWidgets -I$Q/include/QtGui -I$Q/include/QtCore \
    -o /tmp/dialog_repro -L$Q/lib -Wl,-rpath,$Q/lib -lQt6Widgets -lQt6Gui -lQt6Core \
    -Wl,--no-as-needed $L/libflipdf.so $L/libutil.so $L/libfljxl.so $L/libdjvulibre.so -Wl,-rpath,$L
QT_QPA_PLATFORMTHEME=gtk3 xvfb-run -a /tmp/dialog_repro      # native folder dialog: must stay up 4 s, exit 0

gcc-14 -std=c11 tools/repro/pixbuf_repro.c -o /tmp/pixbuf_repro -ldl -Wl,--no-as-needed $L/libdjvulibre.so -Wl,-rpath,$L
/tmp/pixbuf_repro some.jpg                                    # system JPEG loader: must print "loaded"
```

`jpeg_repro.cpp` does the same through Qt's own JPEG plugin (build like `dialog_repro.cpp`, linking `Qt6Gui` and `Qt6Core`).
