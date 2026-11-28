#!/usr/bin/env bash
# Kiem tra moi truong truoc khi build/do (KE_HOACH muc 6.2). Chay trong WSL2/Ubuntu, tai goc repo:
#   scripts/check_env.sh            # kiem tra nhanh (~10 giay)
#   scripts/check_env.sh --full     # them: make test va make debug cua repo
# Moi muc in [ OK ], [WARN] hoac [FAIL]. Co [FAIL] -> ma thoat 1 (chua build/do duoc).
# [WARN] van build duoc, nhung so do co the khong dai dien: doc ghi chu va ghi vao docs/machine*.md.
# Phia Windows (power mode, sac, .wslconfig) xem scripts/check_windows.ps1.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
ROOT=$(pwd)

FULL=false
case "${1:-}" in
  --full) FULL=true ;;
  "") ;;
  *) sed -n '2,7p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac

NOK=0; NWARN=0; NFAIL=0
ok()   { printf '  [ OK ] %s\n' "$*"; NOK=$((NOK+1)); }
warn() { printf '  [WARN] %s\n' "$*"; NWARN=$((NWARN+1)); }
fail() { printf '  [FAIL] %s\n' "$*"; NFAIL=$((NFAIL+1)); }
note() { printf '         %s\n' "$*"; }
head_() { printf '\n== %s\n' "$*"; }
have() { command -v "$1" >/dev/null 2>&1; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# Bien dich + chay mot chuong trinh C nho. $1 = ten, $2 = CC, $3.. = co; ma nguon doc tu stdin.
# Tra ve 0 neu chay ra ma 0; output chuong trinh nam o $TMP/$1.out, loi bien dich o $TMP/$1.cc.
try_c() {
  local name=$1 cc=$2; shift 2
  cat > "$TMP/$name.c"
  "$cc" "$@" "$TMP/$name.c" -o "$TMP/$name" > "$TMP/$name.cc" 2>&1 || return 2
  "$TMP/$name" > "$TMP/$name.out" 2>&1
}

IS_WSL=false
grep -qi microsoft /proc/version 2>/dev/null && IS_WSL=true

# ---------------------------------------------------------------------------
head_ "He dieu hanh"
OS_NAME=$(. /etc/os-release 2>/dev/null; echo "${PRETTY_NAME:-khong ro}")
OS_VER=$(. /etc/os-release 2>/dev/null; echo "${VERSION_ID:-0}")
if [ "$(uname -s)" != Linux ]; then
  fail "Khong phai Linux ($(uname -s)). Nhom build va do trong WSL2 Ubuntu 24.04"
elif [ "${OS_VER%%.*}" -ge 24 ] 2>/dev/null; then
  ok "$OS_NAME, kernel $(uname -r)"
else
  warn "$OS_NAME: ke hoach dung Ubuntu 24.04 tro len"
fi
if $IS_WSL; then
  if grep -qi 'WSL2' /proc/version || [ -d /run/WSL ] || uname -r | grep -qi wsl2; then
    ok "WSL2"
  else
    warn "Co ve la WSL1 (khong co kernel rieng): OpenMP/perf khong dai dien. Doi: wsl --set-version <distro> 2"
  fi
fi

# ---------------------------------------------------------------------------
head_ "Vi tri repo"
case "$ROOT" in
  /mnt/[a-z]/*|/mnt/[a-z])
    warn "Repo nam trong $ROOT (o dia Windows qua 9P): build va doc file cham hon nhieu lan, t_io sai lech"
    note "Clone lai vao ~/projects: cd ~ && mkdir -p projects && cd projects && git clone <url>" ;;
  *) ok "Repo o $ROOT (filesystem Linux)" ;;
esac

# ---------------------------------------------------------------------------
head_ "Git"
if have git; then
  ok "$(git --version)"
  AC=$(git config --get core.autocrlf || true)
  case "$AC" in
    true) warn "core.autocrlf=true: script .sh co the bi doi sang CRLF -> loi \$'\\r'. Sua: git config --global core.autocrlf input" ;;
    *)    ok "core.autocrlf=${AC:-<chua dat>} (dung trong WSL: input hoac de trong)" ;;
  esac
  # File .sh/.c trong working tree dang co CRLF thi bash/gcc se loi o may nay
  CRLF=$(git ls-files -z -- '*.sh' '*.c' '*.h' 'Makefile' 2>/dev/null | xargs -0 -r grep -lI $'\r' 2>/dev/null | head -5)
  if [ -n "$CRLF" ]; then
    fail "File co CRLF trong working tree: $(echo $CRLF)"
    note "Sua: git config --global core.autocrlf input, roi sed -i 's/\\r\$//' <tung file tren>"
  else
    ok "Script va ma nguon dung LF"
  fi
  [ -n "$(git config --get user.name || true)" ] && [ -n "$(git config --get user.email || true)" ] \
    && ok "user.name/user.email da dat ($(git config --get user.name))" \
    || warn "Chua dat git user.name/user.email: git config --global user.name \"Ten\"; ... user.email \"...\""
else
  fail "Khong co git: sudo apt install -y git"
fi

# ---------------------------------------------------------------------------
head_ "Trinh bien dich va Make"
if have gcc; then
  GCC_V=$(gcc -dumpfullversion 2>/dev/null || gcc -dumpversion)
  if [ "${GCC_V%%.*}" -ge 13 ]; then ok "gcc $GCC_V"
  else warn "gcc $GCC_V: ke hoach dung GCC 13 tro len (so do khac phien ban khong so sanh truc tiep)"; fi
else
  fail "Khong co gcc: sudo apt install -y build-essential"
fi
if have make; then
  MK=$(make --version | head -1)
  case "$MK" in "GNU Make 4"*) ok "$MK" ;; *) warn "$MK: Makefile viet cho GNU Make 4.x" ;; esac
else
  fail "Khong co make: sudo apt install -y build-essential"
fi

if have gcc; then
  # C11 + aligned_alloc (compat.h dung aligned_alloc tren Linux)
  if try_c align gcc -std=c11 -O2 -Wall -Werror <<'EOF'
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    void *p = aligned_alloc(64, 64 * 1000);
    if (!p || ((uintptr_t)p % 64) != 0) return 1;
    free(p);
    printf("%ld\n", (long)__STDC_VERSION__);
    return 0;
}
EOF
  then ok "C11 (__STDC_VERSION__ $(cat "$TMP/align.out")) va aligned_alloc can le 64 byte"
  else fail "C11/aligned_alloc khong chay: $(head -3 "$TMP/align.cc" "$TMP/align.out" 2>/dev/null | tr '\n' ' ')"; fi
fi

# ---------------------------------------------------------------------------
head_ "OpenMP"
if have gcc; then
  OMPV=$(echo | gcc -fopenmp -dM -E - 2>/dev/null | sed -n 's/^#define _OPENMP //p')
  if [ -z "$OMPV" ]; then
    fail "gcc -fopenmp khong dinh nghia _OPENMP: sudo apt install -y libgomp1 (hoac build-essential)"
  elif [ "$OMPV" -ge 201511 ]; then
    ok "_OPENMP $OMPV (>= 4.5, co reduction tren mang hist[:B])"
  else
    fail "_OPENMP $OMPV < 201511: khong build duoc V4 reduction(+:hist[:B])"
  fi

  # Chay that: so thread, reduction tren mang, atomic. Dung nproc thread.
  NPROC=$(nproc)
  if OMP_NUM_THREADS=$NPROC try_c omp gcc -std=c11 -O2 -fopenmp <<'EOF'
#include <omp.h>
#include <stdio.h>
int main(void) {
    enum { N = 1000000, B = 16 };
    unsigned h[B] = {0}, a = 0;
    int team = 0;
    #pragma omp parallel
    {
        #pragma omp single
        team = omp_get_num_threads();
    }
    #pragma omp parallel for reduction(+:h[:B])
    for (int i = 0; i < N; i++) h[i % B]++;
    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        #pragma omp atomic update
        a++;
    }
    unsigned s = 0;
    for (int b = 0; b < B; b++) s += h[b];
    printf("%d %u %u\n", team, s, a);
    return !(s == N && a == N);
}
EOF
  then
    read -r TEAM _ _ < "$TMP/omp.out"
    if [ "$TEAM" = "$NPROC" ]; then ok "Vung song song chay $TEAM thread; reduction mang va atomic cho ket qua dung"
    else warn "Yeu cau $NPROC thread nhung vung song song chi co $TEAM"; fi
  else
    fail "Chuong trinh OpenMP nho khong chay dung: $(head -3 "$TMP/omp.cc" "$TMP/omp.out" 2>/dev/null | tr '\n' ' ')"
  fi
fi

OMPVARS=$(env | grep '^OMP_' | sort | tr '\n' ' ')
if [ -n "$OMPVARS" ]; then
  warn "Bien OMP_ dang dat san trong shell: $OMPVARS"
  note "run_*.sh tu dat OMP_PLACES/OMP_PROC_BIND; OMP_NUM_THREADS dat san se lam lech mac dinh cua hist_bench"
else
  ok "Khong co bien OMP_ dat san (run_*.sh tu dat khi do)"
fi

# ---------------------------------------------------------------------------
head_ "Sanitizer (make debug, make tsan)"
if have gcc; then
  if try_c asan gcc -std=c11 -O0 -g -fopenmp -fsanitize=address,undefined -fno-omit-frame-pointer <<'EOF'
#include <stdlib.h>
int main(void) {
    int *p = malloc(8 * sizeof *p), s = 0;
    #pragma omp parallel for reduction(+:s)
    for (int i = 0; i < 8; i++) { p[i] = i; s += i; }
    free(p);
    return s != 28;
}
EOF
  then ok "ASan + UBSan chay duoc (make debug)"
  elif [ $? -eq 2 ]; then
    fail "Khong bien dich duoc voi -fsanitize=address,undefined: $(head -2 "$TMP/asan.cc" | tr '\n' ' ')"
  else
    if grep -q 'DEADLYSIGNAL\|unexpected memory mapping\|Shadow memory range' "$TMP/asan.out" 2>/dev/null; then
      fail "ASan chet ngay khi chay (kernel moi dat vm.mmap_rnd_bits qua cao)"
      note "Sua tam: sudo sysctl vm.mmap_rnd_bits=28   (hien tai: $(cat /proc/sys/vm/mmap_rnd_bits 2>/dev/null))"
    else
      fail "Chuong trinh ASan nho bao loi: $(head -3 "$TMP/asan.out" | tr '\n' ' ')"
    fi
  fi
fi

TSAN_CC=""
for c in clang-18 clang; do have "$c" && { TSAN_CC=$c; break; }; done
if [ -z "$TSAN_CC" ]; then
  warn "Khong co clang: make tsan chi chay tren CI. Cai: sudo apt install -y clang-18 libomp-18-dev"
else
  ARCHER=$(ls /usr/lib/llvm-*/lib/libarcher.so 2>/dev/null | head -1)
  if OMP_TOOL_LIBRARIES="$ARCHER" TSAN_OPTIONS="ignore_noninstrumented_modules=1" \
     try_c tsan "$TSAN_CC" -std=c11 -O1 -g -fopenmp -fsanitize=thread <<'EOF'
