#!/bin/sh
# Counts strict still lifes with n cells, splitting the work by box across several processes.
#
#   sh scripts/count.sh PROGRAM RULE n JOBS > boxes.txt
#
# PROGRAM is build/stilllife or build/crosscheck. Each output line is "w h count", one per
# box with w <= h; the term a(n) is the sum of the third column:
#
#   awk '{s += $3} END {print s}' boxes.txt
#
# Boxes are queued from the largest to the smallest, because the large ones take longest.
set -e
prog=$1; rule=$2; n=$3; jobs=${4:-1}
[ -x "$prog" ] && [ -n "$rule" ] && [ -n "$n" ] || { echo "usage: sh scripts/count.sh PROGRAM RULE n [JOBS]" >&2; exit 2; }
hmax=$((2 * n - 1)); [ "$hmax" -gt 62 ] && hmax=62
h=$hmax
while [ "$h" -ge 1 ]; do
  w=$h
  while [ "$w" -ge 1 ]; do echo "$w $h"; w=$((w - 1)); done
  h=$((h - 1))
done | xargs -P "$jobs" -n 2 sh -c 'printf "%s %s %s\n" "$0" "$1" "$(BOX=$0,$1 '"$prog"' '"$rule"' '"$n"' 2>/dev/null)"'
