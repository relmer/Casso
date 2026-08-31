<#
.SYNOPSIS
    Keeps the long, greedy scripts off the foreground's back.

.DESCRIPTION
    A full build saturates every core and the disk with it, and on a laptop
    that is the difference between a machine you can keep working on and one
    that stops answering the mouse for two minutes. None of this work is
    latency-sensitive -- nobody watches a build -- so it has no business
    competing with the editor, the browser, or the emulator being tested.

    Priority is set on the RUNNING SCRIPT's own process rather than on the
    tool it launches, because Windows hands a child its parent's priority
    class at creation. Setting it here therefore reaches MSBuild, and every
    cl.exe MSBuild fans out, and vstest and its hosts -- none of which is
    reachable by Start-Process, which has no priority parameter at all.

    BELOW NORMAL, not Idle. Idle means a build makes no progress whatever
    while anything else wants the CPU, which on a busy desktop can mean
    almost none; BelowNormal yields the moment the foreground asks and
    otherwise runs flat out. Measured on an idle machine the two are the
    same speed, and this way a background build still finishes while you use
    the machine rather than only when you stop.

    -Idle is offered for the extreme case (a long Harte or mutation run you
    genuinely do not care about finishing soon), and every script exposes
    -NormalPriority to opt out entirely -- CI has no foreground to protect
    and should not pay even BelowNormal's scheduling quantum.

.NOTES
    Restoring matters. These scripts are usually run as `.\Build.ps1`, whose
    process exits and takes the setting with it -- but they are also
    dot-sourced and run from an interactive session, and a shell left at
    BelowNormal for the rest of the day is a slow shell nobody can explain.
    Use-CassoHostLoad restores in a finally, so an error or a Ctrl+C puts it
    back.
#>

function Set-CassoHostLoad {
    param(
        [ValidateSet('BelowNormal', 'Idle', 'Normal')]
        [string] $Priority = 'BelowNormal'
    )

    $process = Get-Process -Id $PID
    $was     = $process.PriorityClass

    try {
        $process.PriorityClass = $Priority
    }
    catch {
        #  Not fatal, ever. A denied priority change is a slow build, and a
        #  script that refuses to build because it could not be polite is
        #  worse than one that builds rudely.
        Write-Host "Could not lower process priority: $($_.Exception.Message)" -ForegroundColor DarkYellow
        return $was
    }

    return $was
}





function Restore-CassoHostLoad {
    param(
        #  NOT mandatory, and null is the ordinary case rather than a
        #  mistake: with -NormalPriority nothing was lowered, so there is
        #  nothing to put back -- and the trap that calls this fires on that
        #  path too. A restore that throws while unwinding an error would
        #  replace the real failure with a binding complaint about itself.
        $Priority = $null
    )

    if ($null -eq $Priority) { return }

    try   { (Get-Process -Id $PID).PriorityClass = $Priority }
    catch { }
}





#
#  The wrapper the scripts actually use: lower, run, restore whatever
#  happened. `-Normal` is the opt-out and runs the block untouched.
#
function Use-CassoHostLoad {
    param(
        [Parameter(Mandatory = $true)][scriptblock] $Body,
        [switch]  $Normal,
        [switch]  $Idle
    )

    if ($Normal) {
        & $Body
        return
    }

    $was = Set-CassoHostLoad -Priority ($Idle ? 'Idle' : 'BelowNormal')

    try     { & $Body }
    finally { Restore-CassoHostLoad -Priority $was }
}
