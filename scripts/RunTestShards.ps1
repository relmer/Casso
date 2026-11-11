<#
.SYNOPSIS
    Runs a test DLL as several vstest.console processes at once, one per
    shard of its test classes, and fails if any shard fails.

.DESCRIPTION
    vstest.console runs the tests in one DLL on one thread; /Parallel only
    runs separate DLLs side by side. CI's 4-core runners spent most of the
    test step with three cores idle. This lists the tests, splits the test
    classes into -Shards groups, and runs one vstest.console per group with
    a /TestCaseFilter that selects those classes.

    Classes are dealt out largest first to the shard with the fewest tests,
    so the shards have about the same number of tests. A class always runs
    whole in one process, so its CLASS_INITIALIZE and any state its methods
    share are not split.

    The total of the shards' test counts must equal the count from the
    listing. A shard whose filter matched nothing, or a class left out of
    every filter, fails the run.

.PARAMETER TestDll
    The test DLL to run.

.PARAMETER Shards
    How many vstest.console processes. Default: the logical core count.

.PARAMETER VSTestPath
    vstest.console.exe. Default: vstest.console.exe on PATH, as CI sets it up.

.EXAMPLE
    .\RunTestShards.ps1 -TestDll x64\Debug\UnitTest.dll
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$TestDll,

    [int]$Shards = [Environment]::ProcessorCount,

    [string]$VSTestPath = 'vstest.console.exe'
)

$ErrorActionPreference = 'Stop'

$TestDll  = (Resolve-Path $TestDll).Path
$workDir  = Join-Path ([System.IO.Path]::GetTempPath()) "CassoTestShards-$PID"
$listPath = Join-Path $workDir 'tests.txt'

New-Item -ItemType Directory -Force $workDir | Out-Null

& $VSTestPath $TestDll /ListFullyQualifiedTests "/ListTestsTargetPath:$listPath" | Out-Null

if ($LASTEXITCODE -ne 0 -or -not (Test-Path $listPath)) {
    throw "Listing the tests in $TestDll failed."
}

$allTests = @(Get-Content $listPath | Where-Object { $_ -match '::' })

if ($allTests.Count -eq 0) {
    throw "No tests found in $TestDll."
}

#
#  Deal the classes out largest first, each to the shard with the fewest
#  tests so far.
#
#  The filter matches by substring, and the C++ adapter does not support
#  ClassName, so "BootDiskTests::" also matches CassoLinkBootDiskTests, and a
#  namespace or outer class matches the classes inside it. The match also
#  ignores case. Classes whose filters overlap go in one shard, so no test
#  runs in two shards.
#
$byClass = $allTests | Group-Object { $_.Substring(0, $_.LastIndexOf('::')) }
$groupOf = @{}

foreach ($c in $byClass) {
    $groupOf[$c.Name] = $c.Name
}

foreach ($a in $byClass) {
    foreach ($b in $byClass) {
        if ($a.Name -ne $b.Name -and "$($b.Name)::".ToLowerInvariant().Contains("$($a.Name)::".ToLowerInvariant())) {
            $from = $groupOf[$a.Name]
            $to   = $groupOf[$b.Name]

            foreach ($k in @($groupOf.Keys)) {
                if ($groupOf[$k] -eq $from) {
                    $groupOf[$k] = $to
                }
            }
        }
    }
}

$classes = $byClass |
    Group-Object { $groupOf[$_.Name] } |
    ForEach-Object {
        [pscustomobject]@{
            Names = @($_.Group | ForEach-Object Name)
            Count = ($_.Group | Measure-Object Count -Sum).Sum
        }
    } |
    Sort-Object Count -Descending

$shardCount = [Math]::Max(1, [Math]::Min($Shards, $classes.Count))
$shardTests = New-Object int[] $shardCount
$shardNames = [System.Collections.Generic.List[object]]::new()

for ($i = 0; $i -lt $shardCount; $i++) {
    $shardNames.Add([System.Collections.Generic.List[string]]::new())
}

foreach ($class in $classes) {
    $target = [Array]::IndexOf($shardTests, [int] ($shardTests | Measure-Object -Minimum).Minimum)
    $shardNames[$target].AddRange([string[]] $class.Names)
    $shardTests[$target] += $class.Count
}

Write-Host ("{0} tests in {1} classes, {2} shards" -f $allTests.Count, $byClass.Count, $shardCount)

#
#  One vstest.console per shard. The filter selects each class by the
#  "Class::" prefix of its tests' fully qualified names.
#
$processes = @()

for ($i = 0; $i -lt $shardCount; $i++) {
    $filter  = ($shardNames[$i] | ForEach-Object { "FullyQualifiedName~$($_)::" }) -join '|'
    $logPath = Join-Path $workDir "shard$i.log"
    $vstestArgs = @("`"$TestDll`"", "`"/TestCaseFilter:$filter`"")

    $processes += [pscustomobject]@{
        Index    = $i
        Expected = $shardTests[$i]
        Log      = $logPath
        Process  = Start-Process -FilePath $VSTestPath -ArgumentList $vstestArgs -NoNewWindow -PassThru `
                                 -RedirectStandardOutput $logPath -RedirectStandardError "$logPath.err"
    }
}

$failed     = $false
$totalRun   = 0
$totalPass  = 0
$totalSkip  = 0

foreach ($p in $processes) {
    $p.Process.WaitForExit()

    $log    = @(Get-Content $p.Log) + @(Get-Content "$($p.Log).err" -ErrorAction SilentlyContinue)
    $total  = [int](($log | Select-String '^\s*Total tests:\s*(\d+)' | Select-Object -Last 1).Matches.Groups[1].Value)
    $passed = [int](($log | Select-String '^\s*Passed:\s*(\d+)' | Select-Object -Last 1).Matches.Groups[1].Value)

    #  A test marked TEST_IGNORE is reported as skipped, not failed, as the
    #  serial run treats it.
    $skipLine = $log | Select-String '^\s*Skipped:\s*(\d+)' | Select-Object -Last 1
    $skipped  = ($null -ne $skipLine) ? [int]$skipLine.Matches.Groups[1].Value : 0

    $totalRun  += $total
    $totalPass += $passed
    $totalSkip += $skipped

    $status  = 'OK'
    $asanHit = $log | Select-String -SimpleMatch 'ERROR: AddressSanitizer' -Quiet

    if ($asanHit) {
        #  An AddressSanitizer report fails the shard even when vstest exits 0.
        #  The whole log is printed so the report is in the caller's output.
        $log | ForEach-Object { Write-Host $_ }
    }

    if ($p.Process.ExitCode -ne 0 -or $total -ne $p.Expected -or $passed + $skipped -ne $total -or $asanHit) {
        $status = 'FAILED'
        $failed = $true
    }

    Write-Host ("Shard {0}: {1} of {2} listed tests run, {3} passed, {4} skipped, exit {5}  {6}" -f $p.Index, $total, $p.Expected, $passed, $skipped, $p.Process.ExitCode, $status)

    if ($status -ne 'OK' -and -not $asanHit) {
        $log | Select-String -Pattern '^\s*Failed |Error Message' -Context 0, 2 | ForEach-Object { Write-Host $_ }
    }
}

Write-Host ("Total: {0} of {1} tests run, {2} passed, {3} skipped" -f $totalRun, $allTests.Count, $totalPass, $totalSkip)

if ($totalRun -ne $allTests.Count) {
    Write-Host 'The shards did not run every listed test.' -ForegroundColor Red
    $failed = $true
}

Remove-Item -Recurse -Force $workDir -ErrorAction SilentlyContinue

if ($failed) {
    exit 1
}

exit 0
