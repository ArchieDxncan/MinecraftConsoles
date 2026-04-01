# Fixes Add-AppxPackage / sideload error:
#   0x800B0109 - The root certificate of the signature must be trusted
#
# LocalMachine (default): requires PowerShell *as Administrator*.
# CurrentUser: no admin; may work for Add-AppxPackage on the same account (try LocalMachine first if not).
#
# Example (elevated):
#   powershell -ExecutionPolicy Bypass -File .\scripts\trust-uwp-signing-cert.ps1 `
#     -PfxPath "D:\Github\MinecraftConsoles\UWP\archiedxncan.pfx" -Password "yourpassword"
#
# Example (no admin):
#   .\scripts\trust-uwp-signing-cert.ps1 -PfxPath "...\archiedxncan.pfx" -Password "..." -Scope CurrentUser

param(
    [Parameter(Mandatory = $true)][string] $PfxPath,
    [string] $Password = "",
    [ValidateSet("LocalMachine", "CurrentUser")]
    [string] $Scope = "LocalMachine"
)

$ErrorActionPreference = "Stop"

function Test-IsAdministrator {
    $p = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    return $p.IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)
}

if (!(Test-Path -LiteralPath $PfxPath)) {
    throw "PFX not found: $PfxPath"
}

if ($Password -eq "" -and $env:UWP_CERT_PFX_PASSWORD) {
    $Password = $env:UWP_CERT_PFX_PASSWORD
}

if ($Scope -eq "LocalMachine" -and !(Test-IsAdministrator)) {
    throw @"
Import to LocalMachine requires Administrator privileges.

You got 0x80090010 (access denied) because TrustedPeople/Root under LocalMachine are protected.

Fix: Right-click PowerShell -> Run as administrator, then run this script again.

If you cannot elevate, try:
  .\scripts\trust-uwp-signing-cert.ps1 -PfxPath `"$PfxPath`" -Password `"...`" -Scope CurrentUser

Or import manually: certlm.msc -> Trusted People -> Import (run certlm as admin).
"@
}

$trustedPeople = "Cert:\$Scope\TrustedPeople"
$rootStore = "Cert:\$Scope\Root"

$sec = ConvertTo-SecureString -String $Password -AsPlainText -Force

Write-Host "Importing PFX into ${Scope}\TrustedPeople..."
Import-PfxCertificate -FilePath $PfxPath -CertStoreLocation $trustedPeople -Password $sec | Out-Null

# Self-signed dev certs usually need the public cert in Trusted Root as well.
$flags = [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::EphemeralKeySet
$cert = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($PfxPath, $Password, $flags)
$cerBytes = $cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert)
$tmpCer = Join-Path $env:TEMP ("uwp_dev_" + [Guid]::NewGuid().ToString("n") + ".cer")
[System.IO.File]::WriteAllBytes($tmpCer, $cerBytes)
try {
    Write-Host "Importing public cert into ${Scope}\Root..."
    Import-Certificate -FilePath $tmpCer -CertStoreLocation $rootStore | Out-Null
} finally {
    Remove-Item -LiteralPath $tmpCer -Force -ErrorAction SilentlyContinue
}

Write-Host "Done. Retry Add-AppxPackage or install from Device Portal."
if ($Scope -eq "CurrentUser") {
    Write-Host "Note: If Add-AppxPackage still reports 0x800B0109, run this script with -Scope LocalMachine from an elevated prompt."
}
Write-Host "If it still fails, reboot once and try again."