int main(void) {
    int s = 0;
    #pragma omp parallel for reduction(+:s)
    for (int i = 0; i < 1000; i++) s += i;
    return s != 499500;
}
EOF
  then
    ok "$TSAN_CC -fsanitize=thread + OpenMP chay duoc (make tsan TSAN_CC=$TSAN_CC)"
    [ -n "$ARCHER" ] || warn "Khong thay libarcher.so: TSan co the bao race gia trong runtime OpenMP"
  elif [ $? -eq 2 ]; then
    if grep -q 'libclang_rt.tsan' "$TMP/tsan.cc"; then
      warn "$TSAN_CC thieu runtime TSan: sudo apt install -y libclang-rt-18-dev"
    elif grep -q 'omp.h\|lomp' "$TMP/tsan.cc"; then
      warn "$TSAN_CC thieu libomp: sudo apt install -y libomp-18-dev"
    else
      warn "$TSAN_CC khong build duoc TSan + OpenMP: $(head -2 "$TMP/tsan.cc" | tr '\n' ' ')"
    fi
  else
    warn "Chuong trinh TSan nho bao loi; xem lai truoc khi tin ket qua make tsan tren may nay"
    note "$(grep -m2 -E 'WARNING|FATAL|ERROR' "$TMP/tsan.out" | tr '\n' ' ')"
  fi
