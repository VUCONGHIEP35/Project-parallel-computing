#!/usr/bin/env bash
# Chay tron cac thi nghiem: histogram E1-E5, E7 (run_hist.sh) va tich vo huong E6 (run_dot.sh).
#   scripts/run_all.sh              # lo chinh tren may do cuoi
#   PILOT=b scripts/run_all.sh      # cau hinh nho (kiem tra M3: chay tron tren may do cuoi)
#   EXPS="E2 E4 E7" scripts/run_all.sh   # chi chay mot so thi nghiem histogram
# Cac bien ghi de cua run_hist.sh (N_LIST, T_LIST, RUNS, P, ...) dung chung cho moi thi nghiem.
# Mot thi nghiem loi thi van chay tiep cac thi nghiem sau; cuoi cung liet ke cai loi, thoat ma 1.
set -uo pipefail
cd "$(dirname "$0")/.."

make -s || exit 1
FAILED=""

for e in ${EXPS:-E1 E2 E3 E4 E5 E7}; do
  echo "==== $e ====" >&2
  bash scripts/run_hist.sh "$e" || FAILED="$FAILED $e"
done

if [ -n "${EXPS:-}" ]; then
  :   # EXPS chi chon thi nghiem histogram; muon E6 thi chay scripts/run_dot.sh
elif [ -f scripts/run_dot.sh ]; then
  echo "==== E6 ====" >&2
  bash scripts/run_dot.sh || FAILED="$FAILED E6"
else
  echo "Bo qua E6: chua co scripts/run_dot.sh" >&2
fi

if [ -n "$FAILED" ]; then
  echo "LOI o:$FAILED (xem thong bao phia tren; chay lai rieng bang scripts/run_hist.sh <E>)" >&2
  exit 1
fi
echo "Xong tat ca thi nghiem" >&2
