#!/bin/bash
# Start-up test for the FLibrary GUI: the main window must come up, and the app keep running,
# both for a collection with books and for an empty one (where no navigation mode has any data).
#
# Usage: gui_startup_test.sh <build dir>
# Needs: curl, python3, and built FLibrary and opds binaries in <build dir>/bin (opds creates the test collections).
# Runs fully isolated on Qt's offscreen platform: settings, collections and logs live in a temporary directory.

set -u

build=$(cd "${1:?usage: gui_startup_test.sh <build dir>}" && pwd)
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
fixture="$here/../../tool/opds/test/make_fixture.py"

# Short path on purpose: the apps create unix sockets in TMPDIR, and a socket path is limited to 108 bytes
work=$(mktemp -d /tmp/flib-gui-test.XXXXXX)
failures=0
checks=0

# How long the GUI gets to build its main window, and how long it must then stay up
startup_timeout_s=30
stay_up_s=3

cleanup()
{
	if [ "$failures" -eq 0 ] && [[ "$work" == /tmp/flib-gui-test.* ]]; then
		rm -rf -- "$work"
	else
		echo "work folder kept for inspection: $work"
	fi
}
trap cleanup EXIT

report()
{
	checks=$((checks + 1))
	if [ "$2" = ok ]; then
		echo "  ok    $1"
	else
		failures=$((failures + 1))
		echo "  FAIL  $1: $3"
	fi
}

# create_collection <case folder> <archive folder>: lets the opds tool create and index a collection, then stops it
create_collection()
{
	local dir=$1 archives=$2
	mkdir -p "$dir/config/HomeCompa" "$dir/tmp" "$archives"
	local port
	port=$(python3 -I -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
	# No check for a newer release at start-up: the test must not depend on the network
	printf '[opds]\nhost=127.0.0.1\nport=%s\n\n[ui]\nView\\CheckForUpdateOnStart=false\n' "$port" >"$dir/config/HomeCompa/FLibrary.conf"

	(cd "$build/bin" && XDG_CONFIG_HOME="$dir/config" TMPDIR="$dir/tmp" exec ./opds --name test --database "$dir/library.db" --archives "$archives") >"$dir/opds.out" 2>&1 &
	local pid=$!
	for _ in $(seq 1 60); do
		if [ "$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:$port/favicon.ico")" = 200 ]; then
			kill "$pid"
			wait "$pid" 2>/dev/null
			return 0
		fi
		kill -0 "$pid" 2>/dev/null || break
		sleep 0.5
	done
	kill "$pid" 2>/dev/null
	wait "$pid" 2>/dev/null
	return 1
}

# check_startup <description> <case folder>
check_startup()
{
	local description=$1 dir=$2
	local log="$dir/tmp/HomeCompa.FLibrary.log"

	(cd "$build/bin" && XDG_CONFIG_HOME="$dir/config" TMPDIR="$dir/tmp" QT_QPA_PLATFORM=offscreen exec ./FLibrary) >"$dir/gui.out" 2>&1 &
	local pid=$!

	local result="main window not created within ${startup_timeout_s}s"
	for _ in $(seq 1 $((startup_timeout_s * 2))); do
		if ! kill -0 "$pid" 2>/dev/null; then
			wait "$pid"
			result="exited during start-up with status $?"
			break
		fi
		if grep -q 'MainWindow created' "$log" 2>/dev/null; then
			sleep "$stay_up_s"
			if kill -0 "$pid" 2>/dev/null; then
				result=ok
			else
				wait "$pid"
				result="exited right after start-up with status $?"
			fi
			break
		fi
		sleep 0.5
	done

	kill "$pid" 2>/dev/null
	wait "$pid" 2>/dev/null
	report "$description" "$result" "$result, see $dir"
}

#..............................................................................

export LD_LIBRARY_PATH="$build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ICU_DATA="$build/lib"

echo "GUI start-up"

# The books must exist before opds indexes the collection
mkdir -p "$work/books/archives"
python3 -I "$fixture" "$work/books/archives" || exit 1
if ! create_collection "$work/books" "$work/books/archives"; then
	report "collection with books is created" fail "opds could not create it, see $work/books"
fi
check_startup "starts with a collection that has books" "$work/books"

if ! create_collection "$work/empty" "$work/empty/archives"; then
	report "empty collection is created" fail "opds could not create it, see $work/empty"
fi
check_startup "starts with an empty collection" "$work/empty"

echo
if [ "$failures" -eq 0 ]; then
	echo "all $checks checks passed"
	exit 0
fi
echo "$failures of $checks checks failed"
exit 1
