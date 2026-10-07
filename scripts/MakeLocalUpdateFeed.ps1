<#
.SYNOPSIS
    Writes release.json for a local update feed: the release GitHub would
    describe, built from a folder of release files.

.DESCRIPTION
    For an end-to-end update test without publishing a release. Point
    CASSO_UPDATE_FEED at the release.json this writes, and Casso reads its
    update check from it instead of api.github.com: the CHANGELOG.md and
    README.md beside it stand in for the tag's, and each asset downloads from
    the file its browser_download_url gives, relative to the folder.

    The folder holds the target release's files: Casso-<version>-x64.zip,
    Casso-<version>-ARM64.zip, Casso-<version>.msixbundle (any of them), and
    CHANGELOG.md and README.md. Every .zip and .msixbundle becomes an asset
    with its size and SHA-256 digest. Running it again rewrites release.json
    from what the folder holds now.

.PARAMETER Folder
    The folder of release files. release.json is written into it.

.PARAMETER Version
    The release's version, e.g. 1.31.91. Default: read from the asset names.

.PARAMETER PublishedAt
    The release's publication time, ISO 8601 UTC. Default: now.

.OUTPUTS
    The path of release.json, last line.
#>
param(
    [Parameter(Mandatory)]
    [string]  $Folder,
    [string]  $Version     = "",
    [string]  $PublishedAt = ""
)

$ErrorActionPreference = 'Stop'

$Folder = (Resolve-Path $Folder).Path

$assets = @(Get-ChildItem $Folder -File | Where-Object { $_.Extension -in '.zip', '.msixbundle' } | Sort-Object Name)

if ($assets.Count -eq 0)
{
    throw "No .zip or .msixbundle files in $Folder."
}

if ([string]::IsNullOrEmpty($Version))
{
    $versions = @($assets | ForEach-Object {
        if ($_.Name -match '^Casso-(\d+\.\d+\.\d+)(-[A-Za-z0-9]+)?\.(zip|msixbundle)$') { $Matches[1] }
    } | Sort-Object -Unique)

    if ($versions.Count -ne 1)
    {
        throw "Cannot tell the version from the asset names (found: $($versions -join ', ')). Pass -Version."
    }

    $Version = $versions[0]
}

foreach ($notes in 'CHANGELOG.md', 'README.md')
{
    if (-not (Test-Path (Join-Path $Folder $notes)))
    {
        Write-Warning "$notes is not in $Folder; the dialog will report the notes as missing."
    }
}

if ([string]::IsNullOrEmpty($PublishedAt))
{
    $PublishedAt = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
}

$tag = "v$Version"

Write-Host "Folder:    $Folder"
Write-Host "Release:   $tag, published $PublishedAt"

$assetEntries = foreach ($asset in $assets)
{
    $digest = (Get-FileHash $asset.FullName -Algorithm SHA256).Hash.ToLower()

    Write-Host ("  {0}  {1,12:N0} bytes  sha256:{2}" -f $asset.Name, $asset.Length, $digest)

    [ordered] @{
        name                 = $asset.Name
        size                 = $asset.Length
        digest               = "sha256:$digest"
        browser_download_url = $asset.Name
    }
}

$release = [ordered] @{
    tag_name     = $tag
    name         = "Casso $Version"
    published_at = $PublishedAt
    prerelease   = $false
    html_url     = "https://github.com/relmer/Casso/releases/tag/$tag"
    assets       = @($assetEntries)
}

$jsonPath = Join-Path $Folder 'release.json'

[IO.File]::WriteAllText($jsonPath, ($release | ConvertTo-Json -Depth 4), (New-Object Text.UTF8Encoding $false))

Write-Host "Wrote:     $jsonPath"
Write-Host ""
Write-Host "Set the feed with:"
Write-Host "  `$env:CASSO_UPDATE_FEED = '$jsonPath'"
Write-Host ""
Write-Host $jsonPath
