#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT_FILE="$ROOT_DIR/compile_commands.json"
TMP_FILE=$(mktemp)

cleanup() {
    rm -f "$TMP_FILE"
}

trap cleanup EXIT

cd "$ROOT_DIR"

make -n \
    | rg ' -c .*/src/.*\.cpp ' \
    | awk -v root="$ROOT_DIR" '
BEGIN {
    print "["
    first = 1
}
{
    if (!first) {
        print ","
    }
    first = 0

    file = ""
    if (match($0, /-c [^ ]+/)) {
        file = substr($0, RSTART + 3, RLENGTH - 3)
    }

    gsub(/\\/, "\\\\", $0)
    gsub(/"/, "\\\"", $0)

    printf "  {\n"
    printf "    \"directory\": \"%s\",\n", root
    printf "    \"command\": \"%s\",\n", $0
    printf "    \"file\": \"%s\"\n", file
    printf "  }"
}
END {
    print "\n]"
}' > "$TMP_FILE"

mv "$TMP_FILE" "$OUTPUT_FILE"
echo "Wrote $OUTPUT_FILE"
