# Doc 78, M0a, on Windows: the reserved port ranges against the Editor's 17789 and the game's 7788, what Hyper-V, WSL
# and Docker are on this machine, and (only if Docker runs Linux containers here) the peer address and the
# Windows-folder bind mount checks. Writes everything to stdout; the workflow keeps it as windows-facts.txt.
# On the user's PC (Docker Desktop running): pwsh Desktop/spike-editor/windows_facts.ps1 -Keep, then open
# http://<this PC's LAN address>:47789/ from a phone or a second computer (the peer it shows), then
# docker rm -f m0a-a m0a-b.
param([string]$Here = $PSScriptRoot, [switch]$Keep)
$ErrorActionPreference = 'Continue'
function Section($name) { Write-Output ""; Write-Output "== $name" }

Section 'Windows'
(Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber | Format-List | Out-String).Trim()

Section 'Reserved TCP port ranges (netsh int ipv4 show excludedportrange protocol=tcp)'
$v4 = netsh int ipv4 show excludedportrange protocol=tcp
$v4
Section 'IPv6 (netsh int ipv6 show excludedportrange protocol=tcp)'
$v6 = netsh int ipv6 show excludedportrange protocol=tcp
$v6
Section 'Dynamic port range (netsh int ipv4 show dynamicport tcp)'
netsh int ipv4 show dynamicport tcp
Section 'Do 17789 and 7788 fall in a reserved range?'
foreach ($port in 17789, 7788, 8765) {
  $hits = @()
  foreach ($line in @($v4) + @($v6)) {
    if ($line -match '^\s*(\d+)\s+(\d+)') {
      if ($port -ge [int]$Matches[1] -and $port -le [int]$Matches[2]) { $hits += $line.Trim() }
    }
  }
  if ($hits) { Write-Output "$port RESERVED by: $($hits -join '; ')" } else { Write-Output "$port free of every reserved range" }
  $listener = $null
  try { $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Any, $port); $listener.Start(); Write-Output "$port bind on 0.0.0.0: ok" }
  catch { Write-Output "$port bind on 0.0.0.0: FAILED $($_.Exception.Message)" }
  finally { if ($listener) { $listener.Stop() } }
}

Section 'Hyper-V, WSL, containers (optional features)'
foreach ($f in 'Microsoft-Hyper-V', 'Microsoft-Hyper-V-All', 'VirtualMachinePlatform', 'Microsoft-Windows-Subsystem-Linux', 'Containers') {
  try { $s = (Get-WindowsOptionalFeature -Online -FeatureName $f -ErrorAction Stop).State; Write-Output "$f : $s" }
  catch { Write-Output "$f : (not known here)" }
}
Section 'wsl --status'
try { wsl.exe --status 2>&1 | ForEach-Object { $_ -replace "`0", '' } } catch { Write-Output "wsl.exe: not present" }

Section 'Docker'
$docker = Get-Command docker -ErrorAction SilentlyContinue
if (-not $docker) { Write-Output 'docker: not installed'; exit 0 }
docker version 2>&1
$os = docker info --format '{{.OSType}}' 2>$null
Write-Output "Docker's container OS: $os"
if (Test-Path "$env:ProgramFiles\Docker\Docker\Docker Desktop.exe") { Write-Output 'Docker Desktop: installed' } else { Write-Output 'Docker Desktop: not installed' }
if ($os -ne 'linux') { Write-Output 'Docker here runs Windows containers only: the peer-address and bind-mount checks need Docker Desktop (the user''s PC).'; exit 0 }

Section 'Peer address through Docker Desktop (Linux containers)'
docker run -d --name m0a-a -p 0.0.0.0:47789:17789 -v "${Here}:/p:ro" --entrypoint python3 python:3.12-slim /p/peer_server.py | Out-Null
docker run -d --name m0a-b -p 47790:17789 -v "${Here}:/p:ro" --entrypoint python3 python:3.12-slim /p/peer_server.py | Out-Null
Start-Sleep 5
$ip = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' } | Select-Object -First 1).IPAddress
foreach ($p in 47789, 47790) {
  foreach ($a in '127.0.0.1', $ip, '[::1]') {
    try { $r = Invoke-WebRequest -UseBasicParsing -TimeoutSec 3 "http://${a}:$p/"; Write-Output "$p from $a : $($r.Content.Trim())" }
    catch { Write-Output "$p from $a : no answer" }
  }
}
if ($Keep) { Write-Output "Left running: open http://${ip}:47789/ and http://${ip}:47790/ from another device, then docker rm -f m0a-a m0a-b" }
else { docker rm -f m0a-a m0a-b | Out-Null }

Section 'chown and chmod into a Windows-folder bind mount'
$base = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $HOME }   # Docker Desktop shares C:\Users by default
$dir = Join-Path $base 'ratw-m0a-manage'
New-Item -ItemType Directory -Force $dir | Out-Null
docker run --rm -v "${dir}:/var/ratw/manage" python:3.12-slim sh -c 'echo secret > /var/ratw/manage/first-editor.txt; chown 10003:10003 /var/ratw/manage/first-editor.txt; echo "chown exit $?"; chmod 600 /var/ratw/manage/first-editor.txt; echo "chmod exit $?"; ls -ln /var/ratw/manage; su -s /bin/sh nobody -c "cat /var/ratw/manage/first-editor.txt" && echo "nobody CAN read it" || echo "nobody cannot read it"'
Get-Acl (Join-Path $dir 'first-editor.txt') | Format-List Owner, AccessToString