fi

# ---------------------------------------------------------------------------
head_ "CPU"
CPU=$(lscpu 2>/dev/null | sed -n 's/^Model name: *//p' | head -1)
NPROC=$(nproc)
CORES=$(lscpu -p=CORE,SOCKET 2>/dev/null | grep -v '^#' | sort -u | wc -l)
TPC=$(lscpu 2>/dev/null | sed -n 's/^Thread(s) per core: *//p')
ok "${CPU:-khong ro}: nproc=$NPROC, core (Linux thay)=$CORES, ${TPC:-?} thread/core"

# CPU lai (P-core + E-core). Linux that co /sys/devices/cpu_core va cpu_atom; WSL thi doan theo ten CPU.
HYBRID=false
if [ -d /sys/devices/cpu_atom ] && [ -d /sys/devices/cpu_core ]; then
  HYBRID=true
  note "P-core CPU: $(cat /sys/devices/cpu_core/cpus 2>/dev/null)   E-core CPU: $(cat /sys/devices/cpu_atom/cpus 2>/dev/null)"
elif echo "$CPU" | grep -qE '1[2-4]th Gen Intel|Core\(TM\) Ultra|Core Ultra|Intel\(R\) Core\(TM\) [3579] [12][0-9]{2}'; then
  HYBRID=true
