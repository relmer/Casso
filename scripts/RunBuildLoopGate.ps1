<#
.SYNOPSIS
    Runs the build loop end to end -- assemble, place, list, set the boot
    program, launch -- and checks that it is one invocation per step, no
    third-party tool, inside the ten-second budget.

.DESCRIPTION
    Two claims about this feature were only ever asserted in prose. This script
    is where they are measured.

    "A single command invocation per step and no third-party tool" is checked
    mechanically rather than by inspection: every step names one executable and
    an argument vector, both executables are resolved inside this repository's
    own build output, and a step whose arguments carry shell punctuation is
    refused before it runs. A loop that quietly needed a pipeline, a helper, or
    something off the PATH could not be expressed here.

    "Under ten seconds, source to program running" is a wall clock over two
    parts. The first is everything this toolchain controls: the four
    command-line steps and the emulator's launch. The second is the guest's own
    boot, which is a property of the disk and the controller rather than of the
    build tools, and is a MEASURED number -- the emulated cycles a DOS 3.3 boot
    spends reaching a BRUN'd binary, divided by the machine's clock. Both are
    reported, because only the first is something a change here can improve.

    WHAT THE LAUNCH STEP DOES AND DOES NOT PROVE. It proves the emulator
    started, mounted the image, and was still running after the guest had had
    time to boot it and run the program. It does NOT read the guest's screen.
    What the guest makes of a placed program is settled by the boot-gated tests
    in the test assembly, which drive a real 6502 over images built by the same
    runner this script drives through the command line, and by the capture
    recorded in the feature's task notes.

    Two traps the help text warns about are checked here as well, because a
    warning nothing exercises is a warning that can quietly stop being true.

.PARAMETER Configuration
    Which build to drive. Default: Release, which is what the budget is about.

.PARAMETER Platform
    Which build to drive. Default: x64.

.PARAMETER BudgetSeconds
    The wall-clock budget for the whole loop. Default: 10.

.PARAMETER WorkDir
    Where to build the loop's files. Default: a fresh directory under TEMP,
    removed on the way out.

.PARAMETER SkipLaunch
    Run the command-line steps only. For a machine with no interactive desktop;
    the run then reports that the budget was not measured rather than passing
    a partial measurement off as a whole one.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',

    [ValidateRange(1, 600)]
    [int]$BudgetSeconds = 10,

    [string]$WorkDir,

    [switch]$SkipLaunch
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$cli      = Join-Path $repoRoot "$Platform\$Configuration\CassoCli.exe"
$casso    = Join-Path $repoRoot "$Platform\$Configuration\Casso.exe"
$master   = Join-Path $env:LOCALAPPDATA 'Casso\Disks\DOS 3.3 System Master.dsk'

# The emulated cost of a DOS 3.3 boot reaching a BRUN'd binary, and the clock
# it is spent at. Both are measured elsewhere in this feature and are quoted
# rather than guessed: the cycle figure is what the direct-boot gate compares
# against, and the rate is the machine's own NTSC clock.
$kDos33BootCycles = 6366505
$kGuestClockHz    = 1020484.0

# A DOS 3.3 file occupies whole sectors plus one track/sector list, and its
# stored form carries a four-byte header ahead of the payload.
$kBytesPerSector   = 256
$kDosBinHeaderSize = 4

$failures = New-Object System.Collections.Generic.List[string]
$notes    = New-Object System.Collections.Generic.List[string]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Add-Failure {
    param([string]$Message)
    $failures.Add($Message) | Out-Null
    Write-Host "  FAIL  $Message" -ForegroundColor Red
}

function Assert-That {
    param($Condition, [string]$Message)
    if ($Condition -eq $true) {
        Write-Host "  ok    $Message" -ForegroundColor DarkGray
    } else {
        Add-Failure $Message
    }
}

function Assert-CassoOwnedExecutable {
    param([string]$Path)

    $resolved = [System.IO.Path]::GetFullPath($Path)
    $inRepo   = $resolved.StartsWith([System.IO.Path]::GetFullPath($repoRoot), 'OrdinalIgnoreCase')
    $leaf     = Split-Path $resolved -Leaf

    Assert-That ($inRepo -and ($leaf -in @('CassoCli.exe', 'Casso.exe'))) `
        "step runs this repository's own $leaf, not a third-party tool"
}

function Assert-NoShellPunctuation {
    param([string[]]$Arguments)

    # "One invocation" has to mean something a script cannot cheat. Anything
    # here that needed a pipeline, a redirect, or a second command would have
    # to smuggle the operator through an argument.
    $offenders = @($Arguments | Where-Object { $_ -match '[|&><;]' })

    Assert-That ($offenders.Count -eq 0) `
        ("step is a single invocation with no shell operators" +
         $(if ($offenders.Count) { " (found: $($offenders -join ', '))" } else { "" }))
}

