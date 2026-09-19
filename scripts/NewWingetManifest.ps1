<#
.SYNOPSIS
    Writes a submission-ready winget manifest set for relmer.Casso from a
    published GitHub release, filling in the values that can only be read off
    the signed .msixbundle.

.DESCRIPTION
    THIS IS FOR THE FIRST SUBMISSION ONLY. Once relmer.Casso exists in
    winget-pkgs, the release job's "Publish to winget" step keeps it current
    with `wingetcreate update`, which reads the same values out of the same
    bundle without anyone running anything. What it cannot do is create a
    package that is not there yet, or change an existing package's
    InstallerType, and those are the two cases this covers.

    THREE OF THE FIELDS CANNOT BE TYPED, and getting one of them wrong is a
    failed validation run rather than a local error:

      InstallerSha256     the hash of the download, which is the bundle as
                          published, so it has to come from the published
                          URL rather than from a local build. A rebuild of
                          the same commit does not produce the same bytes.

      SignatureSha256     the hash of AppxSignature.p7x INSIDE the bundle.
                          winget uses it to check that the package it
                          downloaded was signed by the publisher the manifest
                          claims, before installing anything. It exists only
                          after signing, so a locally packed bundle has
                          nothing to read here.

      PackageFamilyName   the identity Windows knows the package by, and
                          what winget uses to find it again to upgrade or
                          uninstall it. It is the manifest's Name and a hash
                          of the manifest's Publisher, so it changes if the
                          signing certificate is ever reissued under a
                          different subject.

    All three are read from the bundle this script downloads, so none of them
    is a value anyone has to keep in their head or in a second file.

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

# UTC, because that is what the release's publishedAt is and what winget's
# ReleaseDate means. The local date is a day out either side of midnight.
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

# Downloaded from the release rather than taken from a local artifacts folder.
# The hash below has to be the hash of the file a user's winget will fetch,
# and the only way to be sure of that is to fetch it.
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

# Name_<hash of Publisher>. The hash is the first eight bytes of the SHA256 of
# the publisher string in UTF-16, base32 encoded with the alphabet Windows
# uses for this and nothing else: digits first, then the letters minus i, l,
# o and u, which are the ones that misread aloud or on screen. It is neither
# RFC 4648's alphabet nor its ordering, which is why the obvious base32 gives
# a plausible-looking wrong answer.
#
# Derived rather than stored, and checked against the machine's own packages
# below, because a wrong family name is a manifest that validates and then
# installs a package winget can never find again.
function Get-PublisherHash
{
    param([string] $publisher)

    $digest = [Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::Unicode.GetBytes($publisher))

    # 64 bits do not divide into 5, so the last character carries four real
    # bits and a zero. Windows pads at the end, not the front.
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

# Every package installed on this machine already knows its own answer, so the
# derivation can be checked against a few hundred of them rather than trusted.
# A failure here means the encoding above has drifted from what Windows does,
# which would otherwise show up as a winget package that cannot be upgraded.
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

    # The TEMPLATE banner is true of the file it is in and false of the file
    # written here, so it does not travel. It is a whole paragraph, blank line
    # included, which is why this matches across lines rather than dropping
    # the comment lines one at a time.
    $text = [regex]::Replace($text, '(?ms)^# TEMPLATE\..*?\r?\n\r?\n', '')

    foreach ($key in $substitutions.Keys)
    {
        $text = $text.Replace($key, $substitutions[$key])
    }

    if ($text -match '\{[A-Z_]+\}')
    {
        throw "$($template.Name) still holds a placeholder after substitution: $($Matches[0])"
    }

    # LF and no BOM: winget-pkgs is a Linux-validated repository and its
    # schema check reads the file as bytes.
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
