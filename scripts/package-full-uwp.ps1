param(
    [string]$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$BuildDir = "",
    [string]$Configuration = "Release",
    [string]$CertPfx = "",
    [string]$CertPassword = "",
    [string]$SdkVersion = "10.0.22621.0"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot "build\uwp-x64"
}
if ([string]::IsNullOrWhiteSpace($CertPfx)) {
    $CertPfx = Join-Path $RepoRoot "UWP\archiedxncan.pfx"
}

$outDir = Join-Path $BuildDir "appx_output"
$layoutDir = Join-Path $BuildDir "appx_layout"
$binDir = Join-Path $BuildDir $Configuration

$makeappx = "C:\Program Files (x86)\Windows Kits\10\bin\$SdkVersion\x64\makeappx.exe"
$signtool = "C:\Program Files (x86)\Windows Kits\10\bin\$SdkVersion\x64\signtool.exe"
$d3dCompiler = "C:\Program Files (x86)\Windows Kits\10\Redist\D3D\x64\D3DCompiler_47.dll"

if (!(Test-Path $makeappx)) { throw "makeappx.exe not found: $makeappx" }
if (!(Test-Path $signtool)) { throw "signtool.exe not found: $signtool" }
if (!(Test-Path $CertPfx)) { throw "PFX not found: $CertPfx" }
if (!(Test-Path (Join-Path $binDir "MinecraftLCE.exe"))) { throw "Build output missing: $binDir\MinecraftLCE.exe" }

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
if (Test-Path $layoutDir) { Remove-Item -Recurse -Force $layoutDir }
New-Item -ItemType Directory -Force -Path $layoutDir | Out-Null

# Core app files
Copy-Item -Force (Join-Path $binDir "MinecraftLCE.exe") (Join-Path $layoutDir "MinecraftLCE.exe")
Copy-Item -Force (Join-Path $RepoRoot "UWP\Package.appxmanifest") (Join-Path $layoutDir "AppxManifest.xml")
Copy-Item -Recurse -Force (Join-Path $RepoRoot "UWP\Assets") (Join-Path $layoutDir "Assets")

# Runtime folders copied by CMake into build/<cfg>
foreach ($name in @("Common", "music", "Windows64Media", "Windows64")) {
    $src = Join-Path $binDir $name
    if (Test-Path $src) {
        Copy-Item -Recurse -Force $src (Join-Path $layoutDir $name)
    }
}

function Copy-DllToLayout {
    param([string]$DllName)
    $dest = Join-Path $layoutDir $DllName
    $candidates = @(
        (Join-Path $binDir $DllName),
        (Join-Path $RepoRoot "x64\$Configuration\$DllName"),
        (Join-Path $RepoRoot "x64\Release\$DllName")
    )
    foreach ($src in $candidates) {
        if (Test-Path $src) {
            Copy-Item -Force $src $dest
            Write-Host "Copied $DllName from $src"
            return $true
        }
    }
    Write-Warning "Missing $DllName (not found next to exe or under x64\Release)"
    return $false
}

foreach ($dll in @("iggy_w64.dll", "mss64.dll")) {
    Copy-DllToLayout $dll | Out-Null
}
if (Test-Path $d3dCompiler) {
    Copy-Item -Force $d3dCompiler (Join-Path $layoutDir "D3DCompiler_47.dll")
}

# Without game data the app runs but shows a black/empty screen (only exe + DLLs ~few MB).
$wm = Join-Path $layoutDir "Windows64Media"
$common = Join-Path $layoutDir "Common"
if (-not (Test-Path $wm)) {
    throw "Layout missing Windows64Media - build Release first so CMake copies assets next to MinecraftLCE.exe, or fix BuildDir/Configuration (got: $binDir)"
}
$arc = Get-ChildItem -Path $wm -Filter "MediaWindows64.arc" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $arc) {
    Write-Warning "MediaWindows64.arc not found under Windows64Media - archive/UI may fail (expected under Minecraft.Client\Windows64Media in repo)."
}
if (-not (Test-Path $common)) {
    Write-Warning "Layout missing Common - strings/DLC assets may be incomplete."
}

$appxPath = Join-Path $outDir "MinecraftLCE_full.appx"
if (Test-Path $appxPath) { Remove-Item -Force $appxPath }

& $makeappx pack /d $layoutDir /p $appxPath /o
if ($LASTEXITCODE -ne 0) { throw "makeappx failed: $LASTEXITCODE" }

if ([string]::IsNullOrWhiteSpace($CertPassword)) {
    & $signtool sign /fd SHA256 /a /f $CertPfx $appxPath
} else {
    & $signtool sign /fd SHA256 /a /f $CertPfx /p $CertPassword $appxPath
}
if ($LASTEXITCODE -ne 0) { throw "signtool failed: $LASTEXITCODE" }

$pkg = Get-Item $appxPath
Write-Host ('Full package created: {0} ({1:N0} bytes)' -f $pkg.FullName, $pkg.Length)
