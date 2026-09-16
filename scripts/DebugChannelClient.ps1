<#
.SYNOPSIS
    Talks to a running Casso's debug channel with nothing but a named pipe.

.DESCRIPTION
    An independent client for the debug channel, written from docs/DebugChannel.md
    alone and using only System.IO.Pipes.NamedPipeClientStream -- no Casso code,
    no CassoCli. It exists to show the protocol document is enough to build a
    client from (success criterion SC-007), and doubles as a quick way to poke a
    running instance.

    It sends hello, sets a breakpoint, resumes the machine, and waits for the
    stopped notification the run produces, printing every record it receives.

.PARAMETER ProcessId
    The Casso process to connect to. Start Casso with --debugger first.

.PARAMETER Breakpoint
    The address to stop at, in hex. Defaults to FDED (COUT), which almost
    anything running on an Apple II reaches quickly.

.PARAMETER TimeoutSeconds
    How long to wait for each record before giving up.

.EXAMPLE
    scripts\DebugChannelClient.ps1 -ProcessId 20044

.NOTES
    Exits 0 when the stop arrives, 1 when a record did not arrive in time or the
    breakpoint was not set, and 2 when the pipe could not be opened.
#>

param(
    [Parameter (Mandatory = $true)]
    [int]    $ProcessId,

    [string] $Breakpoint     = 'FDED',

    [int]    $TimeoutSeconds = 10
)

$ErrorActionPreference = 'Stop'





function Read-Record {
    param ([System.IO.StreamReader] $Reader, [int] $TimeoutSeconds)

    $task = $Reader.ReadLineAsync()

    if (-not $task.Wait($TimeoutSeconds * 1000)) {
        return $null
    }

    $line = $task.Result

    if ($null -eq $line) {
        return $null
    }

    Write-Host "< $line"
    return ($line | ConvertFrom-Json)
}





function Send-Record {
    param ([System.IO.StreamWriter] $Writer, [hashtable] $Record)

    $json = $Record | ConvertTo-Json -Compress
    Write-Host "> $json"
    $Writer.WriteLine($json)
}





function Wait-For {
    param ([System.IO.StreamReader] $Reader, [int] $TimeoutSeconds, [scriptblock] $IsWanted)

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)

    while ((Get-Date) -lt $deadline) {
        $record = Read-Record -Reader $Reader -TimeoutSeconds ([Math]::Max(1, [int] ($deadline - (Get-Date)).TotalSeconds))

        if ($null -eq $record) {
            return $null
        }

        if (& $IsWanted $record) {
            return $record
        }
    }

    return $null
}





$pipe = New-Object System.IO.Pipes.NamedPipeClientStream('.', "Casso.Debug.$ProcessId", [System.IO.Pipes.PipeDirection]::InOut)

try {
    $pipe.Connect($TimeoutSeconds * 1000)
}
catch {
    Write-Error "Could not open \\.\pipe\Casso.Debug.$ProcessId. Is Casso running with --debugger? $($_.Exception.Message)"
    exit 2
}

try {
    $writer           = New-Object System.IO.StreamWriter($pipe)
    $writer.AutoFlush = $true
    $writer.NewLine   = "`n"
    $reader           = New-Object System.IO.StreamReader($pipe)

    Send-Record -Writer $writer -Record @{ type = 'hello'; id = 1; client = 'DebugChannelClient.ps1'; protocol = 1 }

    $hello = Wait-For -Reader $reader -TimeoutSeconds $TimeoutSeconds -IsWanted { param ($r) $r.type -eq 'hello' }

    if ($null -eq $hello) {
        Write-Error 'No hello reply arrived.'
        exit 1
    }

    Write-Host "Connected to pid $($hello.pid), $($hello.machine), $($hello.state)."

    Send-Record -Writer $writer -Record @{ type = 'command'; id = 2; line = "bp $Breakpoint" }

    $set = Wait-For -Reader $reader -TimeoutSeconds $TimeoutSeconds -IsWanted { param ($r) $r.type -eq 'reply' -and $r.id -eq 2 }

    if ($null -eq $set -or $set.status -ne 'ok') {
        Write-Error 'The breakpoint was not set.'
        exit 1
    }

    Send-Record -Writer $writer -Record @{ type = 'command'; id = 3; line = 'g' }

    #  The stop may come before the reply -- a run that finishes at once is
    #  reported before its command is answered -- so both are collected in
    #  whichever order they arrive.
    $script:stop    = $null
    $script:replied = $false

    $null = Wait-For -Reader $reader -TimeoutSeconds $TimeoutSeconds -IsWanted {
        param ($r)

        if ($r.type -eq 'stopped') { $script:stop    = $r }
        if ($r.type -eq 'reply' -and $r.id -eq 3) { $script:replied = $true }

        return ($null -ne $script:stop) -and $script:replied
    }

    $stop = $script:stop

    if ($null -eq $stop) {
        Write-Error "No stop arrived within $TimeoutSeconds seconds."
        exit 1
    }

    Write-Host ('Stopped: {0} at ${1:X4}' -f $stop.reason, $stop.pc)
    exit 0
}
finally {
    $pipe.Dispose()
}
