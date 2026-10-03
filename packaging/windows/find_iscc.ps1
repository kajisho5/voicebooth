# Inno Setup の ISCC.exe を探す（CI と build_installer.ps1 で共用）
# Inno Setup 6.5 以降は既定で「自分だけ」に入る（%LOCALAPPDATA%\Programs）ので、Program Files・ユーザーの Programs・
# アンインストール情報（レジストリの InstallLocation）・PATH を順に見る。見つかれば @{ Path; Version }、無ければ $null
# PATH は最後（choco の shim は別のバージョン情報を持つため、本体の場所を優先する）

# 文字列から x.y.z を取り出す。0.0.0 は「読めなかった」扱い
function ConvertTo-IsccVersion([string]$text) {
    if ($text -and $text -match '(\d+)\.(\d+)\.(\d+)') {
        $v = [version]::new([int]$Matches[1], [int]$Matches[2], [int]$Matches[3])
        if ($v -gt [version]'0.0.0') { return $v }
    }
    return $null
}

function Find-Iscc {
    $candidates = @()
    foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles, (Join-Path $env:LOCALAPPDATA "Programs"))) {
        if (-not $base) { continue }
        foreach ($dir in @("Inno Setup 6", "Inno Setup 7")) { $candidates += (Join-Path (Join-Path $base $dir) "ISCC.exe") }
    }

    # バージョン：ISCC.exe のファイル情報が "0.0.0.0" のことがある（windows-latest の 6.7.1 で確認）ので、
    # ファイル情報・レジストリ・ISCC の出力をすべて読み、0.0.0 を除いた一番新しいものを使う
    $registryVersions = @()
    $keys = @(
        "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*")
    foreach ($k in $keys) {
        # Set-StrictMode 下でも動くように、無い値は $null として読む（DisplayName の無い項目もある）
        foreach ($item in @(Get-ItemProperty $k -ErrorAction SilentlyContinue)) {
            $props = $item.PSObject.Properties
            $name = if ($props['DisplayName']) { [string]$props['DisplayName'].Value } else { "" }
            $loc  = if ($props['InstallLocation']) { [string]$props['InstallLocation'].Value } else { "" }
            if (-not ($name -like "Inno Setup*" -and $loc)) { continue }
            $candidates += (Join-Path $loc "ISCC.exe")
            if ($props['DisplayVersion']) { $registryVersions += [string]$props['DisplayVersion'].Value }
        }
    }

    $onPath = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($onPath) { $candidates += $onPath.Source }

    foreach ($p in $candidates) {
        if (-not ($p -and (Test-Path $p))) { continue }
        $vi = (Get-Item $p).VersionInfo
        $versions = @(@($vi.ProductVersion, $vi.FileVersion) + $registryVersions | ForEach-Object { ConvertTo-IsccVersion $_ } | Where-Object { $_ })
        if ($versions.Count -eq 0) {
            $out = (& $p "/?" 2>&1 | Out-String)
            $v = ConvertTo-IsccVersion $out
            if ($v) { $versions = @($v) }
        }
        $ver = [version]'0.0.0'
        if ($versions.Count -gt 0) { $ver = ($versions | Sort-Object -Descending | Select-Object -First 1) }
        return @{ Path = $p; Version = $ver }
    }
    return $null
}
