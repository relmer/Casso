<#
.SYNOPSIS
    Builds, starts or stops one of CassoLink's emulator targets in a WSL
    instance: KEGS, GSplus, MAME's LANceGS or AppleWin's Uthernet.

.DESCRIPTION
    The targets run headless in a WSL instance, Ubuntu-CassoEmu by default,
    through the scripts in `wsl/`, which this script runs as root with the
    folder's path as the instance sees it. No packet capture driver is
    installed: MAME's LANceGS uses a TAP device in the instance, and
    AppleWin's Uthernet uses libslirp.

    -Setup builds the target in the instance (wsl/setup.sh). KEGS and GSplus
    take the IIgs's ROM 03, which is dumped from Windows MAME's apple2gs
    through DumpIIgsRom.lua, since MAME's two ROM files joined end to end
    are not the ROM's image. The LANceGS target takes Windows MAME's roms
    folder.

    Without -Setup or -Stop, the target is started with the two disks in
    drives 1 and 2 (wsl/run.sh) and runs until it is stopped, for an hour at
    most. KEGS and GSplus connect their modem port to the Windows host at
    -Port; the Windows host's address is the instance's default route. The
    network cards take an address by DHCP or slirp, and UDP 6502 on the
    instance's address is forwarded to the Apple; -Address writes that
    address.

    -Stop ends every target in the instance.

    CassoLinkIIgsTests and CassoLinkEthernetTests start and stop the targets
    through this script.

.PARAMETER Target
    Kegs, Gsplus, LanceGs or Uthernet.

.PARAMETER Disk1
    The disk image for drive 1, usually the universal boot disk.

.PARAMETER Disk2
    The disk image for drive 2.

.PARAMETER Port
    The Windows host's TCP port KEGS and GSplus connect their modem port to.

.PARAMETER Setup
    Builds the target in the instance instead of starting it.

.PARAMETER Stop
    Ends every target in the instance.

.PARAMETER Address
    Writes the instance's address, the one the network cards are reached at.

.PARAMETER Instance
    The WSL instance. Ubuntu-CassoEmu by default.

.EXAMPLE
    scripts\EmuTargets\Wsl.ps1 -Target Kegs -Setup
    scripts\EmuTargets\Wsl.ps1 -Target Kegs -Disk1 universal.dsk -Disk2 data.dsk -Port 19802
#>

[CmdletBinding()]
param (
    [ValidateSet ('Kegs', 'Gsplus', 'LanceGs', 'Uthernet')]
    [string] $Target,
    [string] $Disk1,
    [string] $Disk2,
    [int]    $Port     = 19802,
    [switch] $Setup,
    [switch] $Stop,
    [switch] $Address,
    [string] $Instance = 'Ubuntu-CassoEmu'
)

$ErrorActionPreference = 'Stop'





function Invoke-Wsl ([string[]] $Arguments)
{
    & wsl.exe -d $Instance -u root -- @Arguments
}




function ConvertTo-WslPath ([string] $Path)
{
    $full = (Resolve-Path -LiteralPath $Path).Path
    (Invoke-Wsl @('wslpath', '-a', $full.Replace('\', '/'))).Trim()
}




function Get-IIgsRom
{
    $mame   = Join-Path $env:LOCALAPPDATA 'Programs\MAME\mame.exe'
    $script = Join-Path $PSScriptRoot 'DumpIIgsRom.lua'
    $rom    = Join-Path $env:TEMP 'CassoIIgsRom\ROM.03'

    if (-not (Test-Path $mame))
    {
        throw "MAME isn't installed at $mame, and its apple2gs is the source of the IIgs ROM."
    }

    New-Item -ItemType Directory -Force (Split-Path $rom) | Out-Null
    $env:CASSO_IIGS_ROM_OUT = $rom

    Push-Location (Split-Path $mame)

    try
    {
        & $mame apple2gs -video none -sound none -skip_gameinfo -seconds_to_run 20 -autoboot_script $script | Out-Null
    }
    finally
    {
        Pop-Location
    }

    if (-not (Test-Path $rom) -or (Get-Item $rom).Length -ne 262144)
    {
        throw "MAME didn't write the 256 KB IIgs ROM to $rom."
    }

    return $rom
}




$scripts = ConvertTo-WslPath (Join-Path $PSScriptRoot 'wsl')

if ($Address)
{
    ((Invoke-Wsl @('hostname', '-I')) -split ' ')[0]
    return
}

if ($Stop)
{
    Invoke-Wsl @('sh', "$scripts/run.sh", 'stop')
    return
}

if (-not $Target)
{
    throw 'Name a -Target: Kegs, Gsplus, LanceGs or Uthernet.'
}

$name = $Target.ToLowerInvariant()

if ($Setup)
{
    $extra = @()

    switch ($Target)
    {
        { $_ -in 'Kegs', 'Gsplus' } { $extra = @(ConvertTo-WslPath (Get-IIgsRom)) }
        'LanceGs'                   { $extra = @(ConvertTo-WslPath (Join-Path $env:LOCALAPPDATA 'Programs\MAME\roms')) }
    }

    Invoke-Wsl (@('sh', "$scripts/setup.sh", $name, $scripts) + $extra)
    exit $LASTEXITCODE
}

if (-not $Disk1 -or -not $Disk2)
{
    throw 'Give -Disk1 and -Disk2.'
}

$arguments = @('sh', "$scripts/run.sh", $name, (ConvertTo-WslPath $Disk1), (ConvertTo-WslPath $Disk2))

if ($Target -in 'Kegs', 'Gsplus')
{
    $route     = Invoke-Wsl @('ip', 'route', 'show', 'default')
    $hostIp    = ($route -split ' ')[2]
    $arguments += @($hostIp, "$Port")
}

Invoke-Wsl $arguments
exit $LASTEXITCODE
