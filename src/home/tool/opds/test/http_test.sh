#!/bin/bash
# HTTP tests for the OPDS server (tool/opds):
#  - query parameters must not be able to change the SQL text (OPDS and ReactApp book lists)
#  - with a password set, every route that serves library data requires it
#
# Usage: http_test.sh <build dir>
# Needs: curl, python3, and a built opds binary in <build dir>/bin.
# Runs fully isolated: settings, the collection and the server's control socket live in a temporary directory.

set -u

build=$(cd "${1:?usage: http_test.sh <build dir>}" && pwd)
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# A short path on purpose: the server's control socket is created in TMPDIR, and a unix socket path is limited to 108 bytes
work=$(mktemp -d /tmp/flib-opds-test.XXXXXX)
server_pid=
failures=0
checks=0

#..............................................................................
# helpers

stop_server()
{
	if [ -n "$server_pid" ]; then
		kill "$server_pid" 2>/dev/null
		wait "$server_pid" 2>/dev/null
		server_pid=
	fi
}

cleanup()
{
	stop_server
	if [ "$failures" -eq 0 ] && [[ "$work" == /tmp/flib-opds-test.* ]]; then
		rm -rf -- "$work"
	else
		echo "work folder kept for inspection: $work"
	fi
}
trap cleanup EXIT

# Sets one key in the server's QSettings ini file, keeping everything the server wrote there
set_setting()
{
	python3 -I - "$conf" "$1" "$2" "$3" <<'EOF'
import configparser, sys
path, section, key, value = sys.argv[1:]
config = configparser.ConfigParser(interpolation=None)
config.optionxform = str
config.read(path)
if not config.has_section(section):
    config.add_section(section)
config[section][key] = value
with open(path, "w") as f:
    config.write(f, space_around_delimiters=False)
EOF
}

start_server()
{
	(cd "$build/bin" && exec ./opds "$@") >>"$work/server.out" 2>&1 &
	server_pid=$!
	for _ in $(seq 1 60); do
		if ! kill -0 "$server_pid" 2>/dev/null; then
			echo "server exited during start-up, see $work/server.out and $TMPDIR/HomeCompa.opds.log"
			exit 1
		fi
		if [ "$(curl -s -o /dev/null -w '%{http_code}' "$base/favicon.ico")" = 200 ]; then
			return
		fi
		sleep 0.5
	done
	echo "server did not start listening on $base"
	exit 1
}

# check <description> <expected http status> <path> [curl options...]
# Never request paths the server has no route for: it bans the client address for a minute
check_status()
{
	local description=$1 expected=$2 path=$3
	shift 3
	local actual
	actual=$(curl -s --max-time 20 -o "$work/response" -w '%{http_code}' "$@" "$base$path")
	report "$description" "$([ "$actual" = "$expected" ] && echo ok)" "expected $expected, got $actual for $path"
}

# check_body <description> <path> <text> <present|absent>
check_body()
{
	local description=$1 path=$2 text=$3 mode=$4
	curl -s --max-time 20 -o "$work/response" "$base$path"
	local found=absent
	grep -qF "$text" "$work/response" && found=present
	report "$description" "$([ "$found" = "$mode" ] && echo ok)" "'$text' is $found in $path, expected $mode"
}

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

#..............................................................................
# setup

export XDG_CONFIG_HOME="$work/config" TMPDIR="$work/tmp"
export LD_LIBRARY_PATH="$build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ICU_DATA="$build/lib"
mkdir -p "$XDG_CONFIG_HOME/HomeCompa" "$TMPDIR"
conf="$XDG_CONFIG_HOME/HomeCompa/FLibrary.conf"