fi
if $HYBRID; then
  warn "Co ve la CPU lai (P-core + E-core): speedup se gay khuc khi T vuot so luong P-core; ghi ro trong bao cao"
  note "Ve thi nghiem: P (so loi vat ly) lay tu lscpu; E-core cham hon nen hieu suat E = S/T se giam o T lon"
fi

if $IS_WSL; then
  WINCORES=""; WINLOGICAL=""
  if have powershell.exe; then
    W=$(timeout 30 powershell.exe -NoProfile -NonInteractive -Command \
        '$c = Get-CimInstance Win32_Processor; ($c | Measure-Object NumberOfCores -Sum).Sum; ($c | Measure-Object NumberOfLogicalProcessors -Sum).Sum' \
        2>/dev/null | tr -d '\r')
    WINCORES=$(echo "$W" | sed -n 1p); WINLOGICAL=$(echo "$W" | sed -n 2p)
  fi
  if [ -n "$WINCORES" ]; then
    if [ "$WINCORES" != "$CORES" ]; then
      warn "Topology WSL la ao: Windows bao $WINCORES core/$WINLOGICAL thread, WSL bao $CORES core/$NPROC thread"
      note "OMP_PLACES=cores khong ghim dung vao core that. Bao cao ghi ca hai so; P dung trong script = $CORES"
    else
      ok "Topology WSL khop Windows ($WINCORES core, $WINLOGICAL thread)"
    fi
    [ -n "$WINLOGICAL" ] && [ "$NPROC" -lt "$WINLOGICAL" ] && \
      warn "WSL chi thay $NPROC/$WINLOGICAL CPU logic (.wslconfig processors=?): khong do duoc het may"
  else
    warn "Khong goi duoc powershell.exe de doi chieu topology voi Windows; chay scripts/check_windows.ps1 ben Windows"
  fi
fi

GOV=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || true)
if [ -n "$GOV" ]; then
  [ "$GOV" = performance ] && ok "CPU governor: performance" \
    || warn "CPU governor: $GOV (ke hoach muc 3.4: dat performance neu duoc: sudo cpupower frequency-set -g performance)"
elif $IS_WSL; then
  note "WSL khong dieu khien tan so CPU: chon power mode Best performance va cam sac ben Windows"
fi

# ---------------------------------------------------------------------------
head_ "Bo nho va dia"
MEM_KB=$(awk '/^MemTotal/ {print $2}' /proc/meminfo)
AVL_KB=$(awk '/^MemAvailable/ {print $2}' /proc/meminfo)
MEM_G=$(awk -v k="$MEM_KB" 'BEGIN { printf "%.1f", k / 1048576 }')
AVL_G=$(awk -v k="$AVL_KB" 'BEGIN { printf "%.1f", k / 1048576 }')
# Lo chinh: N = 10^8 float = 381 MiB; anh 16384^2 = 256 MiB uint8 + 1 GiB float (t_conv) ~ 1.3 GiB
if [ "$AVL_KB" -ge $((3 * 1048576)) ]; then
  ok "RAM ${MEM_G} GiB, con trong ${AVL_G} GiB (lo chinh can ~1.5 GiB, lon nhat la anh 16384^2 cua E5)"
