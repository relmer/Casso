<#
.SYNOPSIS
    Starts or stops one of CassoLink's MAME targets on Windows: a IIgs on its
    modem port, a IIgs with a Super Serial Card, a //c Plus, or an Apple ///.

.DESCRIPTION
    MAME is the one in %LOCALAPPDATA%\Programs\MAME, with the ROM sets in its
    roms folder. Each target has its serial port on a TCP socket at -Port:

      IIgsModem  apple2gs, the modem port connecting to a listening Wi-Fi
                 modem connection, which the universal disk dials.
      IIgsSsc    apple2gs with a Super Serial Card in slot 7, which the IIgs
                 has set to "Your card" by default. The card's socket listens,
                 since no modem answers a dial, and Casso connects to it with a
                 TCP connection once the universal disk waits. The card's ROM,
                 341-0065-A, is copied to MAME's roms\a2ssc from Casso's data
                 folder if it isn't there; scripts\FetchRoms.ps1 fetches it.
      IIcPlus    apple2cp, the modem port's socket listening as the IIgsSsc
                 target's does. Disk 2 is in the internal 3.5-inch drive, as
                 an 800K block device; the universal disk boots from the
                 external 5.25-inch drive, since a ProDOS volume with no PRODOS
                 file is passed over. MAME's //c with ROM 4 and the UniDisk
                 3.5 //c have no 3.5-inch drive, so they aren't targets.
      AppleIII   apple3, the serial port connecting to a listening Wi-Fi modem
                 connection.

    MAME runs until it is stopped, for an hour at most, and this script waits
    for it. The process ID is written to %TEMP%\CassoEmuTargets, so -Stop ends
    the MAME this script started and no other.

    CassoLinkIIgsTests starts and stops the IIgs targets through this script,
    and CassoLinkBlockEmulatorTests the //c Plus.

.PARAMETER Target
    IIgsModem, IIgsSsc, IIcPlus or AppleIII.

.PARAMETER Disk1
    The disk image for drive 1.

.PARAMETER Disk2
    The disk image for drive 2.

.PARAMETER Port
    The TCP port.

.PARAMETER Headless
    No window and no sound.

.PARAMETER Stop
    Ends the MAME this script started for the target.

.EXAMPLE
    scripts\EmuTargets\Mame.ps1 -Target IIgsSsc -Disk1 universal.dsk -Disk2 data.dsk -Port 19802
#>

[CmdletBinding()]
param (
    [Parameter (Mandatory)]
    [ValidateSet ('IIgsModem', 'IIgsSsc', 'IIcPlus', 'AppleIII')]
    [string] $Target,
    [string] $Disk1,
    [string] $Disk2,
    [int]    $Port = 19802,
    [switch] $Headless,
    [switch] $Stop
)

$ErrorActionPreference = 'Stop'

$mame    = Join-Path $env:LOCALAPPDATA 'Programs\MAME\mame.exe'
$pidFile = Join-Path $env:TEMP "CassoEmuTargets\mame-$Target.pid"





if ($Stop)
{
    if (Test-Path $pidFile)
    {
        $id = [int] (Get-Content $pidFile)

        Get-Process -Id $id -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -eq 'mame' } | Stop-Process -Force
        Remove-Item $pidFile
    }

    return
}

if (-not (Test-Path $mame))
{
    throw "MAME isn't installed at $mame."
}

if (-not $Disk1 -or -not $Disk2)
{
    throw 'Give -Disk1 and -Disk2.'
}

switch ($Target)
{
    'IIgsModem' { $machine = @('apple2gs', '-modem', 'null_modem', '-bitb', "socket.127.0.0.1:$Port") }
    'IIgsSsc'   { $machine = @('apple2gs', '-sl7', 'ssc', '-sl7:ssc:rs232', 'null_modem', '-bitb', "socket.0.0.0.0:$Port") }
    'IIcPlus'   { $machine = @('apple2cp', '-modem', 'null_modem', '-bitb', "socket.0.0.0.0:$Port") }
    'AppleIII'  { $machine = @('apple3', '-rs232', 'null_modem', '-bitb', "socket.127.0.0.1:$Port") }
}

if ($Target -eq 'IIgsSsc')
{
    $sscRom = Join-Path (Split-Path $mame) 'roms\a2ssc\341-0065-a.bin'

    if (-not (Test-Path $sscRom))
    {
        $cassoRom = Join-Path $PSScriptRoot '..\..\CassoData\Devices\SuperSerialCard\SSC.rom'

        if (-not (Test-Path $cassoRom))
        {
            throw "The Super Serial Card's ROM isn't in MAME's roms\a2ssc or in CassoData. Run scripts\FetchRoms.ps1."
        }

        New-Item -ItemType Directory -Force (Split-Path $sscRom) | Out-Null
        Copy-Item $cassoRom $sscRom
    }
}

$secondDrive = ($Target -eq 'IIcPlus') ? '-flop3' : '-flop2'
$arguments   = $machine + @('-flop1', "`"$((Resolve-Path $Disk1).Path)`"", $secondDrive, "`"$((Resolve-Path $Disk2).Path)`"",
                            '-skip_gameinfo', '-seconds_to_run', '3600')

if ($Headless)
{
    $arguments += @('-video', 'none', '-sound', 'none')
}

New-Item -ItemType Directory -Force (Split-Path $pidFile) | Out-Null

$process = Start-Process $mame -ArgumentList $arguments -WorkingDirectory (Split-Path $mame) -PassThru -WindowStyle ($Headless ? 'Hidden' : 'Normal')

Set-Content $pidFile $process.Id
$process.WaitForExit()
