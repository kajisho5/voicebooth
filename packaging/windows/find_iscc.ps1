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

    # 版：ISCC.exe のファイル情報の数値が 0 のことがある（windows-latest の 6.7.1 で確認）ので、文字列 → レジストリ → ISCC の出力の順に読む
    $registryVersion = $null
    $keys = @(
        "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*")
    foreach ($k in $keys) {
        Get-ItemProperty $k -ErrorAction SilentlyContinue |
            Where-Object { $_.DisplayName -like "Inno Setup*" -and $_.InstallLocation } |
            ForEach-Object {
                $candidates += (Join-Path $_.InstallLocation "ISCC.exe")
                if (-not $registryVersion -and $_.DisplayVersion) { $registryVersion = $_.DisplayVersion }
            }
    }

    foreach ($p in $candidates) {
        if (-not ($p -and (Test-Path $p))) { continue }
        $vi = (Get-Item $p).VersionInfo
        $text = @($vi.ProductVersion, $vi.FileVersion, $registryVersion) | Where-Object { $_ -match '\d+\.\d+\.\d+' } | Select-Object -First 1
        if (-not $text) {
            $out = (& $p "/?" 2>&1 | Out-String)
            if ($out -match '(\d+\.\d+\.\d+)') { $text = $Matches[1] }
        }
        $ver = [version]'0.0.0'
        if ($text -and $text -match '(\d+)\.(\d+)\.(\d+)') { $ver = [version]::new([int]$Matches[1], [int]$Matches[2], [int]$Matches[3]) }
        return @{ Path = $p; Version = $ver }
    }
    return $null
}
