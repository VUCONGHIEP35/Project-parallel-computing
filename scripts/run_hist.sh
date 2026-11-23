#!/usr/bin/env bash
# Chay mot thi nghiem histogram (KE_HOACH muc 4), ghi CSV tho.
#
#   scripts/run_hist.sh E2              # lo chinh tren may do cuoi -> results/raw/
#   PILOT=b scripts/run_hist.sh E2      # pilot nho tren may minh   -> results/pilot/b/
#
# Thi nghiem: E1 (C), E2 (B), E3 (A), E4 (B), E5 (A). E7 cho hist_local_nopad.
# Ghi de bang bien moi truong (danh sach cach nhau bang dau cach):
#   N_LIST     vd "1e6 1e7"        BINS_LIST  vd "16 256"       T_LIST  vd "1 2 4 8"
#   RUNS       so lan do (mac dinh 7, pilot 3)
#   P          so loi vat ly, dung lam T cho E2/E3/E4 (mac dinh doc tu lscpu)
#   IMG_LIST   canh anh vuong cho E5, vd "1024 4096"
#   DATA_DIR   noi de anh PGM cua E5 (mac dinh ~/data)
#   MAX_MB     bo qua cau hinh can nhieu bo nho hon (mac dinh 2500)
#   BIN, OUT_DIR  thu muc chua hist_bench / noi ghi CSV
# Yeu cau: da chay `make` (dung build/release/bin/hist_bench, gen_data).
set -euo pipefail
cd "$(dirname "$0")/.."

EXP=${1:-}
case "$EXP" in E1|E2|E3|E4|E5|E7) ;; *)
  sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac

BIN=${BIN:-build/release/bin}
BENCH=$BIN/hist_bench
[ -x "$BENCH" ] || { echo "Chua co $BENCH, chay make truoc" >&2; exit 1; }

# Bien OpenMP khi do (KE_HOACH muc 6.4). Ghi de duoc tu ngoai.
export OMP_PROC_BIND=${OMP_PROC_BIND:-close}
export OMP_PLACES=${OMP_PLACES:-cores}
export OMP_DYNAMIC=false

NPROC=$(nproc)
if [ -z "${P:-}" ]; then
  P=$(lscpu -p=CORE,SOCKET 2>/dev/null | grep -v '^#' | sort -u | wc -l || true)
  [ "${P:-0}" -ge 1 ] || P=$NPROC
fi

# Day thread: 1, 2, 4, 6, 8, 12, ... den nproc, them P va nproc
t_sweep() {
  local t out=""
  for t in 1 2 4 6 8 12 14 16 20 24 32 48 64 $P $NPROC; do
    [ "$t" -le "$NPROC" ] && out="$out $t"
  done
  echo $out | tr ' ' '\n' | sort -nu | tr '\n' ' '
}

ALL_BINS="16 256 4096 65536 1048576"
if [ -n "${PILOT:-}" ]; then
  OUT_DIR=${OUT_DIR:-results/pilot/$PILOT}
  RUNS=${RUNS:-3}
  BIG_N=1e7                     # thay cho N = 10^8
  DEF_N="1e6 1e7"
  DEF_IMG="1024 2048"
else
  OUT_DIR=${OUT_DIR:-results/raw}
  RUNS=${RUNS:-7}
  BIG_N=1e8
  DEF_N="1e6 1e7 1e8"
  DEF_IMG="1024 4096 16384"
fi

csv() { echo $* | tr ' ' ','; }

ARGS=()
case "$EXP" in
  E1)  # speedup/efficiency theo T va N
    ARGS=(--dist uniform,zipf1.5 --n "$(csv ${N_LIST:-$DEF_N})" --bins "$(csv ${BINS_LIST:-256})"
          --threads "$(csv ${T_LIST:-$(t_sweep)})"
          --variants hist_seq,hist_atomic,hist_critical,hist_local_serial,hist_local_parmerge,hist_omp_reduce) ;;
  E2)  # so bin, diem giao atomic va cuc bo
    ARGS=(--dist uniform --n "$(csv ${N_LIST:-$DEF_N})" --bins "$(csv ${BINS_LIST:-$ALL_BINS})"
          --threads "$(csv ${T_LIST:-$P})"
          --variants hist_seq,hist_atomic,hist_local_serial,hist_local_parmerge,hist_omp_reduce) ;;
  E3)  # tranh chap theo phan phoi
    ARGS=(--dist uniform,normal,zipf1.1,zipf1.5,onebin,sorted --n "$(csv ${N_LIST:-$BIG_N})"
          --bins "$(csv ${BINS_LIST:-16 256 65536})" --threads "$(csv ${T_LIST:-$P})"
          --variants hist_seq,hist_atomic,hist_local_parmerge,hist_omp_reduce) ;;
  E4)  # tach pha
    ARGS=(--dist uniform --n "$(csv ${N_LIST:-$BIG_N})" --bins "$(csv ${BINS_LIST:-$ALL_BINS})"
          --threads "$(csv ${T_LIST:-$P})"
          --variants hist_local_serial,hist_local_parmerge,hist_omp_reduce) ;;
  E5)  # anh: doc PGM (t_io), doi x = p/256 (vao t_init)
    DATA_DIR=${DATA_DIR:-$HOME/data}
    mkdir -p "$DATA_DIR"
    imgs=""
    for s in ${IMG_LIST:-$DEF_IMG}; do
      f=$DATA_DIR/img$s.pgm
      [ -f "$f" ] || "$BIN/gen_data" image "$s" "$s" 1 "$f" >&2
      imgs="$imgs $f"
    done
    ARGS=(--image "$(csv $imgs)" --bins "$(csv ${BINS_LIST:-256})"
          --threads "$(csv ${T_LIST:-$(t_sweep)})"
          --variants hist_seq,hist_atomic,hist_local_parmerge) ;;
  E7)  # padding histogram cuc bo: can phuong an hist_local_nopad
    if ! "$BENCH" --list | grep -qx hist_local_nopad; then
      echo "E7: chua co hist_local_nopad trong hist_bench" >&2; exit 1
    fi
    ARGS=(--dist uniform --n "$(csv ${N_LIST:-$BIG_N})" --bins "$(csv ${BINS_LIST:-256})"
          --threads "$(csv ${T_LIST:-$P})" --variants hist_local_parmerge,hist_local_nopad) ;;
esac

mkdir -p "$OUT_DIR"
STAMP=$(date +%Y%m%d-%H%M%S)
OUT=$OUT_DIR/${EXP}_${STAMP}.csv

if [ -z "${PILOT:-}" ]; then
  # Lo chinh: ghi moi truong canh CSV (KE_HOACH muc 6.3, quy tac 5)
  [ -z "$(git status --porcelain --untracked-files=no 2>/dev/null)" ] ||
    echo "CHU Y: working tree co thay doi chua commit; lo chinh phai build tu commit da ghi" >&2
  bash scripts/collect_env.sh "$OUT_DIR/${EXP}_${STAMP}.env.json" >&2
fi

echo "$EXP: P=$P nproc=$NPROC RUNS=$RUNS OMP_PLACES=$OMP_PLACES OMP_PROC_BIND=$OMP_PROC_BIND" >&2
echo "$BENCH ${ARGS[*]} --runs $RUNS" >&2
"$BENCH" "${ARGS[@]}" --runs "$RUNS" --max-mb "${MAX_MB:-2500}" > "$OUT"
echo "Xong: $OUT ($(($(wc -l < "$OUT") - 1)) dong)" >&2
