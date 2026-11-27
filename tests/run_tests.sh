#!/usr/bin/env bash
# Chay moi file test da build trong thu muc $1. Test tra ve 0 la dat.
# Test truot thi in 40 dong cuoi cua output (dong [FAIL], bao cao ASan/TSan) de doc duoc tren CI.
set -u
DIR=${1:-build/release/tests}
PASS=0; FAIL=0
for t in "$DIR"/*; do
  [ -x "$t" ] || continue
  for T in 1 2 4 8; do
    if out=$(OMP_NUM_THREADS=$T "$t" 2>&1); then PASS=$((PASS+1));
    else
      echo "  [FAIL] $(basename "$t") voi OMP_NUM_THREADS=$T"
      printf '%s\n' "$out" | grep -v '^  \[OK\]' | tail -n 40 | sed 's/^/      /'
      FAIL=$((FAIL+1))
    fi
  done
done
echo "Test: $PASS dat, $FAIL truot"
[ $FAIL -eq 0 ]
