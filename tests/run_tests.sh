#!/usr/bin/env bash
# Chay moi file test da build trong thu muc $1. Test tra ve 0 la dat.
set -u
DIR=${1:-build/release/tests}
PASS=0; FAIL=0
for t in "$DIR"/*; do
  [ -x "$t" ] || continue
  for T in 1 2 4 8; do
    if OMP_NUM_THREADS=$T "$t" >/dev/null 2>&1; then PASS=$((PASS+1));
    else echo "  [FAIL] $(basename "$t") voi OMP_NUM_THREADS=$T"; FAIL=$((FAIL+1)); fi
  done
done
echo "Test: $PASS dat, $FAIL truot"
[ $FAIL -eq 0 ]
