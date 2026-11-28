# Kiem tra phia Windows truoc khi do (KE_HOACH muc 6.2): WSL2, distro, RAM/CPU cap cho WSL,
# CPU lai, cam sac, power mode. Phan Linux (gcc, OpenMP, sanitizer) xem scripts/check_env.sh.
#
# Chay tu WSL, tai goc repo:
#   powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(wslpath -w scripts/check_windows.ps1)"
# Hoac tu PowerShell tren Windows (khong can quyen Administrator):
#   powershell -NoProfile -ExecutionPolicy Bypass -File \\wsl.localhost\Ubuntu-24.04\home\<user>\projects\Project-parallel-computing\scripts\check_windows.ps1
#
# Moi muc in [ OK ], [WARN] hoac [FAIL]. Co [FAIL] -> ma thoat 1.
# File chi dung ky tu ASCII de Windows PowerShell 5.1 doc dung (khong can BOM).

$ErrorActionPreference = 'Continue'
$script:nOk = 0; $script:nWarn = 0; $script:nFail = 0

function Ok($m)   { Write-Host "  [ OK ] $m"; $script:nOk++ }
function Warn($m) { Write-Host "  [WARN] $m" -ForegroundColor Yellow; $script:nWarn++ }
function Fail($m) { Write-Host "  [FAIL] $m" -ForegroundColor Red; $script:nFail++ }
function Note($m) { Write-Host "         $m" }
function Head($m) { Write-Host ""; Write-Host "== $m" }

# wsl.exe in UTF-16 neu khong dat WSL_UTF8; dat de doc duoc output
$env:WSL_UTF8 = '1'
function Invoke-Wsl([string[]]$wslArgs) {
    $out = & wsl.exe @wslArgs 2>&1 | Out-String
    return ($out -replace "`0", '').Trim()
}

# ---------------------------------------------------------------------------
Head 'Windows'
$os = Get-CimInstance Win32_OperatingSystem
Ok ("{0} {1} (build {2})" -f $os.Caption, $os.Version, $os.BuildNumber)
$ramGiB = [math]::Round($os.TotalVisibleMemorySize / 1MB, 1)

# ---------------------------------------------------------------------------
Head 'CPU'
$cpus = @(Get-CimInstance Win32_Processor)
$cpuName = ($cpus | Select-Object -First 1).Name.Trim()
$cores = ($cpus | Measure-Object NumberOfCores -Sum).Sum
$logical = ($cpus | Measure-Object NumberOfLogicalProcessors -Sum).Sum
$maxMHz = ($cpus | Select-Object -First 1).MaxClockSpeed
Ok "$cpuName : $cores core, $logical thread, MaxClockSpeed $maxMHz MHz"

# CPU lai Intel: chi P-core co Hyper-Threading nen so thread < 2 x so core (vd 12700H: 14 core, 20 thread)
if ($logical -gt $cores -and $logical -lt 2 * $cores) {
    $p = $logical - $cores
    $e = $cores - $p
    Warn "CPU lai: uoc tinh $p P-core (co HT) + $e E-core. Speedup se gay khuc khi T > $p; ghi vao bao cao"
} elseif ($cpuName -match 'Core\(TM\) Ultra|Core Ultra|1[2-4]th Gen') {
    Warn 'Ten CPU cho thay la CPU lai (P-core + E-core); ghi vao bao cao'
} else {
    Ok 'Khong thay dau hieu CPU lai'
}


