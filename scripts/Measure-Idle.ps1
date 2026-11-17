param([int]$Seconds = 20)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$data = Join-Path $root 'build\perf-test'
New-Item -ItemType Directory -Path $data -Force | Out-Null
$fixture = Join-Path $root 'build\Release\Elka.MultiLaunch.Tests.exe'
$trigger = Join-Path $data 'Elka.PerfTrigger.exe'
$target = Join-Path $data 'Elka.PerfTarget.exe'
Copy-Item -LiteralPath $fixture -Destination $trigger -Force
Copy-Item -LiteralPath $fixture -Destination $target -Force
$lines = [Collections.Generic.List[string]]::new()
$lines.Add("ELKA-MULTILAUNCH`t2")
$lines.Add("mode`tpolling")
$lines.Add("admin`t0")
foreach ($index in 1..10) {
    $lines.Add("profile`tperf-$index`tPerformance profile $index`t$($trigger.Replace('\','\\'))`t1")
    foreach ($number in 1..10) { $lines.Add("target`t$($target.Replace('\','\\'))`t--helper`t`t1") }
}
[IO.File]::WriteAllLines((Join-Path $data 'settings.elka'), $lines, [Text.UTF8Encoding]::new($false))
$exe = Join-Path $root 'build\Release\Elka.MultiLaunch.exe'
$process = Start-Process -FilePath $exe -ArgumentList ('--tray --data-dir "' + $data + '"') -WindowStyle Hidden -PassThru
try {
    Start-Sleep -Seconds 3
    $log = Get-Content -LiteralPath (Join-Path $data 'history.log') -Tail 1
    if ($log -notmatch 'Watching.*1 second process checks') { throw 'The performance instance did not confirm active monitoring.' }
    $process.Refresh()
    $initialCpu = $process.TotalProcessorTime.TotalMilliseconds
    $initialHandles = $process.HandleCount
    Start-Sleep -Seconds $Seconds
    $process.Refresh()
    [pscustomobject]@{
        Seconds = $Seconds; Profiles = 10; Targets = 100
        CPUms = [math]::Round($process.TotalProcessorTime.TotalMilliseconds - $initialCpu, 3)
        WorkingSetMiB = [math]::Round($process.WorkingSet64 / 1MB, 2)
        PrivateMiB = [math]::Round($process.PrivateMemorySize64 / 1MB, 2)
        HandlesBefore = $initialHandles; HandlesAfter = $process.HandleCount
    } | ConvertTo-Json | Tee-Object (Join-Path $root 'build\perf-result.json')
} finally {
    if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    $process.Dispose()
}
