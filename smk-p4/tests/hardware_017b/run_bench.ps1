[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Port,
    [Parameter(Mandatory)][string]$BoardId,
    [Parameter(Mandatory)][string]$Bundle,
    [Parameter(Mandatory)][ValidatePattern('^[a-zA-Z0-9][a-zA-Z0-9._-]*$')][string]$SessionId,
    [ValidateRange(1,3600)][int]$Seconds = 900,
    [ValidateRange(1,4000000)][int]$Baud = 115200,
    [string]$OutputRoot = 'build/hardware-017b',
    [string]$Pio = 'pio',
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$preflight = & "$PSScriptRoot/preflight.ps1" -Port $Port -Bundle $Bundle -Pio $Pio -Python $Python
$null = New-Item -ItemType Directory -Path $OutputRoot -Force
$session = Join-Path $OutputRoot $SessionId
$null = New-Item -ItemType Directory -Path $session
$preflight | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $session 'preflight.json') -Encoding utf8
$raw = Join-Path $session 'serial.log'
Write-Host 'Inicie a coleta antes de energizar, se a porta permanecer acessível. Execute A e C–H no boot principal.'
Write-Host 'Mantenha a captura por dois relatórios no início e no fim. Faça B em captura complementar.'
& "$PSScriptRoot/capture.ps1" -Port $Port -BoardId $BoardId -Bundle $Bundle -Output $raw -SessionId $SessionId -Seconds $Seconds -Baud $Baud -Python $Python
& $Python "$PSScriptRoot/analyze.py" $raw --metadata "$raw.metadata.json" --out $session
if ($LASTEXITCODE) { throw 'Falha de análise' }
& $Python "$PSScriptRoot/evaluate.py" (Join-Path $session 'metrics.json') --out $session
if ($LASTEXITCODE) { throw 'Falha do avaliador' }
Write-Host "Relatório em $session. Depois da revisão física, execute evaluate.py com --evidence."