function Invoke-Step {
    param(
        [string]$Name,
        [string]$Exe,
        [string[]]$Arguments,
        [int]$ExpectedExit
    )

    Write-Host "[$Name] $(Split-Path $Exe -Leaf) $($Arguments -join ' ')" -ForegroundColor Cyan

    Assert-CassoOwnedExecutable $Exe
    Assert-NoShellPunctuation $Arguments

    $outFile = Join-Path $work "$Name.out"
    $errFile = Join-Path $work "$Name.err"

    $proc = Start-Process -FilePath $Exe -ArgumentList $Arguments -NoNewWindow -Wait -PassThru `
                          -RedirectStandardOutput $outFile -RedirectStandardError $errFile

    $result = [pscustomobject]@{
        Name   = $Name
        Exit   = $proc.ExitCode
        Stdout = (Get-Content $outFile -Raw -ErrorAction SilentlyContinue)
        Stderr = (Get-Content $errFile -Raw -ErrorAction SilentlyContinue)
    }

    Assert-That ($result.Exit -eq $ExpectedExit) `
        "[$Name] exit $($result.Exit), expected $ExpectedExit"

    return $result
}


# ---------------------------------------------------------------------------
# Prerequisites -- absent means FAIL, never skip
# ---------------------------------------------------------------------------

Write-Host "Build loop gate -- $Configuration $Platform" -ForegroundColor White

foreach ($needed in @($cli, $casso, $master)) {
    if (-not (Test-Path $needed)) {
        throw "Required input missing: $needed"
    }
}

if (-not $WorkDir) {
    $WorkDir = Join-Path $env:TEMP ("CassoBuildLoop-" + [Guid]::NewGuid().ToString('N').Substring(0, 8))
}

$work = $WorkDir
New-Item -ItemType Directory -Force -Path $work | Out-Null

$source  = Join-Path $work 'prog.a65'
$binary  = Join-Path $work 'prog.bin'
$greeting = Join-Path $work 'greet.bas'
$image   = Join-Path $work 'mydisk.dsk'

# A few kilobytes, which is the size SC-006 names. The program prints through
# the monitor's own character output so a human watching the launch step sees
# it land, and returns so the greeting that called it can finish.
$progText = @'
        .org $6000
        LDX #$00
LOOP    LDA BANNER,X
        BEQ DONE
        ORA #$80
        JSR $FDED
        INX
        BNE LOOP
DONE    RTS
BANNER  .byte "CASSO BUILD LOOP", $0D, $00
        .ds 2048
        .byte $60
'@

Set-Content -Path $source -Value $progText -Encoding ascii
Set-Content -Path $greeting -Value '10 PRINT CHR$(4);"BRUN PROG"' -Encoding ascii
Copy-Item $master $image -Force

