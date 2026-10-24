#!/bin/bash
# Starts a locally built FLibrary (or the OPDS/web server) straight from the build folder,
# with the library paths it needs. Works from a normal terminal and from VS Code's terminal.
#
# Usage: ./run.sh [--debug] [--opds] [program arguments...]
#   --debug   run the Debug build in build-debug/ instead of build/
#   --opds    run the OPDS/web server (opds) instead of the desktop app
# Every other argument is passed to the program; put "--" before arguments that clash with the ones above.

set -euo pipefail

repo=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
build="$repo/build"
program=FLibrary
args=()

while [ $# -gt 0 ]; do
	case "$1" in
		--debug) build="$repo/build-debug" ;;
		--opds) program=opds ;;
		--)
			shift
			args+=("$@")
			break
			;;
		*) args+=("$1") ;;
	esac
	shift
done

binary="$build/bin/$program"
if [ ! -x "$binary" ]; then
	echo "$binary not found. Build it first: cmake --build $(basename "$build") --parallel 8" >&2
	exit 1
fi

# A snap-packaged VS Code passes its own GTK, GSettings and locale paths to everything started from it, and the system GTK
# that Qt loads then aborts on the snap's settings schemas. Restore the values the snap saved and drop the ones it added
# (only those pointing into a snap, so values set on purpose are kept)
for name in $(compgen -e); do
	case "$name" in
		*_VSCODE_SNAP_ORIG)
			original=${name%_VSCODE_SNAP_ORIG}
			if [ -n "${!name}" ]; then
				export "$original=${!name}"
			else
				unset "$original"
			fi
			unset "$name"
			;;
	esac
done
for name in GSETTINGS_SCHEMA_DIR GTK_PATH GTK_EXE_PREFIX GIO_MODULE_DIR GDK_PIXBUF_MODULE_FILE GDK_PIXBUF_MODULEDIR LOCPATH; do
	case "${!name:-}" in
		/snap/* | "$HOME"/snap/*) unset "$name" ;;
	esac
done

# The build's own libraries (Qt modules copied by the build, ICU, DjVuLibre, 7z.so) and ICU's data file
export LD_LIBRARY_PATH="$build/lib:$build/bin${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export ICU_DATA="$build/lib"

cd "$build/bin"
exec "./$program" "${args[@]}"
