#!/usr/bin/env bash
# Ghi moi truong do ra JSON (KE_HOACH muc 6.5). Chay tren may do cuoi truoc moi lo.
#   scripts/collect_env.sh                   # -> results/env.json
#   scripts/collect_env.sh duong/dan.json
# Phia Windows lay qua powershell.exe neu chay trong WSL; thieu cong cu nao thi ghi "".
set -uo pipefail
cd "$(dirname "$0")/.."
OUT=${1:-results/env.json}
mkdir -p "$(dirname "$OUT")"

# Chuoi JSON: bo ky tu dieu khien, escape \ " xuong dong, tab
j() {
  local s
  s=$(printf '%s' "$1" | tr -d '\000-\010\013\014\016-\037\r')
  s=${s//\\/\\\\}; s=${s//\"/\\\"}; s=${s//$'\n'/\\n}; s=${s//$'\t'/\\t}
  printf '"%s"' "$s"
}
run() { "$@" 2>/dev/null || true; }

IS_WSL=false
grep -qi microsoft /proc/version 2>/dev/null && IS_WSL=true

# ---- Windows (chi khi co powershell.exe) ----
WIN=""
if $IS_WSL && command -v powershell.exe >/dev/null; then
  WIN=$(timeout 60 powershell.exe -NoProfile -NonInteractive -Command '
    $os = Get-CimInstance Win32_OperatingSystem
    $cpu = Get-CimInstance Win32_Processor
    "os=" + $os.Caption + " " + $os.Version + " (build " + $os.BuildNumber + ")"
    "cpu=" + ($cpu | Select-Object -First 1).Name
    "cores=" + ($cpu | Measure-Object NumberOfCores -Sum).Sum
    "logical=" + ($cpu | Measure-Object NumberOfLogicalProcessors -Sum).Sum
    "ram_gib=" + [math]::Round($os.TotalVisibleMemorySize / 1MB, 1)
    "power_scheme=" + ((powercfg /getactivescheme) -join " ")
    $b = Get-CimInstance Win32_Battery
    if ($b) { "on_ac=" + ($b.BatteryStatus -eq 2) + " (BatteryStatus " + $b.BatteryStatus + ")" } else { "on_ac=no battery" }
  ' 2>/dev/null | tr -d '\r' || true)
fi
win() { printf '%s\n' "$WIN" | sed -n "s/^$1=//p" | head -1; }

WSLCONFIG=""
if $IS_WSL; then
  UP=$(run cmd.exe /c 'echo %USERPROFILE%' | tr -d '\r')
  [ -n "$UP" ] && f=$(run wslpath "$UP")/.wslconfig && [ -f "$f" ] && WSLCONFIG=$(cat "$f")
fi

CORES=$(run lscpu -p=CORE,SOCKET | grep -v '^#' | sort -u | wc -l)
OPENMP=$(echo | run gcc -fopenmp -dM -E - | sed -n 's/^#define _OPENMP //p')
PYPKG=$(run python3 -c '
import importlib
for m in ("numpy", "pandas", "matplotlib"):
    try: print(m, importlib.import_module(m).__version__)
    except Exception: print(m, "khong co")')

{
  echo "{"
  echo "  \"timestamp\": $(j "$(date -u +%Y-%m-%dT%H:%M:%SZ)"),"
  echo "  \"host\": $(j "$(hostname)"),"
  echo "  \"code\": {"
  echo "    \"commit\": $(j "$(run git rev-parse HEAD)"),"
  echo "    \"branch\": $(j "$(run git rev-parse --abbrev-ref HEAD)"),"
  echo "    \"dirty\": $([ -n "$(run git status --porcelain --untracked-files=no)" ] && echo true || echo false)"
  echo "  },"
  echo "  \"windows\": {"
  echo "    \"os\": $(j "$(win os)"),"
  echo "    \"cpu\": $(j "$(win cpu)"),"
  echo "    \"cores\": $(j "$(win cores)"),"
  echo "    \"logical\": $(j "$(win logical)"),"
  echo "    \"ram_gib\": $(j "$(win ram_gib)"),"
  echo "    \"power_scheme\": $(j "$(win power_scheme)"),"
  echo "    \"on_ac\": $(j "$(win on_ac)")"
  echo "  },"
  echo "  \"linux\": {"
  echo "    \"wsl\": $IS_WSL,"
  echo "    \"os\": $(j "$(. /etc/os-release 2>/dev/null; echo "${PRETTY_NAME:-}")"),"
  echo "    \"kernel\": $(j "$(uname -r)"),"
  echo "    \"cpu\": $(j "$(run lscpu | sed -n 's/^Model name: *//p')"),"
  echo "    \"nproc\": $(nproc),"
  echo "    \"cores\": ${CORES:-0},"
  echo "    \"lscpu\": $(j "$(run lscpu)"),"
  echo "    \"free\": $(j "$(run free -h)"),"
  echo "    \"wslconfig\": $(j "$WSLCONFIG")"
  echo "  },"
  echo "  \"tools\": {"
  echo "    \"gcc\": $(j "$(run gcc --version | head -1)"),"
  echo "    \"openmp\": $(j "$OPENMP"),"
  echo "    \"make_flags\": $(j "$(run make -s flags)"),"
  echo "    \"omp_env\": $(j "$(env | grep '^OMP_' | sort)"),"
  echo "    \"python\": $(j "$(run python3 --version)"),"
  echo "    \"python_packages\": $(j "$PYPKG")"
  echo "  }"
  echo "}"
} > "$OUT"
echo "Da ghi $OUT"
