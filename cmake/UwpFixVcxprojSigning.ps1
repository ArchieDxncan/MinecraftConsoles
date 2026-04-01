# Fixes MinecraftLCE.vcxproj so MSBuild never sees CMake's temp cert (F63A...) mismatched with your PFX.
# Run from PRE_BUILD (non-interactive). Password: env UWP_CERT_PFX_PASSWORD or -Password.
param(
    [Parameter(Mandatory = $true)][string] $VcxPath,
    [Parameter(Mandatory = $true)][string] $PfxPath,
    [string] $Password = ''
)
$ErrorActionPreference = 'Stop'

if ($Password -eq '' -and $env:UWP_CERT_PFX_PASSWORD) {
    $Password = $env:UWP_CERT_PFX_PASSWORD
}

$flags = [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::EphemeralKeySet
$cert = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($PfxPath, $Password, $flags)
$thumb = $cert.Thumbprint

# MSBuild accepts forward slashes in PackageCertificateKeyFile
$pfxForXml = $PfxPath -replace '\\', '/'

if (-not (Test-Path -LiteralPath $VcxPath)) {
    Write-Error "UwpFixVcxprojSigning: not found: $VcxPath"
}

$txt = [System.IO.File]::ReadAllText($VcxPath)

# 1) Known stale thumbprint from CMake's Windows_TemporaryKey (always nuke)
$txt = $txt.Replace('F63AAC84A0884CDB0126C3CDEA991E36DF064B3E', $thumb)

# 2) Every PackageCertificateThumbprint must match the PFX (replace all tags)
$txt = [regex]::Replace($txt, '<PackageCertificateThumbprint>[^<]*</PackageCertificateThumbprint>',
    "<PackageCertificateThumbprint>$thumb</PackageCertificateThumbprint>")

# 3) Every PackageCertificateKeyFile must be our PFX (replace all tags)
$txt = [regex]::Replace($txt, '<PackageCertificateKeyFile>[^<]*</PackageCertificateKeyFile>',
    "<PackageCertificateKeyFile>$pfxForXml</PackageCertificateKeyFile>")

# 4) Drop None item for generated temp key (if still present)
$txt = [regex]::Replace($txt, '\r?\n\s*<None Include="[^"]*Windows_TemporaryKey\.pfx"\s*/>', '')

[System.IO.File]::WriteAllText($VcxPath, $txt, [System.Text.UTF8Encoding]::new($false))

# Optional: same line in .filters
$filters = [System.IO.Path]::ChangeExtension($VcxPath, '.vcxproj.filters')
if (Test-Path -LiteralPath $filters) {
    $ft = [System.IO.File]::ReadAllText($filters)
    $ft = [regex]::Replace($ft, '\r?\n\s*<None Include="[^"]*Windows_TemporaryKey\.pfx"\s*/>', '')
    [System.IO.File]::WriteAllText($filters, $ft, [System.Text.UTF8Encoding]::new($false))
}