# ---------------------------------------------------------------------------
Head 'WSL'
$hasWsl = $null -ne (Get-Command wsl.exe -ErrorAction SilentlyContinue)
$wslMemGiB = $null
$wslProcs = $null
if (-not $hasWsl) {
    Fail 'Khong co wsl.exe. Cai (PowerShell Administrator): wsl --install -d Ubuntu-24.04'
} else {
    $ver = Invoke-Wsl @('--version')
    $line = ($ver -split "`r?`n" | Where-Object { $_ -match 'WSL' } | Select-Object -First 1)
    if ($line -and $line -match '([0-9]+\.[0-9]+\.[0-9]+)') {
        Ok "WSL $($Matches[1]) (ban tu Microsoft Store)"
    } else {
        Warn 'wsl --version khong chay: WSL ban cu (in-box). Cap nhat: wsl --update'
    }

    $list = Invoke-Wsl @('--list', '--verbose')
    $rows = @()
    foreach ($l in ($list -split "`r?`n" | Select-Object -Skip 1)) {
        $t = $l.Trim()
        if ($t -eq '') { continue }
        $isDefault = $t.StartsWith('*')
        $t = $t.TrimStart('*').Trim()
        $parts = $t -split '\s+'
        if ($parts.Count -ge 3) {
            $rows += [pscustomobject]@{ Name = $parts[0]; State = $parts[1]; Version = $parts[-1]; Default = $isDefault }
        }
    }
    $ubuntu = @($rows | Where-Object { $_.Name -match 'Ubuntu' })
    if ($ubuntu.Count -eq 0) {
        Fail 'Chua co distro Ubuntu trong WSL. Cai: wsl --install -d Ubuntu-24.04'
        if (($cpus | Select-Object -First 1).VirtualizationFirmwareEnabled -eq $false) {
            Note 'VirtualizationFirmwareEnabled = False: neu WSL2 khong khoi dong, bat VT-x/AMD-V trong BIOS'
        }
    } else {
        foreach ($d in $ubuntu) {
            $tag = ''
            if ($d.Default) { $tag = ', mac dinh' }
            if ($d.Version -eq '2') {
                Ok "$($d.Name): WSL$($d.Version), $($d.State)$tag"
            } else {
                Fail "$($d.Name) dang la WSL$($d.Version): OpenMP/do dac khong dai dien. Doi: wsl --set-version $($d.Name) 2"
            }
        }
        $def = $rows | Where-Object { $_.Default } | Select-Object -First 1
        if ($def -and $def.Name -notmatch 'Ubuntu') {
            Warn "Distro mac dinh la $($def.Name), khong phai Ubuntu: wsl -s $($ubuntu[0].Name)"
        }
    }

    # .wslconfig: gioi han RAM/CPU cho WSL2. Mac dinh: 50% RAM, moi CPU logic.
    $cfgPath = Join-Path $env:USERPROFILE '.wslconfig'
    $wslMemGiB = [math]::Round($ramGiB / 2, 1)
    $wslProcs = $logical
    if (Test-Path $cfgPath) {
        $cfg = Get-Content $cfgPath -ErrorAction SilentlyContinue
        $memLine = $cfg | Where-Object { $_ -match '^\s*memory\s*=' } | Select-Object -First 1
        $procLine = $cfg | Where-Object { $_ -match '^\s*processors\s*=' } | Select-Object -First 1
        if ($memLine -and $memLine -match '=\s*([0-9.]+)\s*(GB|MB)') {
            $v = [double]$Matches[1]
            if ($Matches[2] -eq 'MB') { $v = $v / 1024 }
            $wslMemGiB = [math]::Round($v, 1)
        }
        if ($procLine -and $procLine -match '=\s*([0-9]+)') { $wslProcs = [int]$Matches[1] }
        Ok ".wslconfig: memory = $wslMemGiB GiB, processors = $wslProcs"
    } else {
        Ok ".wslconfig chua co: WSL dung mac dinh ~$wslMemGiB GiB (50% cua $ramGiB GiB), $wslProcs CPU logic"
    }
    if ($wslMemGiB -lt 3) {
        Fail "WSL chi duoc ~$wslMemGiB GiB RAM: lo chinh (E3 N = 10^8, anh E5 16384^2) can ~1.5 GiB trong"
        Note "Them vao $cfgPath :  [wsl2]  memory=6GB   roi chay: wsl --shutdown"
    } elseif ($wslMemGiB -lt 5) {
        Warn "WSL duoc ~$wslMemGiB GiB RAM: vua du lo chinh; dong app khac khi do"
        Note "Neu can: them memory=6GB vao muc [wsl2] cua $cfgPath roi wsl --shutdown"
    } else {
        Ok "RAM cho WSL ~$wslMemGiB GiB, du cho lo chinh"
    }
    if ($wslProcs -lt $logical) {
        Warn "WSL chi duoc $wslProcs / $logical CPU logic (processors trong .wslconfig): khong do duoc het may"
    }
}

