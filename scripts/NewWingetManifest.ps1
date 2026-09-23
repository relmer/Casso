<#
.SYNOPSIS
    Writes a submission-ready winget manifest set for relmer.Casso from a
    published GitHub release, filling in the values that can only be read off
    the signed .msixbundle.

.DESCRIPTION
    FOR THE FIRST SUBMISSION ONLY. Once relmer.Casso exists in winget-pkgs
    the release job keeps it current with `wingetcreate update`; what that
    cannot do is create a package or change its InstallerType.

    Three fields cannot be typed, and all three are read out of the bundle
    this downloads: InstallerSha256 (the published bytes), SignatureSha256
    (AppxSignature.p7x inside the bundle, so it exists only after signing),
    and PackageFamilyName.

.PARAMETER Tag
    Release tag to build the manifest from, e.g. v1.25.0. Default: the
    repository's latest release.

.PARAMETER Repository
    owner/name to read the release from. Default: relmer/Casso.

.PARAMETER Destination
    Where to write the three YAML files. Default: a manifests/r/relmer/Casso/
    <version> tree under the system temp directory, laid out the way
    winget-pkgs expects so the folder can be copied straight into a fork.

.EXAMPLE
    .\NewWingetManifest.ps1
    Writes the manifest set for the latest release and prints where it went.

.OUTPUTS
    The destination directory, last line. Exit code 0 on success.
#>
param(
    [string]$Tag         = "",
    [string]$Repository  = "relmer/Casso",
    [string]$Destination = ""
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$templates = Join-Path $repoRoot 'Installer/winget'

#-----------------------------------------------------------------------------
# The release
#-----------------------------------------------------------------------------

if ([string]::IsNullOrEmpty($Tag))
{
    $Tag = gh release view --repo $Repository --json tagName --jq '.tagName'
    if ($LASTEXITCODE -ne 0 -or -not $Tag) { throw "Could not read the latest release of $Repository." }
}

$release = gh release view $Tag --repo $Repository --json publishedAt,assets | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) { throw "No release $Tag in $Repository." }

$version = $Tag -replace '^v', ''
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "Tag $Tag does not carry a three-part version." }

$asset = $release.assets | Where-Object { $_.name -like '*.msixbundle' } | Select-Object -First 1
if (-not $asset)
{
    throw ("Release $Tag has no .msixbundle. Releases before the MSIX existed ship only the " +
           'zips, and a zip cannot be an msix manifest.')
}

# UTC: the local date is a day out either side of midnight.
$releaseDate = ([datetime] $release.publishedAt).ToUniversalTime().ToString('yyyy-MM-dd')

Write-Host "Release:  $Tag ($releaseDate)"
Write-Host "Bundle:   $($asset.name)"

#-----------------------------------------------------------------------------
# The bundle
#-----------------------------------------------------------------------------

$work = Join-Path ([IO.Path]::GetTempPath()) "CassoWinget-$version"
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
New-Item -ItemType Directory -Path $work -Force | Out-Null

$bundle = Join-Path $work $asset.name

# From the release, not a local build: the hash must match what winget fetches.
gh release download $Tag --repo $Repository --pattern '*.msixbundle' --dir $work
if ($LASTEXITCODE -ne 0) { throw "Could not download the bundle from $Tag." }

$installerSha = (Get-FileHash $bundle -Algorithm SHA256).Hash.ToUpper()

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($bundle)
try
{
    $signature = $archive.Entries | Where-Object { $_.FullName -eq 'AppxSignature.p7x' }
    if (-not $signature)
    {
        throw ("$($asset.name) holds no AppxSignature.p7x, so it was never signed. An unsigned " +
               'bundle cannot be installed and cannot be submitted.')
    }

    $stream = $signature.Open()
    try
    {
        $signatureSha = ([Security.Cryptography.SHA256]::Create().ComputeHash($stream) |
                         ForEach-Object { $_.ToString('X2') }) -join ''
    }
    finally
    {
        $stream.Dispose()
    }

    $entry = $archive.Entries | Where-Object { $_.FullName -eq 'AppxMetadata/AppxBundleManifest.xml' }
    if (-not $entry) { throw "$($asset.name) has no AppxBundleManifest.xml." }

    $reader = New-Object IO.StreamReader($entry.Open())
    try { $manifest = [xml] $reader.ReadToEnd() } finally { $reader.Dispose() }
}
finally
{
    $archive.Dispose()
}

