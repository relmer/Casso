<#
.SYNOPSIS
    Boots a disk image in MAME headlessly and saves a picture of the screen.

.DESCRIPTION
    Runs MAME with no window and no sound, at full speed, with the image in
    the first floppy drive or on a hard disk card in slot 7, and saves a PNG
    of the screen after the given number of emulated seconds. Used to check that a disk Casso made or
    changed starts a machine Casso doesn't emulate, such as an Apple ///.

    MAME needs the machine's ROMs in its roms folder. MAME warns about ROMs
    whose checksums differ from the dumps it lists, and runs them.

.PARAMETER Machine
    MAME's name for the machine: apple3, apple2ee, apple2gs.

.PARAMETER Disk
    The floppy image to boot.
.PARAMETER Drive
    MAME's name for the drive the floppy goes in: flop1 for the first 5.25"
    drive, flop3 for the first 3.5" drive of a //c Plus or a IIgs. Defaults
    to flop1.
.PARAMETER HardDisk
    A hard disk image (.hdv, .2mg, .hd or .chd) on the card in slot 7.
.PARAMETER Card
    MAME's name for the slot 7 card the hard disk is on: scsi for Apple's II
    SCSI Card, hsscsi for its High-Speed SCSI Card, cffa2 for a CFFA 2.0.
    Defaults to scsi.

.PARAMETER Out
    Where to save the PNG.

.PARAMETER Keys
    Keys typed after KeysAt emulated seconds, such as a menu choice or a
    command and a return.
.PARAMETER KeysAt
    Emulated seconds before the keys are typed. Defaults to 10.
.PARAMETER Seconds
    Emulated seconds before the picture. Defaults to 20.

.PARAMETER Mame
    The path of mame.exe. Defaults to %LOCALAPPDATA%\Programs\MAME\mame.exe.

.EXAMPLE
    scripts/MameSnapshot.ps1 -Machine apple3 -Disk sos.po -Out sos.png
.EXAMPLE
    scripts/MameSnapshot.ps1 -Machine apple2ee -HardDisk big.hdv -Out big.png
#>

param (
    [Parameter (Mandatory)] [string] $Machine,
    [string] $Disk     = '',
    [string] $Drive    = 'flop1',
    [string] $HardDisk = '',
    [string] $Card     = 'scsi',
    [Parameter (Mandatory)] [string] $Out,
    [string] $Keys     = '',
    [int]    $KeysAt   = 10,
    [int]    $Seconds  = 20,
    [string] $Mame    = (Join-Path $env:LOCALAPPDATA 'Programs\MAME\mame.exe')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Mame)) {
    Write-Error "MAME isn't at $Mame"
}

if (-not $Disk -and -not $HardDisk) {
    Write-Error 'A floppy image, a hard disk image or both is required'
}

$media = @()

if ($Disk) {
    $media += @("-$Drive", (Resolve-Path -LiteralPath $Disk).Path)
}

if ($HardDisk) {
    #  On a card with two drives, as the CFFA 2.0 has, MAME's option for the
    #  first is hard1.
    $hard   = if ($Card -like 'cffa*') { '-hard1' } else { '-hard' }
    $media += @('-sl7', $Card, $hard, (Resolve-Path -LiteralPath $HardDisk).Path)
}
$work = Join-Path ([IO.Path]::GetTempPath()) ("CassoMame-" + [Guid]::NewGuid().ToString('N'))
$lua  = Join-Path $work 'snapshot.lua'

New-Item -ItemType Directory -Path $work | Out-Null

@"
local done = false
local typed = false
local keys = [==[$Keys]==]
local notifier = emu.add_machine_frame_notifier(function()
    if not typed and keys ~= '' and manager.machine.time.seconds > $KeysAt then
        typed = true
        manager.machine.natkeyboard.in_use = true
        manager.machine.natkeyboard:post(keys)
    end
    if not done and manager.machine.time.seconds > $Seconds then
        done = true
        manager.machine.video:snapshot()
        manager.machine:exit()
    end
end)
"@ | Set-Content -LiteralPath $lua

#  An empty slot 4 leaves out the //e's Mockingboard, whose speech ROM is
#  often missing. A //c has no slots.
$slots = if ($Machine -like 'apple2*' -and $Machine -notlike 'apple2c*') { @('-sl4', '') } else { @() }

Push-Location (Split-Path -Parent $Mame)

try {
    & $Mame $Machine @media @slots -video none -sound none -nothrottle -skip_gameinfo `
        -snapshot_directory $work -autoboot_script $lua -seconds_to_run ($Seconds + 40) 2>&1 | Out-Null
}
finally {
    Pop-Location
}

$png = Get-ChildItem -LiteralPath $work -Recurse -Filter *.png | Select-Object -First 1

if (-not $png) {
    Remove-Item -LiteralPath $work -Recurse -Force
    Write-Error "MAME didn't save a picture of $Machine booting $Disk$HardDisk"
}

Move-Item -LiteralPath $png.FullName -Destination $Out -Force
Remove-Item -LiteralPath $work -Recurse -Force

Write-Host "Saved $Out"
