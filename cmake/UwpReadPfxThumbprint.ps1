# Non-interactive thumbprint read (Get-PfxCertificate prompts for password and fails under CMake).
param(
    [Parameter(Mandatory = $true)][string] $PfxPath,
    [string] $Password = ''
)
$flags = [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::EphemeralKeySet
$cert = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($PfxPath, $Password, $flags)
Write-Output $cert.Thumbprint
