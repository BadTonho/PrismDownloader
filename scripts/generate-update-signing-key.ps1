[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Repository
)

$ErrorActionPreference = 'Stop'
$openssl = Get-Command openssl -ErrorAction Stop
$gh = Get-Command gh -ErrorAction Stop
$temporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) ("PrismUpdateKey-" + [guid]::NewGuid().ToString('N'))
$privateKey = Join-Path $temporaryDirectory 'prism-update-ed25519.pem'
$publicDer = Join-Path $temporaryDirectory 'prism-update-ed25519-public.der'

New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null
try {
    & $openssl.Source genpkey -algorithm Ed25519 -out $privateKey
    & $openssl.Source pkey -in $privateKey -pubout -outform DER -out $publicDer
    $publicBytes = [IO.File]::ReadAllBytes($publicDer)
    if ($publicBytes.Length -lt 32) {
        throw 'OpenSSL did not produce an Ed25519 public key.'
    }
    $publicKeyBase64 = [Convert]::ToBase64String($publicBytes[($publicBytes.Length - 32)..($publicBytes.Length - 1)])
    $privateKeyBase64 = [Convert]::ToBase64String([IO.File]::ReadAllBytes($privateKey))

    # The private key is streamed to GitHub and never printed or written into this repository.
    $privateKeyBase64 | & $gh.Source secret set PRISM_UPDATE_ED25519_PRIVATE_KEY --repo $Repository
    Write-Host "GitHub Secret PRISM_UPDATE_ED25519_PRIVATE_KEY configured for $Repository."
    Write-Host "Embedded public key for verification: $publicKeyBase64"
} finally {
    Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
