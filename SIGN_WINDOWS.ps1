param([Parameter(Mandatory=$true)][string]$CertificateThumbprint, [string]$SignTool='signtool.exe', [string]$TimestampUrl='')
$ErrorActionPreference = 'Stop'
$certificate = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Thumbprint -eq $CertificateThumbprint -and $_.HasPrivateKey }
if (-not $certificate) { throw 'A code-signing certificate with its private key is required in CurrentUser\My.' }
$folder = if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'GBStudio.exe')) { $PSScriptRoot } else { Join-Path $PSScriptRoot 'dist' }
$files = @('GBStudio.exe','gb_pixel_filters.exe','tools\gb_headless.exe')
foreach ($name in $files) {
    $file = Join-Path $folder $name
    if (-not (Test-Path -LiteralPath $file)) { throw "Missing distribution file: $name" }
    $signArgs = @('sign','/sha1',$CertificateThumbprint,'/fd','SHA256')
    if ($TimestampUrl) { if (-not $TimestampUrl.StartsWith('https://')) { throw 'Timestamp URL must use HTTPS.' }; $signArgs += @('/tr',$TimestampUrl,'/td','SHA256') }
    & $SignTool @signArgs $file
    if ($LASTEXITCODE -ne 0) { throw "Signing failed: $name" }
    & $SignTool verify /pa $file
    if ($LASTEXITCODE -ne 0) { throw "Signature verification failed: $name" }
}
