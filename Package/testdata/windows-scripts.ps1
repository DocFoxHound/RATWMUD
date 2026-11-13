# restore.bat and reset-editor-password.bat in cmd.exe on Windows (Docs/Design/78, 3.6 and 12.2; Phase M5), against a
# stand-in docker.exe (FakeDocker.cs, compiled here with the .NET Framework's csc): the same cases as
# Package/test_package.py's Scripts tests run on Linux with restore.sh and reset-editor-password.sh, plus typed input
# that cmd could read as a command. Run from the repository's root: pwsh Package/testdata/windows-scripts.ps1
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$work = Join-Path ([IO.Path]::GetTempPath()) "s2stage-m5-scripts"
Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
$bin = New-Item -ItemType Directory -Force (Join-Path $work 'bin')
$world = New-Item -ItemType Directory -Force (Join-Path $work 'my world (test)')
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $csc /nologo /out:"$bin\docker.exe" "$root\Package\testdata\FakeDocker.cs" | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'csc failed' }
Copy-Item "$root\Package\restore.bat", "$root\Package\reset-editor-password.bat" $world
Set-Content -Path (Join-Path $world 'compose.yaml') -Value 'name: ratw-world'
$env:PATH = "$bin;$env:PATH"
$failures = 0

function Run-Script($script, $answers, $arguments = '', [switch]$refuse) {
    $log = Join-Path $work 'calls.txt'
    Remove-Item $log -ErrorAction SilentlyContinue
    $env:FAKE_LOG = $log
    $env:FAKE_REFUSE = if ($refuse) { '1' } else { '' }
    $input_ = Join-Path $work 'answers.txt'
    [IO.File]::WriteAllText($input_, ($answers -join "`r`n") + "`r`n")
    # (Through a wrapper, so no shell's quoting rules stand between the test and cmd's: the world folder's path has a
    # space and brackets in it on purpose, as a host's may.)
    $wrapper = Join-Path $work 'run.cmd'
    [IO.File]::WriteAllText($wrapper, "@call `"$world\$script`" $arguments < `"$input_`"`r`n")
    Push-Location $work
    $out = cmd /d /c $wrapper 2>&1 | Out-String
    Pop-Location
    $calls = if (Test-Path $log) { @(Get-Content $log) } else { @() }
    return @{out = $out; calls = $calls}
}

function Check($what, $ok, $detail) {
    if ($ok) { Write-Host "ok   $what" } else { Write-Host "FAIL $what`n$detail"; $script:failures++ }
}

$r = Run-Script 'restore.bat' @('1', 'RESTORE', '')
$want = @('compose version', 'compose stop backup', 'compose run --rm -T backup list', 'compose run --rm -T backup name 1',
          'compose run --rm -T backup show ratw-20261112T140300Z-by-hand', 'compose stop world',
          'compose run --rm -T backup restore ratw-20261112T140300Z-by-hand --keep-accounts', 'compose up -d')
Check 'restore: lists, asks, says what is lost, stops the world, keeps accounts' (($r.calls -join '|') -eq ($want -join '|')) ($r.calls -join "`n") 
Check 'restore: what is lost is said' ($r.out -match 'Everything players did since then is lost') $r.out
Check 'restore: done' ($r.out -match 'Your world is back') $r.out

$r = Run-Script 'restore.bat' @('RESTORE', 'b') 'ratw-20261112T140300Z-by-hand'
Check 'restore NAME: the backup''s accounts taken' ($r.calls -contains 'compose run --rm -T backup restore ratw-20261112T140300Z-by-hand --take-accounts') ($r.calls -join "`n")

foreach ($answers in @(@(''), @('1', 'yes'), @('9'), @('1&echo HACKED>hacked.txt'), @('1', 'RESTORE&echo HACKED>hacked.txt'))) {
    $r = Run-Script 'restore.bat' $answers
    Check "restore [$($answers -join ' / ')]: nothing changed" ($r.out -match 'Nothing was changed') $r.out
    Check "restore [$($answers -join ' / ')]: the world isn't stopped" (-not ($r.calls -match 'stop world|backup restore')) ($r.calls -join "`n")
    Check "restore [$($answers -join ' / ')]: started again" ($r.calls[-1] -eq 'compose up -d') ($r.calls -join "`n")
    Check "restore [$($answers -join ' / ')]: nothing typed ran" (-not ((Test-Path (Join-Path $work 'hacked.txt')) -or (Test-Path (Join-Path $world 'hacked.txt')))) ''
}

$r = Run-Script 'restore.bat' @('1', 'RESTORE', '') -refuse
Check 'restore refused: said, and the world started again' (($r.out -match "The restore didn't finish") -and ($r.calls[-1] -eq 'compose up -d')) $r.out

$r = Run-Script 'reset-editor-password.bat' @('host-k7q2')
$want = @('compose version', 'compose exec -T world true', 'compose exec -T world ratw-editor list',
          'compose exec -T world ratw-editor reset host-k7q2 --write-file')
Check 'reset: lists, asks, resets, writes the file' (($r.calls -join '|') -eq ($want -join '|')) ($r.calls -join "`n")
Check 'reset: the code is shown' (($r.out -match 'ratw-editor:abc') -and ($r.out -match 'first-editor.txt')) $r.out
foreach ($answers in @(@('Host K7Q2'), @('pell&echo HACKED>hacked.txt'), @(''))) {
    $r = Run-Script 'reset-editor-password.bat' $answers
    Check "reset [$($answers -join ' / ')]: nothing reset" (-not ($r.calls -match 'reset')) ($r.calls -join "`n")
    Check "reset [$($answers -join ' / ')]: nothing typed ran" (-not ((Test-Path (Join-Path $work 'hacked.txt')) -or (Test-Path (Join-Path $world 'hacked.txt')))) ''
}

Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
if ($failures) { throw "$failures check(s) failed" }
Write-Host 'all the scripts'' checks passed'
