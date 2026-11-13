<#
.SYNOPSIS
    Boots a disk image in MAME headlessly and saves a picture of the screen.

.DESCRIPTION
    Runs MAME with no window and no sound, at full speed, with the image in
    the first floppy drive, and saves a PNG of the screen after the given
    number of emulated seconds. Used to check that a disk Casso made or
    changed starts a machine Casso doesn't emulate, such as an Apple ///.

    MAME needs the machine's ROMs in its roms folder. MAME warns about ROMs
    whose checksums differ from the dumps it lists, and runs them.

.PARAMETER Machine
    MAME's name for the machine: apple3, apple2ee, apple2gs.

.PARAMETER Disk
    The image to boot.

.PARAMETER Out
    Where to save the PNG.

.PARAMETER Seconds
    Emulated seconds before the picture. Defaults to 20.

.PARAMETER Mame
    The path of mame.exe. Defaults to %LOCALAPPDATA%\Programs\MAME\mame.exe.

.EXAMPLE
    scripts/MameSnapshot.ps1 -Machine apple3 -Disk sos.po -Out sos.png
#>

param (
    [Parameter (Mandatory)] [string] $Machine,
    [Parameter (Mandatory)] [string] $Disk,
    [Parameter (Mandatory)] [string] $Out,
    [int]    $Seconds = 20,
    [string] $Mame    = (Join-Path $env:LOCALAPPDATA 'Programs\MAME\mame.exe')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Mame)) {
    Write-Error "MAME isn't at $Mame"
}

$disk = (Resolve-Path -LiteralPath $Disk).Path
$work = Join-Path ([IO.Path]::GetTempPath()) ("CassoMame-" + [Guid]::NewGuid().ToString('N'))
$lua  = Join-Path $work 'snapshot.lua'

New-Item -ItemType Directory -Path $work | Out-Null

@"
local done = false
local notifier = emu.add_machine_frame_notifier(function()
    if not done and manager.machine.time.seconds > $Seconds then
        done = true
        manager.machine.video:snapshot()
        manager.machine:exit()
    end
end)
"@ | Set-Content -LiteralPath $lua

#  An empty slot 4 leaves out the //e's Mockingboard, whose speech ROM is
#  often missing.
$slots = if ($Machine -like 'apple2*') { @('-sl4', '') } else { @() }

Push-Location (Split-Path -Parent $Mame)

try {
    & $Mame $Machine -flop1 $disk @slots -video none -sound none -nothrottle -skip_gameinfo `
        -snapshot_directory $work -autoboot_script $lua -seconds_to_run ($Seconds + 40) 2>&1 | Out-Null
}
finally {
    Pop-Location
}

$png = Get-ChildItem -LiteralPath $work -Recurse -Filter *.png | Select-Object -First 1

if (-not $png) {
    Remove-Item -LiteralPath $work -Recurse -Force
    Write-Error "MAME saved no picture of $Machine booting $Disk"
}

Move-Item -LiteralPath $png.FullName -Destination $Out -Force
Remove-Item -LiteralPath $work -Recurse -Force

Write-Host "Saved $Out"
