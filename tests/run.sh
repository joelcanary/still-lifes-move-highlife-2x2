#!/bin/sh
# Test suite: builds both enumerators and checks them against each other and against the
# values the OEIS published before this work. Runs in about a minute on one core.
#
#   sh tests/run.sh
#
# What a pass means: both programs reproduce every published term checked here, and they
# agree with each other box by box on a nontrivial case. What it does not mean: the new
# terms are re-derived here (they take minutes to hours; see data/boxes/ and README.md).
set -u
cd "$(dirname "$0")/.."
make -s build || exit 1
fails=0
pass() { printf '  ok    %s\n' "$1"; }
fail() { printf '  FAIL  %s\n' "$1"; fails=$((fails + 1)); }

# term PROGRAM RULE n -> a(n), computed box by box
term() { sh scripts/count.sh "$1" "$2" "$3" "${JOBS:-2}" | awk '{s += $3} END {print s + 0}'; }

# check RULE NMAX "published terms a(1)..a(NMAX)" NAME
check() {
  rule=$1; nmax=$2; expected=$3; name=$4
  for prog in build/stilllife build/crosscheck; do
    got=""; n=1
    while [ "$n" -le "$nmax" ]; do got="$got $(term "$prog" "$rule" "$n")"; n=$((n + 1)); done
    got=${got# }
    if [ "$got" = "$expected" ]; then pass "$name, $(basename "$prog"): a(1..$nmax) = $got"
    else fail "$name, $(basename "$prog"): expected [$expected], got [$got]"; fi
  done
}

echo "Published terms (OEIS, before this work)"
check B3/S23    11 "0 0 0 2 1 5 4 9 10 25 46"    "A019473 Conway"
check B36/S23   11 "0 0 0 2 1 4 4 9 9 25 44"     "A175000 HighLife"
check B368/S245 10 "0 0 0 1 1 2 2 4 1 3"         "A175001 Move"
check B36/S125   8 "0 2 1 3 4 9 10 27"           "A166476 2x2"
check B3/S12345  8 "0 2 1 5 7 18 40 197"         "A175002 Maze"

echo "The two programs agree box by box"
for spec in "B368/S245 13" "B36/S23 12"; do
  set -- $spec
  a=$(sh scripts/count.sh build/stilllife "$1" "$2" "${JOBS:-2}" | sort -n -k2 -k1)
  b=$(sh scripts/count.sh build/crosscheck "$1" "$2" "${JOBS:-2}" | sort -n -k2 -k1)
  if [ "$a" = "$b" ]; then pass "$1 n=$2: $(printf '%s\n' "$a" | wc -l | tr -d ' ') boxes identical"
  else fail "$1 n=$2: the box counts differ"; fi
done

echo "Definitions"
# Pseudo still lifes are stable but must not be counted: with 8 cells, two blocks one column
# apart are stable and split into two stable pieces. stilllife reports every stable pattern on
# stderr, so the strict count must be the published 9 while the stable count is larger.
strict=$(build/stilllife B3/S23 8 2>/dev/null)
stable=$(build/stilllife B3/S23 8 2>&1 >/dev/null | sed -n 's/.*(strict and pseudo): //p')
if [ "$strict" = "9" ] && [ "$stable" -gt 9 ]; then pass "Conway n=8: 9 strict out of $stable stable, pseudo still lifes excluded"
else fail "Conway n=8: strict=$strict stable=$stable"; fi

echo "Data files"
for f in data/b*.txt; do
  seq=$(basename "$f" .txt | sed 's/^b/A/')
  if awk 'NF == 0 {next} NF != 2 || $1 != ++i {bad = 1} END {exit bad}' "$f"; then pass "$seq: consecutive n, one term per line"
  else fail "$seq: malformed b-file"; fi
done
for f in data/boxes/*.txt; do
  name=$(basename "$f" .txt)            # e.g. A175001_n17_stilllife
  key=${name%_*}                        # A175001_n17
  sum=$(awk '{s += $3} END {print s + 0}' "$f")
  want=$(awk -v k="$key" '$1 == k {print $2}' data/terms.txt)
  if [ -n "$want" ] && [ "$sum" = "$want" ]; then pass "$name: boxes add up to $want"; else fail "$name: boxes add up to $sum, expected $want"; fi
done
for key in $(awk '{print $1}' data/terms.txt); do
  if [ "$(sort -n -k2 -k1 "data/boxes/${key}_stilllife.txt")" = "$(sort -n -k2 -k1 "data/boxes/${key}_crosscheck.txt")" ]; then pass "$key: both programs agree on every box"
  else fail "$key: the two programs disagree on some box"; fi
done
# Each new term is also the value the b-file (what the OEIS serves) has at that index.
while read -r key want; do
  [ -n "$key" ] || continue
  seq=${key%_n*}; n=${key##*_n}
  got=$(awk -v n="$n" '$1 == n {print $2}' "data/b$(printf '%s' "$seq" | sed 's/^A//').txt")
  if [ "$got" = "$want" ]; then pass "$key: $want matches the b-file"; else fail "$key: $want, b-file says ${got:-nothing}"; fi
done < data/terms.txt

echo
if [ "$fails" -eq 0 ]; then echo "all tests passed"; else echo "$fails test(s) failed"; fi
exit "$fails"
