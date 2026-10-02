# Builds the unsigned Windows installer (DESIGN 11.6) from an existing Release build.
#
#   pwsh packaging/windows/build_installer.ps1                 # version from build/CMakeCache.txt
#   pwsh packaging/windows/build_installer.ps1 -Version 0.1.0 -BuildDir build
#
# Needs Inno Setup 6.5.2+ (ISCC.exe). Install: winget install JRSoftware.InnoSetup  or  choco install innosetup
# Output: <BuildDir>/installer/VoiceBooth-<version>-win-x64-setup.exe
[CmdletBinding()]
param(
    [string]$Version = "",
    [string]$BuildDir = "build",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $RepoRoot $BuildDir }
if (-not $OutputDir) { $OutputDir = Join-Path $BuildDir "installer" }

# --- version (CMake PROJECT_VERSION) ---
if (-not $Version) {
    $cache = Join-Path $BuildDir "CMakeCache.txt"
    if (-not (Test-Path $cache)) { throw "Missing $cache. Configure and build first, or pass -Version." }
    $line = Select-String -Path $cache -Pattern '^CMAKE_PROJECT_VERSION:STATIC=(.+)$' | Select-Object -First 1
    if (-not $line) { throw "CMAKE_PROJECT_VERSION not found in $cache" }
    $Version = $line.Matches[0].Groups[1].Value.Trim()
}
if ($Version -notmatch '^\d+\.\d+\.\d+(\.\d+)?$') { throw "Version must be numeric x.y.z (got '$Version')" }

# --- app ---
$AppBuildDir = Join-Path $BuildDir "VoiceBooth_artefacts\Release"
$exe = Join-Path $AppBuildDir "VoiceBooth.exe"
if (-not (Test-Path $exe)) { throw "Missing $exe. Run: cmake --build build --config Release" }

# --- ISCC ---
. (Join-Path $PSScriptRoot "find_iscc.ps1")
$found = Find-Iscc
if (-not $found) { throw "ISCC.exe (Inno Setup 6.5.2+) not found. Install: winget install JRSoftware.InnoSetup" }
$iscc = $found.Path
$isccVersion = $found.Version
Write-Host "ISCC: $iscc ($isccVersion)"

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$iss = Join-Path $PSScriptRoot "VoiceBooth.iss"
& $iscc "/DAppVersion=$Version" "/DAppBuildDir=$AppBuildDir" "/DOutputDir=$OutputDir" $iss
if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }

$setup = Join-Path $OutputDir "VoiceBooth-$Version-win-x64-setup.exe"
if (-not (Test-Path $setup)) { throw "Expected output not found: $setup" }
$size = [math]::Round((Get-Item $setup).Length / 1MB, 1)
Write-Host "Built: $setup ($size MB, unsigned)"

# Hand the path to the CI step that uploads it
if ($env:GITHUB_OUTPUT) { "setup=$setup" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8 }