elif [ "$AVL_KB" -ge $((3 * 1048576 / 2)) ]; then
  warn "RAM ${MEM_G} GiB, con trong ${AVL_G} GiB: vua du lo chinh; dong app khac, han che N > 10^8 (MAX_MB)"
  $IS_WSL && note "Tang RAM cho WSL: them memory=8GB vao %USERPROFILE%\\.wslconfig roi wsl --shutdown"
else
  fail "Con trong ${AVL_G} GiB < 1.5 GiB: E3 (N = 10^8) va E5 (anh 16384^2) se bi swap hoac bi giet"
  $IS_WSL && note "Tang RAM cho WSL: them memory=8GB vao %USERPROFILE%\\.wslconfig roi wsl --shutdown"
fi
SWAP_KB=$(awk '/^SwapTotal/ {print $2}' /proc/meminfo)
[ "${SWAP_KB:-0}" -gt 0 ] && note "Swap $(awk -v k="$SWAP_KB" 'BEGIN { printf "%.1f", k / 1048576 }') GiB: neu do ma bi swap thi so do vo nghia, theo doi free -h"

DATA_DIR=${DATA_DIR:-$HOME/data}
[ -d "$DATA_DIR" ] || DATA_DIR=$HOME   # chua co ~/data thi xem o chua no
FREE_KB=$(df -Pk "$DATA_DIR" 2>/dev/null | awk 'NR == 2 {print $4}')
if [ -n "$FREE_KB" ]; then
  FREE_G=$(awk -v k="$FREE_KB" 'BEGIN { printf "%.1f", k / 1048576 }')
  if [ "$FREE_KB" -ge $((2 * 1048576)) ]; then ok "Dia trong o $DATA_DIR: ${FREE_G} GiB (anh E5 + CSV can < 1 GiB)"
  else warn "Dia trong o $DATA_DIR chi ${FREE_G} GiB: anh E5 (16384^2 = 256 MiB) va file .bin co the khong du cho"; fi
fi

# ---------------------------------------------------------------------------
head_ "Python (analysis/)"
if have python3; then
  PYV=$(python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])')
  case "$PYV" in 3.1[2-9]|3.[2-9]*) ok "Python $PYV" ;; *) warn "Python $PYV: ke hoach dung 3.12" ;; esac
  MISSING=$(python3 - <<'EOF' 2>/dev/null
import importlib
miss = []
for m in ("numpy", "pandas", "matplotlib"):
    try: importlib.import_module(m)
    except Exception: miss.append(m)
print(" ".join(miss))
EOF
)
  if [ -z "$MISSING" ]; then ok "numpy, pandas, matplotlib co san"
  else warn "Thieu goi Python: $MISSING (chi can cho may phan tich): python3 -m venv .venv && . .venv/bin/activate && pip install $MISSING"; fi
else
  warn "Khong co python3: chi can cho phan tich (analysis/summarize.py)"
fi

# ---------------------------------------------------------------------------
if $FULL; then
  head_ "Build va test cua repo (--full)"
  if make -s -j"$NPROC" > "$TMP/make.log" 2>&1; then ok "make (release) thanh cong"
  else fail "make loi:"; tail -15 "$TMP/make.log" | sed 's/^/         /'; fi
  if make -s test > "$TMP/test.log" 2>&1; then ok "make test: $(tail -1 "$TMP/test.log")"
  else fail "make test truot:"; tail -15 "$TMP/test.log" | sed 's/^/         /'; fi
  if UBSAN_OPTIONS=halt_on_error=1 make -s debug > "$TMP/debug.log" 2>&1; then ok "make debug: $(tail -1 "$TMP/debug.log")"
  else fail "make debug truot:"; tail -15 "$TMP/debug.log" | sed 's/^/         /'; fi
fi

# ---------------------------------------------------------------------------
printf '\nKet qua: %d OK, %d WARN, %d FAIL\n' "$NOK" "$NWARN" "$NFAIL"
if [ "$NFAIL" -gt 0 ]; then
  echo "Con FAIL: sua cac muc tren roi chay lai truoc khi build/do."
  exit 1
fi
[ "$NWARN" -gt 0 ] && echo "Co WARN: build va test duoc; ghi cac canh bao lien quan den do dac vao docs/machine*.md."
exit 0