try {

# ---------------------------------------------------------------------------
# The loop
# ---------------------------------------------------------------------------

$clock = [System.Diagnostics.Stopwatch]::StartNew()

$assemble = Invoke-Step -Name 'assemble' -Exe $cli -ExpectedExit 0 -Arguments @(
    $source, '-o', $binary, '--raw')

$payloadBytes = (Get-Item $binary).Length
$dataSectors  = [Math]::Ceiling(($payloadBytes + $kDosBinHeaderSize) / $kBytesPerSector)
$expectedRow  = ' B {0:000} PROG' -f ($dataSectors + 1)

Assert-That ($payloadBytes -gt 2000) "assembled a few-kilobyte program ($payloadBytes bytes)"

# --raw, not --dos-bin. put writes the DOS 3.3 header itself, and a file that
# already carries one has its own header loaded where the code should be. The
# trap section below shows what that costs.
$firstFour = [IO.File]::ReadAllBytes($binary)[0..3]
Assert-That (-not ($firstFour[0] -eq 0x00 -and $firstFour[1] -eq 0x60)) `
    "the assembled file carries no DOS 3.3 header for put to double"

$putProgram = Invoke-Step -Name 'put-program' -Exe $cli -ExpectedExit 0 -Arguments @(
    'disk', 'put', $image, $binary, '--as', 'PROG', '--type', 'B', '--addr', '$6000')

$list = Invoke-Step -Name 'list' -Exe $cli -ExpectedExit 0 -Arguments @(
    'disk', 'list', $image)

# The whole rendered row, not the name. A DOS 3.3 catalog name may hold a
# colon and a backslash, so an implementation that placed the host path
# verbatim would produce a listing containing "PROG" too.
$listRows = @($list.Stdout -split "`r?`n")
Assert-That ($listRows -contains $expectedRow) `
    "the listing shows '$expectedRow'"

$putGreeting = Invoke-Step -Name 'put-greeting' -Exe $cli -ExpectedExit 0 -Arguments @(
    'disk', 'put', $image, $greeting, '--as', 'STARTUP', '--basic')

$boot = Invoke-Step -Name 'boot' -Exe $cli -ExpectedExit 0 -Arguments @(
    'disk', 'boot', $image, 'STARTUP')

$toolElapsed = $clock.Elapsed.TotalSeconds

# ---------------------------------------------------------------------------
# Launch
# ---------------------------------------------------------------------------

$guestBootSeconds = $kDos33BootCycles / $kGuestClockHz
$launched         = $null
$launchElapsed    = 0.0

if ($SkipLaunch) {
    $notes.Add("launch skipped -- the budget was NOT measured this run") | Out-Null
} else {
    Write-Host "[launch] $(Split-Path $casso -Leaf) --disk1 $image" -ForegroundColor Cyan
    Assert-CassoOwnedExecutable $casso
    Assert-NoShellPunctuation @('--disk1', $image)

    $launched = Start-Process -FilePath $casso -ArgumentList @('--disk1', $image) -PassThru

    $windowDeadline = (Get-Date).AddSeconds(30)
    while ($launched.MainWindowHandle -eq 0 -and (Get-Date) -lt $windowDeadline) {
        Start-Sleep -Milliseconds 100
        $launched.Refresh()
    }

    Assert-That ($launched.MainWindowHandle -ne 0) "the emulator came up with a window"
    $launchElapsed = $clock.Elapsed.TotalSeconds - $toolElapsed

    # Give the guest the boot it is owed, then confirm the emulator is still
    # there. A launch that reported success and died during the boot would
    # otherwise be indistinguishable from one that ran the program.
    Start-Sleep -Seconds ([Math]::Ceiling($guestBootSeconds))
    $launched.Refresh()

    Assert-That (-not $launched.HasExited) "the emulator was still running after the guest's boot"
}

$totalElapsed = $toolElapsed + $launchElapsed + $guestBootSeconds

# ---------------------------------------------------------------------------
# The two traps the help text warns about
# ---------------------------------------------------------------------------

Write-Host "Documented traps" -ForegroundColor White

$trapImage = Join-Path $work 'trap.dsk'
Copy-Item $master $trapImage -Force

$trapPut = Invoke-Step -Name 'trap-put' -Exe $cli -ExpectedExit 0 -Arguments @(
    'disk', 'put', $trapImage, $binary, '--as', 'PROG', '--type', 'B', '--addr', '$6000')

$trapBoot = Invoke-Step -Name 'trap-boot-a-binary' -Exe $cli -ExpectedExit 1 -Arguments @(
    'disk', 'boot', $trapImage, 'PROG')

$namedTheReason = [bool]([string]$trapBoot.Stderr -match 'RUNs its greeting')

Assert-That $namedTheReason `
    "naming a binary as the boot program is refused with the reason, not silently accepted"

$dosBin = Join-Path $work 'prog.dosbin'

$trapAssemble = Invoke-Step -Name 'trap-assemble' -Exe $cli -ExpectedExit 0 -Arguments @(
    $source, '-o', $dosBin, '--dos-bin')

$dosBinHead = [IO.File]::ReadAllBytes($dosBin)[0..1]

Assert-That ($dosBinHead[0] -eq 0x00 -and $dosBinHead[1] -eq 0x60) `
    "--dos-bin writes a header of its own, which is why put wants --raw"

# ---------------------------------------------------------------------------
# Verdict
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host ("Command line steps : {0,6:N2} s" -f $toolElapsed)
Write-Host ("Emulator launch    : {0,6:N2} s" -f $launchElapsed)
Write-Host ("Guest boot (measured, {0} cycles at {1:N0} Hz) : {2,6:N2} s" -f `
            $kDos33BootCycles, $kGuestClockHz, $guestBootSeconds)
Write-Host ("Total              : {0,6:N2} s  against a {1} s budget" -f $totalElapsed, $BudgetSeconds)

if (-not $SkipLaunch) {
    Assert-That ($totalElapsed -lt $BudgetSeconds) `
        ("the whole loop fits the budget: {0:N2} s < {1} s" -f $totalElapsed, $BudgetSeconds)
}

}
finally {
    if ($launched -and -not $launched.HasExited) {
        # Only ever the process this script started. Other Casso instances on
        # this machine belong to somebody else.
        Stop-Process -Id $launched.Id -Force -ErrorAction SilentlyContinue
    }

    if (-not $PSBoundParameters.ContainsKey('WorkDir')) {
        Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Host ""

foreach ($note in $notes) {
    Write-Host "NOTE: $note" -ForegroundColor Yellow
}

if ($failures.Count -gt 0) {
    Write-Host "BUILD LOOP GATE FAILED -- $($failures.Count) check(s)" -ForegroundColor Red
    exit 1
}

if ($SkipLaunch) {
    Write-Host "Command-line steps passed; the budget was not measured (-SkipLaunch)" -ForegroundColor Yellow
    exit 2
}

Write-Host "BUILD LOOP GATE PASSED" -ForegroundColor Green
exit 0
