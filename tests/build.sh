#!/bin/sh
# build.sh - portable test build and run (POSIX). Used by CI; needs gcc or clang.
# Env: CC (default: gcc, fallback clang), RB_TEST_SOAK (property-test iteration multiplier).
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$DIR/.." && pwd)
CC="${CC:-gcc}"
command -v "$CC" >/dev/null 2>&1 || CC=clang
command -v "$CC" >/dev/null 2>&1 || { echo "no gcc/clang found"; exit 2; }

SAN="-fsanitize=address,undefined"
CFLAGS="-std=c99 -g -O1 -Wall -Wextra -Wpedantic -I$ROOT -DRB_TEST_SOAK=${RB_TEST_SOAK:-1}"

echo "== build: test_base =="
"$CC" $SAN $CFLAGS "$DIR/test_base.c" "$ROOT/ring_buffer.c" -o "$DIR/test_base"
"$DIR/test_base"

echo "== build: test_chapter =="
"$CC" $SAN $CFLAGS "$DIR/test_chapter.c" "$ROOT/ring_buffer.c" "$ROOT/ring_buffer_chapter.c" -o "$DIR/test_chapter"
"$DIR/test_chapter"

echo "== build: repro_overflow (no crash expected after fix) =="
"$CC" $SAN $CFLAGS "$DIR/repro_overflow.c" "$ROOT/ring_buffer.c" -o "$DIR/repro_overflow"
"$DIR/repro_overflow"

echo "== build: test_hooks (critical-section hook injection) =="
"$CC" $SAN $CFLAGS -DRB_CRITICAL_ENTER=rb_hook_lock -DRB_CRITICAL_EXIT=rb_hook_unlock \
    -include "$DIR/rb_hook_decl.h" \
    "$DIR/test_hooks.c" "$ROOT/ring_buffer.c" "$ROOT/ring_buffer_chapter.c" -o "$DIR/test_hooks"
"$DIR/test_hooks"

echo "ALL TEST BINS DONE"