port=$(python3 -I -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
base="http://127.0.0.1:$port"
set_setting opds host 127.0.0.1
set_setting opds port "$port"

python3 -I "$here/make_fixture.py" "$work/archives" || exit 1

#..............................................................................
echo "SQL injection through query parameters (no password set)"

start_server --name test --database "$work/test.db" --archives "$work/archives"

author_id=$(curl -s "$base/opds/Authors" | python3 -I -c '
import re, sys
feed = sys.stdin.read()
for entry in re.findall(r"<entry>.*?</entry>", feed, re.S):
    if "Petrov Ivan" in entry:
        print(re.search(r"Authors=(\d+)", entry).group(1))
')
genre_code=$(curl -s "$base/opds/Genres?Genres=001" | grep -oE 'Genres=001\.[0-9]+' | head -1 | cut -d= -f2)
if [ -z "$author_id" ] || [ -z "$genre_code" ]; then
	echo "cannot find the test author or genre in the feeds"
	failures=1
	exit 1
fi

check_body "author filter lists the author's book"                  "/opds/Books?Authors=$author_id"                                        "First Test Book" present
check_body "author filter excludes other authors"                   "/opds/Books?Authors=$author_id"                                        "Detective Story" absent
check_body "injected author filter returns no other books"          "/opds/Books?Authors=$author_id%20or%201=1"                             "Detective Story" absent
check_body "genre filter lists the genre's book"                    "/opds/Books?Genres=$genre_code"                                        "First Test Book" present
check_body "injected genre filter returns no other books"           "/opds/Books?Genres=$genre_code'%20or%20'1'='1"                         "Detective Story" absent
check_body "ReactApp author books list the author's book"           "/main/getBooks/getSearchAuthorBooks?selectedItemID=$author_id"         "First Test Book" present
check_body "injected ReactApp author books return no other books"   "/main/getBooks/getSearchAuthorBooks?selectedItemID=$author_id%20or%201=1" "Detective Story" absent

check_status "without a password, the book download is open"        200 "/Images/fb2/3"

opensearch=$(curl -s --max-time 20 "$base/opds/opensearch")
report "OpenSearch link uses the address the client called" \
	"$(grep -qF "template=\"$base/opds/search?q={searchTerms}\"" <<<"$opensearch" && echo ok)" "got: $opensearch"

# Switching the collection while other requests run must neither crash nor deadlock the server
collection_id=$(python3 -I -c 'import configparser, sys; c = configparser.ConfigParser(interpolation=None); c.read(sys.argv[1]); print(c["Collections"]["current"])' "$conf")
pids=()
for _ in $(seq 1 20); do
	curl -s --max-time 30 -o /dev/null "$base/opds/Books?Authors=$author_id" &
	pids+=($!)
	curl -s --max-time 30 -o /dev/null "$base/opds/collection?id=$collection_id" &
	pids+=($!)
done
wait "${pids[@]}"
check_body "books still listed after concurrent collection switches" "/opds/Books?Authors=$author_id" "First Test Book" present

stop_server

#..............................................................................
echo "Authentication on every data route (password set)"

user=tester
password=secret
machine_id=$(head -c 32 /var/lib/dbus/machine-id 2>/dev/null || head -c 32 /etc/machine-id)
set_setting opds Authentication "$(printf '%s%s:%s' "$machine_id" "$user" "$password" | md5sum | cut -d' ' -f1)"

start_server --name test

for path in "/opds" "/opds/Books?Authors=$author_id" "/" "/assets/index.js" "/main/getBooks/getConfig" "/Images/covers/3" "/Images/fb2/3" "/Images/fb2compact/3" "/Images/zip/3"; do
	check_status "no credentials: $path"    401 "$path"
done
check_status "wrong password: /Images/fb2/3" 401 "/Images/fb2/3" -u "$user:wrong"
report "a failed login does not write the credentials to the log" \
	"$(grep -qF "$(printf '%s' "$user:wrong" | base64)" "$TMPDIR/HomeCompa.opds.log" || echo ok)" "the base64 credentials are in $TMPDIR/HomeCompa.opds.log"

for path in "/opds" "/opds/Books?Authors=$author_id" "/" "/main/getBooks/getConfig" "/Images/fb2/3" "/Images/zip/3"; do
	check_status "with credentials: $path"  200 "$path" -u "$user:$password"
done
check_status "public route stays public: /favicon.ico" 200 "/favicon.ico"

# Last on purpose: if the server treats this as an unknown route, it bans the client address for a minute
traversal=$(curl -s --max-time 20 -o "$work/response" -w '%{http_code}' -u "$user:$password" "$base/assets/..%2F..%2FLICENSE.txt")
report "assets cannot reach files outside the web app folder" \
	"$([ "$traversal" != 200 ] && ! grep -q 'Permission is hereby granted' "$work/response" && echo ok)" "got $traversal: $(head -c 120 "$work/response")"

stop_server

#..............................................................................
echo
if [ "$failures" -eq 0 ]; then
	echo "all $checks checks passed"
	exit 0
fi
echo "$failures of $checks checks failed"
exit 1
