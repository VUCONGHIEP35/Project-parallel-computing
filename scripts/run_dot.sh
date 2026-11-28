#!/usr/bin/env bash
# Chay thi nghiem E6 - tich vo huong, ghi CSV tho. scripts/run_all.sh tu goi file nay.
#
#   scripts/run_dot.sh              # lo chinh tren may do cuoi -> results/raw/E6_<gio>.csv
#   PILOT=b scripts/run_dot.sh      # pilot nho tren may minh   -> results/pilot/b/
#
# Tham so E6 (chua doi chieu KE_HOACH): moi phuong an cua dot_bench, day thread giong run_hist.sh,
# n chon theo tang bo nho (2 vector = 8n byte), da quet thu tren may C:
#   1e4 (80 KB)  chi phi mo vung song song lan at      1e5 (800 KB) speedup dang tang
#   3e5 (2,4 MB) dinh: chia vao L2 rieng moi core        1e6 (8 MB)   L3
#   1e7 (80 MB), 1e8 (800 MB) RAM; 1e6, 1e7, 1e8 trung voi E1 de so sanh voi histogram.
# Ghi de bang bien moi truong (danh sach cach nhau bang dau cach):
#   N_LIST     vd "1e6 1e8"        T_LIST  vd "1 2 4 8"        VARIANTS  vd "dot_seq dot_partial"
#   RUNS       so lan do (mac dinh 7, pilot 3)
#   P          so loi vat ly, dua them vao day thread (mac dinh doc tu lscpu)
#   MIN_TIME   giay toi thieu cho moi lan do (mac dinh 0.05)
#   MAX_MB     bo qua n can nhieu bo nho hon (mac dinh 2500)
#   BIN, OUT_DIR  thu muc chua dot_bench / noi ghi CSV
# Yeu cau: da chay `make` (dung build/release/bin/dot_bench).
set -euo pipefail
cd "$(dirname "$0")/.."

BIN=${BIN:-build/release/bin}
BENCH=$BIN/dot_bench
[ -x "$BENCH" ] || { echo "Chua co $BENCH, chay make truoc" >&2; exit 1; }

# Bien OpenMP khi do: giong run_hist.sh (KE_HOACH muc 6.4). Ghi de duoc tu ngoai.
export OMP_PROC_BIND=${OMP_PROC_BIND:-close}
export OMP_PLACES=${OMP_PLACES:-cores}
export OMP_DYNAMIC=false

NPROC=$(nproc)
if [ -z "${P:-}" ]; then
  P=$(lscpu -p=CORE,SOCKET 2>/dev/null | grep -v '^#' | sort -u | wc -l || true)
  [ "${P:-0}" -ge 1 ] || P=$NPROC
fi

# Day thread: 1, 2, 4, 6, 8, 12, ... den nproc, them P va nproc (giong run_hist.sh)
t_sweep() {
  local t out=""
  for t in 1 2 4 6 8 12 14 16 20 24 32 48 64 $P $NPROC; do
    [ "$t" -le "$NPROC" ] && out="$out $t"
  done
  echo $out | tr ' ' '\n' | sort -nu | tr '\n' ' '
}

if [ -n "${PILOT:-}" ]; then
  OUT_DIR=${OUT_DIR:-results/pilot/$PILOT}
  RUNS=${RUNS:-3}
  DEF_N="1e4 1e5 3e5 1e6 1e7"   # bo N = 10^8 cho nhanh
else
  OUT_DIR=${OUT_DIR:-results/raw}
  RUNS=${RUNS:-7}
  DEF_N="1e4 1e5 3e5 1e6 1e7 1e8"
fi

csv() { echo $* | tr ' ' ','; }

ARGS=(--n "$(csv ${N_LIST:-$DEF_N})" --threads "$(csv ${T_LIST:-$(t_sweep)})"
      --runs "$RUNS" --min-time "${MIN_TIME:-0.05}" --max-mb "${MAX_MB:-2500}")
[ -n "${VARIANTS:-}" ] && ARGS+=(--variants "$(csv $VARIANTS)")

mkdir -p "$OUT_DIR"
STAMP=$(date +%Y%m%d-%H%M%S)
OUT=$OUT_DIR/E6_${STAMP}.csv

if [ -z "${PILOT:-}" ]; then
  # Lo chinh: ghi moi truong canh CSV (KE_HOACH muc 6.3, quy tac 5)
  [ -z "$(git status --porcelain --untracked-files=no 2>/dev/null)" ] ||
    echo "CHU Y: working tree co thay doi chua commit; lo chinh phai build tu commit da ghi" >&2
  bash scripts/collect_env.sh "$OUT_DIR/E6_${STAMP}.env.json" >&2
fi

echo "E6: P=$P nproc=$NPROC RUNS=$RUNS OMP_PLACES=$OMP_PLACES OMP_PROC_BIND=$OMP_PROC_BIND" >&2
echo "$BENCH ${ARGS[*]}" >&2
"$BENCH" "${ARGS[@]}" > "$OUT"
echo "Xong: $OUT ($(($(wc -l < "$OUT") - 1)) dong)" >&2
