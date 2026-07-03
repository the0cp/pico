Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$timeoutSec = 5

if (-not [string]::IsNullOrWhiteSpace($env:TIMEOUT_SEC)) {
    $timeoutSec = [int]$env:TIMEOUT_SEC
}

$picoExec = $env:PICO_EXEC

if ([string]::IsNullOrWhiteSpace($picoExec)) {
    $candidates = @(
        (Join-Path $PSScriptRoot "..\build\debug\pico.exe"),
        (Join-Path $PSScriptRoot "..\build\release\pico.exe")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            $picoExec = $candidate
            break
        }
    }
}

if ([string]::IsNullOrWhiteSpace($picoExec) -or
    -not (Test-Path -LiteralPath $picoExec)) {
    Write-Error @"
PiCo executable was not found.

Set PICO_EXEC explicitly, for example:

    `$env:PICO_EXEC = "..\build\debug\pico.exe"
    .\run_tests.ps1
"@
    exit 1
}

$picoExec = (Resolve-Path -LiteralPath $picoExec).Path

function Show-CapturedOutput {
    param(
        [string]$StdoutPath,
        [string]$StderrPath
    )

    $lines = @()

    if (Test-Path -LiteralPath $StdoutPath) {
        $lines += @(Get-Content -LiteralPath $StdoutPath)
    }

    if (Test-Path -LiteralPath $StderrPath) {
        $lines += @(Get-Content -LiteralPath $StderrPath)
    }

    if ($lines.Count -eq 0) {
        Write-Host "No output captured."
        return
    }

    $lines |
        Select-Object -First 120 |
        ForEach-Object {
            Write-Host $_
        }
}

$testFiles = Get-ChildItem `
    -LiteralPath $PSScriptRoot `
    -Filter "test_*.pcs" |
    Where-Object {
        -not $_.PSIsContainer
    } |
    Sort-Object Name

$passed = 0
$failed = 0
$timeouts = 0

Write-Host "Starting tests..."
Write-Host "PICO_EXEC: $picoExec"
Write-Host "TIMEOUT_SEC: $timeoutSec"
Write-Host ""

foreach ($test in $testFiles) {
    Write-Host ("Running {0,-35} " -f $test.Name) -NoNewline

    $temporaryBase = Join-Path `
        ([System.IO.Path]::GetTempPath()) `
        ("pico-test-{0}-{1}" -f $PID, [Guid]::NewGuid().ToString("N"))

    $stdoutFile = "$temporaryBase.stdout.log"
    $stderrFile = "$temporaryBase.stderr.log"

    try {
        $process = Start-Process `
            -FilePath $picoExec `
            -ArgumentList @($test.Name) `
            -WorkingDirectory $PSScriptRoot `
            -RedirectStandardOutput $stdoutFile `
            -RedirectStandardError $stderrFile `
            -NoNewWindow `
            -PassThru

        $finished = $process.WaitForExit($timeoutSec * 1000)

        if (-not $finished) {
            try {
                $process.Kill()
            }
            catch {
                # The process may already have exited.
            }

            $process.WaitForExit()

            Write-Host "[TIMEOUT]"
            Write-Host "Timed out after $timeoutSec seconds."

            Show-CapturedOutput `
                -StdoutPath $stdoutFile `
                -StderrPath $stderrFile

            $timeouts++
            $failed++
            continue
        }

        # Wait once more so redirected output is completely flushed.
        $process.WaitForExit()

        $exitCode = $process.ExitCode

        if ($exitCode -eq 0) {
            Write-Host "[PASS]"
            $passed++
        }
        elseif ($exitCode -lt 0) {
            Write-Host "[CRASH: exit $exitCode]"

            Show-CapturedOutput `
                -StdoutPath $stdoutFile `
                -StderrPath $stderrFile

            $failed++
        }
        else {
            Write-Host "[FAIL: exit $exitCode]"

            Show-CapturedOutput `
                -StdoutPath $stdoutFile `
                -StderrPath $stderrFile

            $failed++
        }
    }
    catch {
        Write-Host "[ERROR]"
        Write-Host $_.Exception.Message
        $failed++
    }
    finally {
        Remove-Item `
            -LiteralPath $stdoutFile, $stderrFile `
            -Force `
            -ErrorAction SilentlyContinue
    }
}

Write-Host ""
Write-Host "========================================"
Write-Host "Summary: $passed Passed, $failed Failed ($timeouts Timeouts)"
Write-Host "========================================"

if ($failed -eq 0) {
    exit 0
}

exit 1
