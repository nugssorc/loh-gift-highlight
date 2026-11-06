param(
    [ValidateSet('Install','Remove','Check')][string]$Action = 'Install',
    [string]$GamePath = '',
    [string]$PayloadPath = $PSScriptRoot      # 묶음의 _files	ool.ps1 로 들어가므로 자기 폴더가 곧 _files
)
# 공개 묶음 설치기(2026-10-31, 11-01 자체 설치 형식). 묶음 하나 = 모드 하나의 한 판(밸런스판·편의판 폴더마다 _files).
# manifest.json = 모드매니저 '자체 설치 압축' 약속: product·version·variant·files[src,dst,sha256]·entries(입구) + 이 설치기용 title·data.
# 입구: 두 모드가 같이 쓰는 작은 로더(표식 LOH_SocialMods entry 1). 이미 우리 입구가 있는 자리를 먼저, 없으면 version → winhttp → winmm → hid 중 빈 자리.
# 다른 모드의 파일·설치 기록 없는 폴더는 바꾸지 않는다. 바꾸기 전에 묶음 파일 해시를 모두 검사하고, 실패하면 백업으로 되돌린다.
# 관리창(5_모드매니저)이 쓰는 옛 설치기 설치관리.ps1 은 그대로 둔다.
$ErrorActionPreference = 'Stop'
$KnownNames = @('LOH_GiftHighlight','LOH_GoldenOrchidJade')
$Slots = @('version.dll','winhttp.dll','winmm.dll','hid.dll')
function Say([string]$t) { Write-Host $t }
function Fail([string]$t) { Write-Host ''; Write-Host "  [멈춤] $t" -ForegroundColor Yellow; Write-Host ''; exit 1 }
function File-Hash([string]$Path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToUpper() }
function Find-Game {
    if ($GamePath) { $g = [IO.Path]::GetFullPath($GamePath).TrimEnd('\'); if (!(Test-Path -LiteralPath (Join-Path $g 'ThreeKingdom.exe'))) { Fail '지정한 게임 폴더에 ThreeKingdom.exe 가 없습니다.' }; return $g }
    $here = $PSScriptRoot
    for ($k = 0; $k -lt 6 -and $here; $k++) { if (Test-Path -LiteralPath (Join-Path $here 'ThreeKingdom.exe')) { return $here }; $here = Split-Path -Parent $here }
    $roots = @()
    foreach ($key in @('HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam')) {
        try { $v = Get-ItemProperty -Path $key -ErrorAction Stop; if ($v.SteamPath) { $roots += $v.SteamPath }; if ($v.InstallPath) { $roots += $v.InstallPath } } catch {} }
    $libs = @()
    foreach ($r in $roots) { $r = $r -replace '/', '\'; $libs += $r
        $vdf = Join-Path $r 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $vdf) { foreach ($m in [regex]::Matches((Get-Content -LiteralPath $vdf -Raw), '"path"\s+"([^"]+)"')) { $libs += ($m.Groups[1].Value -replace '\\\\', '\') } } }
    foreach ($l in ($libs | Select-Object -Unique)) { $g = Join-Path $l 'steamapps\common\LegendOfHeros'; if (Test-Path -LiteralPath (Join-Path $g 'ThreeKingdom.exe')) { return [IO.Path]::GetFullPath($g).TrimEnd('\') } }
    Say '  게임 폴더를 자동으로 찾지 못했습니다.'
    $p = (Read-Host '  영웅입지전 게임 폴더(ThreeKingdom.exe 가 있는 폴더) 경로를 붙여 넣고 엔터').Trim('"', ' ')
    if ($p -and (Test-Path -LiteralPath (Join-Path $p 'ThreeKingdom.exe'))) { return [IO.Path]::GetFullPath($p).TrimEnd('\') }
    Fail '그 폴더에 ThreeKingdom.exe 가 없습니다.'
}
function Checked-Path([string]$Relative) {
    $Full = [IO.Path]::GetFullPath((Join-Path $Game $Relative))
    if (!$Full.StartsWith($Game + '\', [StringComparison]::OrdinalIgnoreCase)) { Fail '게임 폴더 밖의 경로입니다.' }
    $Current = $Full
    while ($Current -and $Current.Length -gt $Game.Length) {
        if ((Test-Path -LiteralPath $Current) -and ((Get-Item -LiteralPath $Current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { Fail "연결 경로는 바꾸지 않습니다: $Current" }
        $Current = [IO.Path]::GetDirectoryName($Current)
    }
    return $Full
}
function Is-Ours([string]$Path) {      # 우리 입구 표식(분리판부터 같은 로더)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    $Text = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($Path))
    return $Text.Contains('LOH_SocialMods entry 1') -or $Text.Contains('LOH_GiftHighlight entry 1')
}
function Owned-Slots { @($Slots | Where-Object { Is-Ours (Join-Path $Game $_) }) }
function Read-Owner([string]$Directory, [string]$RecordName, [string]$Name) {      # 설치 기록이 우리 것인 폴더만 바꿈
    if (!(Test-Path -LiteralPath $Directory)) { return $null }
    if (!(Get-Item -LiteralPath $Directory).PSIsContainer) { Fail "폴더가 아닙니다: $Directory" }
    if (Get-ChildItem -LiteralPath $Directory -Force -Recurse | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) { Fail "모드 안에 연결 경로가 있습니다: $Directory" }
    $RecordPath = Join-Path $Directory $RecordName
    if (!(Test-Path -LiteralPath $RecordPath -PathType Leaf)) { Fail "설치 기록이 없는 기존 폴더는 바꾸지 않습니다: $Directory" }
    $Record = Get-Content -LiteralPath $RecordPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($Record.Name -ne $Name) { Fail "다른 모드의 폴더입니다: $Directory" }
    return $Record
}

$PayloadPath = [IO.Path]::GetFullPath($PayloadPath)
$Manifest = Get-Content -LiteralPath (Join-Path $PayloadPath 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$Name = [string]$Manifest.product; $Title = [string]$Manifest.title; $Version = [string]$Manifest.version; $Variant = [string]$Manifest.variant; $DataName = [string]$Manifest.data
if ($KnownNames -notcontains $Name) { Fail "알 수 없는 모드입니다: $Name" }
Say ''; Say "  ==== $Title $Version $(if ($Variant) { "($Variant) " })— $(@{Install='설치';Remove='제거';Check='확인'}[$Action]) ===="
$Game = Find-Game; Say "  게임 폴더: $Game"
$Directory = Checked-Path $Name
$Prev = Read-Owner $Directory '설치기록.json' $Name
$DataPath = $null
if ($DataName) {
    if ($Name -ne 'LOH_GoldenOrchidJade' -or $DataName -ne '금란선옥') { Fail '알 수 없는 데이터 경로입니다.' }
    $DataPath = Checked-Path ('MODs\' + $DataName)
    $null = Read-Owner $DataPath '.LOH_GoldenOrchidJade.json' $Name
}
if ($Action -eq 'Check') {
    Say "  설치됨: $(if ($Prev) { "예 ($($Prev.Version)$(if ($Prev.Variant) { ' ' + $Prev.Variant }), 입구 $($Prev.Entry))" } else { '아니요' })"
    Say "  우리 입구: $(if ((Owned-Slots).Count) { (Owned-Slots) -join ', ' } else { '없음' })"
    foreach ($s in $Slots) { $p = Join-Path $Game $s; if ((Test-Path -LiteralPath $p) -and !(Is-Ours $p)) { Say "  $s : 다른 모드가 쓰는 중" } }
    exit 0
}
if (Get-Process -Name 'ThreeKingdom' -ErrorAction SilentlyContinue) { Fail '게임이 켜져 있습니다. 게임을 완전히 끈 뒤 다시 실행하세요.' }

# 입구 자리 고르기
$Entry = $null
if ($Action -eq 'Install') {
    $Owned = @(Owned-Slots)
    if ($Owned.Count) { $Entry = $Owned[0] }
    else { foreach ($s in $Slots) { if (!(Test-Path -LiteralPath (Join-Path $Game $s))) { $Entry = $s; break }; Say "  - $s 자리: 다른 모드가 쓰는 중 → 건너뜀" } }
    if (!$Entry) { Fail '입구로 쓸 빈 자리가 없습니다(version / winhttp / winmm / hid 가 모두 다른 모드에 쓰이는 중). 다른 로더 모드를 정리한 뒤 다시 실행하세요.' }
    Say "  입구 자리: $Entry"
    foreach ($File in @($Manifest.files) + @($Manifest.entries)) {      # 바꾸기 전에 묶음 전체 검사
        $Source = [IO.Path]::GetFullPath((Join-Path $PayloadPath $File.src))
        if (!$Source.StartsWith($PayloadPath + '\', [StringComparison]::OrdinalIgnoreCase)) { Fail '잘못된 배포 파일 경로입니다.' }
        if (!(Test-Path -LiteralPath $Source -PathType Leaf) -or (File-Hash $Source) -ne ([string]$File.sha256).ToUpper()) { Fail "설치 파일 '$($File.src)' 이(가) 없거나 받은 것과 다릅니다. 압축을 다시 풀어 주세요(백신이 지웠다면 허용 뒤 다시)." }
    }
} elseif (!$Prev) { Say '  이 모드는 설치되어 있지 않습니다.'; exit 0 }

$Touched = @($Directory) + $(if ($DataPath) { @($DataPath) } else { @() }) + @($Slots | ForEach-Object { Join-Path $Game $_ } | Where-Object { Is-Ours $_ })
if ($Entry) { $Touched += (Join-Path $Game $Entry) }
$Backup = Checked-Path ('.loh-social-backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $Backup -Force | Out-Null
$Snapshots = @()
foreach ($Path in @($Touched | Select-Object -Unique)) {
    $Saved = Join-Path $Backup $Path.Substring($Game.Length + 1); $Existed = Test-Path -LiteralPath $Path
    if ($Existed) { New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($Saved)) -Force | Out-Null; Copy-Item -LiteralPath $Path -Destination $Saved -Recurse -Force }
    $Snapshots += [pscustomobject]@{ Path=$Path; Saved=$Saved; Existed=$Existed }
}
try {
    if ($Action -eq 'Install') {
        $Keep = @(); if (Test-Path -LiteralPath $Directory) { $Keep = @(Get-ChildItem -LiteralPath $Directory -Filter '*.ini' -File | ForEach-Object { [pscustomobject]@{ Name=$_.Name; Bytes=[IO.File]::ReadAllBytes($_.FullName) } }) }      # 메모장 설정은 다시 설치해도 그대로
        if (Test-Path -LiteralPath $Directory) { Remove-Item -LiteralPath $Directory -Recurse -Force }      # 옛 판 파일이 남지 않게(로그는 백업에)
        New-Item -ItemType Directory -Path $Directory -Force | Out-Null
        Get-ChildItem -LiteralPath (Join-Path $PayloadPath $Name) | Copy-Item -Destination $Directory -Recurse -Force
        foreach ($k in $Keep) { $t = Join-Path $Directory $k.Name; if (!(Test-Path -LiteralPath $t)) { [IO.File]::WriteAllBytes($t, $k.Bytes) } }      # 묶음에 같은 설정 파일이 있으면 그 판의 값(밸런스판·편의판)이 이김, 옛 파일은 백업에
        $Record = [ordered]@{ Name=$Name; Version=$Version; Variant=$Variant; Entry=$Entry; Data=$DataName; Installed=(Get-Date -Format o) }
        $Record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Directory '설치기록.json') -Encoding UTF8
        if ($DataPath) {
            if (Test-Path -LiteralPath $DataPath) { Remove-Item -LiteralPath $DataPath -Recurse -Force }
            New-Item -ItemType Directory -Path $DataPath -Force | Out-Null
            Get-ChildItem -LiteralPath (Join-Path $PayloadPath ('MODs\' + $DataName)) | Copy-Item -Destination $DataPath -Recurse -Force
            $Record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $DataPath '.LOH_GoldenOrchidJade.json') -Encoding UTF8
        }
        $EntryPath = Join-Path $Game $Entry; $New = Join-Path $PayloadPath ('entry\' + $Entry)
        if (!(Test-Path -LiteralPath $EntryPath) -or (File-Hash $EntryPath) -ne (File-Hash $New)) { Copy-Item -LiteralPath $New -Destination $EntryPath -Force }
        Say ''; Say "  설치 끝: $Title $Version $(if ($Variant) { "($Variant)" })"
        if ($DataPath) { Say '  게임을 켜고 [모드] 목록에서 "선물 횟수 리셋 아이템 (금란선옥)" 을 켠 뒤 저장을 불러오세요.' }
    } else {
        Remove-Item -LiteralPath $Directory -Recurse -Force
        if ($DataPath -and (Test-Path -LiteralPath $DataPath)) { Remove-Item -LiteralPath $DataPath -Recurse -Force }
        $Remaining = @($KnownNames | Where-Object { Test-Path -LiteralPath (Join-Path $Game ($_ + '\' + $_ + '.dll')) })
        if (!$Remaining.Count) { foreach ($s in Owned-Slots) { Remove-Item -LiteralPath (Join-Path $Game $s) -Force } }
        Say ''; Say "  제거 끝: $Title. $(if ($Remaining.Count) { '같이 쓰는 다른 모드가 있어 입구는 남겼습니다.' } else { '우리 입구도 지웠습니다.' })"
    }
} catch {
    foreach ($Snapshot in $Snapshots) {
        if (Test-Path -LiteralPath $Snapshot.Path) { Remove-Item -LiteralPath $Snapshot.Path -Recurse -Force }
        if ($Snapshot.Existed) { Copy-Item -LiteralPath $Snapshot.Saved -Destination $Snapshot.Path -Recurse -Force }
    }
    Fail "중간에 실패해 원래대로 되돌렸습니다: $($_.Exception.Message)"
}
Say "  백업: $Backup"; Say ''