$name      = $manifest.Bundle.Identity.Name
$publisher = $manifest.Bundle.Identity.Publisher

#-----------------------------------------------------------------------------
# The family name
#-----------------------------------------------------------------------------

# Name_<hash>: first 8 bytes of SHA256 of the UTF-16 publisher, base32 with
# Windows's own alphabet. NOT RFC 4648's, which gives a plausible wrong answer.
function Get-PublisherHash
{
    param([string] $publisher)

    $digest = [Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::Unicode.GetBytes($publisher))

    # 64 bits do not divide into 5; Windows pads at the end, not the front.
    $bits = (($digest[0..7]) | ForEach-Object { [Convert]::ToString($_, 2).PadLeft(8, '0') }) -join ''
    $bits += '0'

    $alphabet = '0123456789abcdefghjkmnpqrstvwxyz'
    $hash     = ''

    for ($i = 0; $i -lt 65; $i += 5)
    {
        $hash += $alphabet[[Convert]::ToInt32($bits.Substring($i, 5), 2)]
    }

    return $hash
}

# Check the derivation against packages that already know their own answer.
$checked = 0
foreach ($package in (Get-AppxPackage | Select-Object -First 50))
{
    $expected = ($package.PackageFamilyName -split '_')[-1]
    if ((Get-PublisherHash $package.Publisher) -ne $expected)
    {
        throw ("The publisher hash derivation disagrees with Windows for " +
               "$($package.PackageFamilyName). Do not submit this manifest.")
    }
    $checked++
}

$familyName = "$name`_$(Get-PublisherHash $publisher)"

Write-Host "Publisher: $publisher"
Write-Host "Family:    $familyName (derivation agreed with $checked installed packages)"

#-----------------------------------------------------------------------------
# Write it out
#-----------------------------------------------------------------------------

if ([string]::IsNullOrEmpty($Destination))
{
    $Destination = Join-Path $work "manifests/r/relmer/Casso/$version"
}

New-Item -ItemType Directory -Path $Destination -Force | Out-Null

$substitutions = @{
    '{VERSION}'             = $version
    '{RELEASE_DATE}'        = $releaseDate
    '{INSTALLER_URL}'       = $asset.url
    '{INSTALLER_SHA256}'    = $installerSha
    '{SIGNATURE_SHA256}'    = $signatureSha
    '{PACKAGE_FAMILY_NAME}' = $familyName
}

foreach ($template in (Get-ChildItem $templates -Filter *.yaml))
{
    $text = Get-Content $template.FullName -Raw

    # Drop the TEMPLATE banner; it is false of the file written here.
    $text = [regex]::Replace($text, '(?ms)^# TEMPLATE\..*?\r?\n\r?\n', '')

    foreach ($key in $substitutions.Keys)
    {
        $text = $text.Replace($key, $substitutions[$key])
    }

    if ($text -match '\{[A-Z_]+\}')
    {
        throw "$($template.Name) still holds a placeholder after substitution: $($Matches[0])"
    }

    # LF, no BOM: winget-pkgs validates on Linux.
    $text = $text -replace "`r`n", "`n"
    [IO.File]::WriteAllText((Join-Path $Destination $template.Name), $text, (New-Object Text.UTF8Encoding $false))

    Write-Host "  wrote $($template.Name)"
}

Write-Host ""
Write-Host "Validate and submit:"
Write-Host "  winget validate --manifest $Destination"
Write-Host "  winget install --manifest $Destination"
Write-Host ""
Write-Host $Destination
