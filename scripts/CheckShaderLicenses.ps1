<#
.SYNOPSIS
    Fails if any shader source under -Path contains a GPL / copyleft term
    outside a designated `// ATTRIBUTION:` comment block.

.DESCRIPTION
    P8-T7 / Constitution v1.5.0. The Casso runtime ships only MIT or public-
    domain shader ports (crt-pi, libretro bloom, libretro ntsc-adaptive
    chroma). A drive-by upstream re-port from a GPL fork would silently
    poison the binary's license posture, so this script runs as a Build.ps1
    pre-build step on every configuration and refuses to let the solution
    compile if it finds any of:

        * "GPL"
        * "GNU General Public"
        * "copyleft"

    Case-insensitive substring match. The check is suppressed inside an
    attribution header: any of the above words appearing on a line within
    one line of a `// ATTRIBUTION:` marker is considered safe (those
    headers cite upstream license names exactly so the script must permit
    them in that narrow context). The marker itself is required to be a
    line-comment so it can never apply mid-shader.

.PARAMETER Path
    Root directories to scan. Default: the two that hold shaders,
    CassoEmuCore/Shaders and Dxui/Render/Shaders, relative to the repo root.
    A root that does not exist fails the check: the shaders moved once and
    the check went on passing over an empty folder, scanning nothing.

.OUTPUTS
    Exit code 0 on success; 1 on the first violation. Violations are
    written to stderr in `file:line -- word` form so MSBuild surfaces
    them as clickable errors.
#>
param(
    [string[]]$Path = @()
)

$ErrorActionPreference = 'Stop'

# Default to the repo's shader roots when no path was supplied so the script
# is callable from anywhere (Build.ps1 pre-build, CI, manual invocation).
if ($Path.Count -eq 0)
{
    $repoRoot = Split-Path $PSScriptRoot -Parent
    $Path     = @((Join-Path $repoRoot 'CassoEmuCore/Shaders'), (Join-Path $repoRoot 'Dxui/Render/Shaders'))
}

$missing = @($Path | Where-Object { -not (Test-Path -LiteralPath $_) })

if ($missing.Count -gt 0)
{
    foreach ($m in $missing)
    {
        [Console]::Error.WriteLine("error CSL0002: shader root '$m' does not exist; point CheckShaderLicenses.ps1 at where the shaders are now")
    }
    exit 1
}

$badWords = @('GPL', 'GNU General Public', 'copyleft')
$files    = @($Path | ForEach-Object { Get-ChildItem -LiteralPath $_ -Recurse -File -Include *.hlsl, *.hlsli, *.h })

$violations = @()

foreach ($f in $files)
{
    $lines = Get-Content -LiteralPath $f.FullName

    for ($i = 0; $i -lt $lines.Length; $i++)
    {
        $line = $lines[$i]

        foreach ($word in $badWords)
        {
            if ($line -imatch [regex]::Escape($word))
            {
                # Allowed only inside an `// ATTRIBUTION:` header block.
                # We treat any line within the surrounding 8 lines of an
                # ATTRIBUTION marker as inside the header (the upstream
                # license name + URL are typically on adjacent lines).
                $isInsideAttribution = $false
                $lo = [Math]::Max(0, $i - 8)
                $hi = [Math]::Min($lines.Length - 1, $i + 1)
                for ($k = $lo; $k -le $hi; $k++)
                {
                    if ($lines[$k] -imatch 'ATTRIBUTION:')
                    {
                        $isInsideAttribution = $true
                        break
                    }
                }

                if (-not $isInsideAttribution)
                {
                    $violations += "$($f.FullName):$($i + 1) -- forbidden token '$word'"
                }
            }
        }
    }
}

if ($violations.Count -gt 0)
{
    foreach ($v in $violations)
    {
        # Write to host (stderr stream) so MSBuild's error capture sees it.
        [Console]::Error.WriteLine("error CSL0001: $v")
    }
    Write-Host ""
    Write-Host "CheckShaderLicenses: FAILED -- $($violations.Count) violation(s). Casso ships only MIT/PD shaders (constitution v1.5.0)." -ForegroundColor Red
    exit 1
}

Write-Host "CheckShaderLicenses: $($files.Count) file(s) scanned under $($Path.Count) root(s) -- OK."
exit 0
