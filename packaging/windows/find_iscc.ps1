# Inno Setup の ISCC.exe を探す（CI と build_installer.ps1 で共用）
# Inno Setup 6.5 以降は既定で「自分だけ」に入る（%LOCALAPPDATA%\Programs）ので、PATH・Program Files・ユーザーの Programs・
# アンインストール情報（レジストリの InstallLocation）を順に見る。見つかれば @{ Path; Version }、無ければ $null
function Find-Iscc {
    $candidates = @()
    $onPath = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($onPath) { $candidates += $onPath.Source }

    foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles, (Join-Path $env:LOCALAPPDATA "Programs"))) {
        if (-not $base) { continue }
        foreach ($dir in @("Inno Setup 6", "Inno Setup 7")) { $candidates += (Join-Path (Join-Path $base $dir) "ISCC.exe") }
    }

    $keys = @(
        "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*")
    foreach ($k in $keys) {
        Get-ItemProperty $k -ErrorAction SilentlyContinue |
            Where-Object { $_.DisplayName -like "Inno Setup*" -and $_.InstallLocation } |
            ForEach-Object { $candidates += (Join-Path $_.InstallLocation "ISCC.exe") }
    }

    foreach ($p in $candidates) {
        if ($p -and (Test-Path $p)) {
            $vi = (Get-Item $p).VersionInfo
            return @{ Path = $p; Version = [version]::new($vi.FileMajorPart, $vi.FileMinorPart, $vi.FileBuildPart) }
        }
    }
    return $null
}
