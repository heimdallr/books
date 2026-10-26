# FLibrary (books) — working rules

C++23 / Qt 6 e-book cataloguer: desktop app `FLibrary` and the OPDS/web server `opds`, on shared libraries in `src/home`
and the `src/ext/foundation` (books-fnd) and `src/ext/scripts` submodules. Follow the surrounding C++/Qt code and the
repo's `.clang-format`; style rules written for other projects (for example C naming rules in a personal CLAUDE.md) do
not apply here.

## Code map (read this before exploring)

- Modules (`*.cmake` per folder, `AddTarget` helper in `src/ext/scripts/cmake/target.cmake`): apps `FLibrary`
  (`src/home/flibrary/app`) and `opds` (`src/home/tool/opds`); libraries `gui`, `gutil`, `logic`, `inpx`, `genre`,
  `joke`, and in the foundation submodule `util`, `zip`, `dbfactory`/`dbsqlite`, `settings`, `network`/`rest`,
  format wrappers `flicu`/`fljxl`/`flidjvu`/`flipdf`, base `fnd`/`flint`/`logging`/`platform`/`ver`.
- Start-up sequence: `src/home/flibrary/app/main.cpp` (DI via `DiLogic`/`DiUi`/`DiGUtil`, translators, migrator check,
  `ITaskQueue::Execute`, `IMainWindow::Show`; restarts in-process on `Global::RESTART_APP`). `opds`: `tool/opds/main.cpp`.
- Data: INPX + archives → `inpx/inpx.cpp` (parser) → per-collection SQLite DB → `logic/data` (`DataProvider`,
  `NavigationQueryExecutor`, `BooksTreeGenerator`) → `logic/model` → `gui/TreeView.cpp`. Book contents are read from the
  archives on demand (`logic/Annotation/ArchiveParser.cpp`, `AnnotationController.cpp`).
- One shared SQLite connection (`logic/database/DatabaseController.cpp`); each `Query` holds its mutex while alive.
- HTTP server: `tool/opds/Server.cpp` (routes, `Authorization`, `RunRequest` lock), requesters in `tool/opds/requester`
  (OPDS `Requester`, web-app JSON `ReactAppRequester`, book bytes `NoSqlRequester`); web UI is the `src/ext/ReactApp` submodule.
- Settings: QSettings, `~/.config/HomeCompa/FLibrary.conf` (or `FLibrary.ini` beside the binary in portable mode).
  The navigation mode (`ui/Navigation\Mode`) is global, not per collection.

## Build

- GCC 14 or newer (`CC=gcc-14 CXX=g++-14`), Conan 2, Qt 6.10+ (with HttpServer, WebSockets, ImageFormats), the 7-Zip
  shared library. Release in `build/`, Debug in `build-debug/` (VS Code tasks "FLibrary: configure/build" and "(debug)").
  Configure (adjust the Qt and 7-Zip paths to your installation):
  `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DQt6_DIR=<qt-prefix>/lib/cmake/Qt6 -D7zip_BIN_DIR=<dir with 7z.so> -DNPM_EXECUTABLE=$(which npm) -DCPACK_GENERATOR=TXZ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
- The first configure builds all Conan dependencies from source and is memory-hungry: lower `--parallel` (and Conan's
  `tools.build:jobs`) if the machine runs short of RAM.
- Debug builds keep symbols; everything else is stripped (`CMakeLists.txt`).
- `.clang-format` needs clang-format 21 or newer (older versions reject the file; `pipx install clang-format` gives a
  current one). About 100 files already drift from the style on `develop`; do not mass-reformat. Format only changed
  lines: `git clang-format origin/develop -- <files>`.

## Run

- Start the apps with `./run.sh` (`--debug` for build-debug, `--opds` for the server). It sets `LD_LIBRARY_PATH`/`ICU_DATA`
  and, when started from a snap-packaged VS Code, removes the GTK/GSettings variables the snap leaks (they make GTK abort).
- Do not test against your real settings in `~/.config/HomeCompa`; use a temporary `XDG_CONFIG_HOME`.

## Test

- After changes, run on `build` (and on `build-debug` when asserts matter):
  `src/home/tool/opds/test/http_test.sh build`, `src/home/flibrary/test/gui_startup_test.sh build`, `ctest --test-dir build --output-on-failure`.
- Also run `python3 tools/check_exported_symbols.py build` (no third-party symbol exports) and
  `python3 tools/find_contextless_timers.py src` (new deferred calls need a context object). See `tools/README.md`.
- Every bug fix gets a test, and the test must fail without the fix: check it against the old code before calling it done.
- Tests are isolated: own `XDG_CONFIG_HOME`, and a short `TMPDIR` (the server's control socket path must stay under 108 bytes).
  The OPDS server bans a client for a minute after a request to an unknown route; keep such requests last in a test.
- GUI tests (`src/home/flibrary/test/gui/GuiTests.cpp`) build the real DI container in-process. Registrations capture the
  container variable by reference, so it must outlive the container (see `BuildContainer`).

## Code rules

- Request values never go into SQL text: bind them, or validate them strictly (integer ids, plain genre codes).
- Every route that serves library data goes through `Server::Impl::Authorization`. Credentials never appear in logs.
- HTTP requests run on worker threads. They must not touch main-thread objects (collections, `AnnotationController`)
  directly: use `AnnotationControllerObserver::Request` and the exclusive path in `RunRequest`.
- Every `QTimer::singleShot` / deferred call that touches an object passes that object as its context.
- Shared libraries must not re-export statically linked third-party libraries: keep `-Wl,--exclude-libs,ALL` (Linux), and
  the matching DjVuLibre settings in the Conan profiles.
- A query that can legitimately return no rows is checked with `Eof()`, not `assert`.

## Git

- Branch off `develop`. Without write access to the upstream repositories, push to your own forks of `books`, `books-fnd`
  and `scripts`, and open pull requests from there.
- Submodule changes: commit on a branch inside the submodule, push it before the main repo, then `git add` the submodule
  in the main repo. Keep the fork names `books-fnd` and `scripts`: `.gitmodules` uses relative URLs. Merge the submodule
  pull requests first; after a squash merge, update the submodule reference to the merged commit.

## Working with Claude Code here

- Use the code map above instead of exploring the tree; point at files and lines, and paste exact error text.
- Keep one area per session with a clear done criterion, and record open work in the session's notes or memory.
