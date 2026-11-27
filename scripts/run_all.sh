#!/usr/bin/env bash
# Chay tron cac thi nghiem histogram (va tich vo huong neu da co scripts/run_dot.sh).
#   scripts/run_all.sh              # lo chinh tren may do cuoi
#   PILOT=b scripts/run_all.sh      # cau hinh nho (kiem tra M3: chay tron tren may do cuoi)
# Cac bien ghi de cua run_hist.sh (N_LIST, T_LIST, RUNS, P, ...) dung chung cho moi thi nghiem.
set -euo pipefail
cd "$(dirname "$0")/.."

make -s
for e in ${EXPS:-E1 E2 E3 E4 E5}; do
  echo "==== $e ====" >&2
  bash scripts/run_hist.sh "$e"
done

if [ -f scripts/run_dot.sh ]; then
  echo "==== E6 ====" >&2
  bash scripts/run_dot.sh
else
  echo "Bo qua E6: chua co scripts/run_dot.sh" >&2
fi