# ---------------------------------------------------------------------------
Head 'Nguon dien va power mode (KE_HOACH muc 3.4)'
$bat = @(Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue)
if ($bat.Count -eq 0) {
    Ok 'Khong co pin (may ban)'
} else {
    $st = ($bat | Select-Object -First 1).BatteryStatus
    $pct = ($bat | Select-Object -First 1).EstimatedChargeRemaining
    # BatteryStatus: 1 = dang xa pin, 2 = cam sac (AC); 6..9 = dang sac
    if ($st -eq 1) {
        Fail "Dang chay bang pin ($pct%): CPU bi ha xung, so do khong dung duoc. Cam sac truoc khi do"
    } else {
        Ok "Dang cam sac (BatteryStatus $st, pin $pct%)"
    }
}

$scheme = (powercfg /getactivescheme) -join ' '
if ($scheme -match '([0-9a-fA-F-]{36})\s*\((.+)\)') {
    $schemeGuid = $Matches[1]; $schemeName = $Matches[2]
    if ($schemeGuid -eq 'a1841308-3541-4fab-bc81-f71556f20b4a') {
        Fail "Power plan: $schemeName (Power saver): CPU bi gioi han, doi sang Balanced/High performance"
    } else {
        Ok "Power plan: $schemeName"
    }
} else {
    Warn "Khong doc duoc power plan: $scheme"
}

# Windows 11: "Power mode" (Settings > System > Power) la overlay tren power plan
$overlayKey = 'HKLM:\SYSTEM\CurrentControlSet\Control\Power\User\PowerSchemes'
$ov = Get-ItemProperty -Path $overlayKey -ErrorAction SilentlyContinue
$overlay = $null
if ($ov) {
    $onAc = ($bat.Count -eq 0) -or ((($bat | Select-Object -First 1).BatteryStatus) -ne 1)
    if ($onAc) { $overlay = $ov.ActiveOverlayAcPowerScheme } else { $overlay = $ov.ActiveOverlayDcPowerScheme }
}
switch ($overlay) {
    'ded574b5-45a0-4f42-8737-46345c09c238' { Ok 'Power mode: Best performance' }
    '961cc777-2547-4f9d-8174-7d86181b8a7a' { Fail 'Power mode: Best power efficiency. Doi sang Best performance (Settings > System > Power)' }
    '00000000-0000-0000-0000-000000000000' { Warn 'Power mode: Balanced. Nen doi sang Best performance khi do (Settings > System > Power)' }
    $null { Note 'Khong doc duoc Power mode (Windows 10 hoac may khong ho tro); tu kiem tra trong Settings' }
    default { Warn "Power mode khong ro (GUID $overlay); tu kiem tra trong Settings > System > Power" }
}

# App nang dang chay lam nhieu so do
$heavy = Get-Process -ErrorAction SilentlyContinue |
    Where-Object { $_.CPU -ne $null } |
    Sort-Object CPU -Descending | Select-Object -First 40 |
    Where-Object { $_.ProcessName -match '^(chrome|msedge|firefox|Teams|ms-teams|Discord|Zoom|obs64|steam|EpicGamesLauncher|OneDrive)$' } |
    Select-Object -ExpandProperty ProcessName -Unique
if ($heavy) {
    Warn ("Dang mo: {0}. Dong truoc khi chay lo do chinh" -f ($heavy -join ', '))
} else {
    Ok 'Khong thay app nang quen thuoc dang chay'
}

# ---------------------------------------------------------------------------
Head 'Git phia Windows (chi de tham khao)'
$git = Get-Command git.exe -ErrorAction SilentlyContinue
if ($git) {
    $ac = (& git.exe config --global --get core.autocrlf) 2>$null
    if (-not $ac) { $ac = '<chua dat>' }
    Ok "git cua Windows co, core.autocrlf = $ac"
    Note 'Repo phai clone va commit tu ben trong WSL (~/projects), khong mo bang git cua Windows'
} else {
    Ok 'Khong co git.exe tren Windows (dung git trong WSL)'
}

# ---------------------------------------------------------------------------
Write-Host ""
Write-Host ("Ket qua: {0} OK, {1} WARN, {2} FAIL" -f $script:nOk, $script:nWarn, $script:nFail)
Write-Host ("Ghi vao docs/machine*.md: CPU {0} ({1} core / {2} thread), RAM {3} GiB, WSL ~{4} GiB" -f $cpuName, $cores, $logical, $ramGiB, $wslMemGiB)
if ($script:nFail -gt 0) {
    Write-Host 'Con FAIL: sua cac muc tren truoc khi chay lo do.'
    exit 1
}
exit 0
