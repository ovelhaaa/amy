[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^COM[1-9][0-9]*$')][string]$Port,
    [Parameter(Mandatory)][string]$BoardId,
    [Parameter(Mandatory)][string]$Bundle,
    [Parameter(Mandatory)][string]$Output,
    [Parameter(Mandatory)][ValidatePattern('^[a-zA-Z0-9][a-zA-Z0-9._-]*$')][string]$SessionId,
    [ValidateRange(1,3600)][int]$Seconds = 180,
    [ValidateRange(1,4000000)][int]$Baud = 115200,
    [switch]$Persistence,
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$json = & $Python "$PSScriptRoot/firmware.py" check --profile guition-editor-8-bench --bundle $Bundle
if ($LASTEXITCODE) { throw 'Manifesto ou firmware inválido' }
$manifest = $json | ConvertFrom-Json -AsHashtable
$manifestSha = (Get-FileHash -LiteralPath (Join-Path $Bundle 'manifest.json')).Hash.ToLowerInvariant()
$metadataPath = "$Output.metadata.json"
if ((Test-Path -LiteralPath $Output) -or (Test-Path -LiteralPath $metadataPath)) {
    throw 'Escolha um caminho novo para a coleta.'
}
$started = [DateTimeOffset]::UtcNow
& $Python "$PSScriptRoot/../capture_serial.py" --port $Port --baud $Baud --seconds $Seconds --output $Output --exclusive
$exitCode = $LASTEXITCODE
if (!(Test-Path -LiteralPath $Output)) { throw "Falha ao abrir $Port (saída $exitCode)" }
$metadata = @{
    kind = 'REAL HARDWARE SESSION'
    role = if ($Persistence) { 'persistence' } else { 'primary' }
    session_id = $SessionId
    board_id = $BoardId
    port = $Port
    baud = $Baud
    requested_seconds = $Seconds
    started_utc = $started.ToString('o')
    ended_utc = [DateTimeOffset]::UtcNow.ToString('o')
    reset_requested = $false
    capture_exit = $exitCode
    bytes = (Get-Item -LiteralPath $Output).Length
    raw_sha256 = (Get-FileHash -LiteralPath $Output).Hash.ToLowerInvariant()
    profile = $manifest.profile
    firmware_sha256 = $manifest.firmware_sha256
    elf_sha256 = $manifest.elf_sha256
    commit = $manifest.commit
    manifest_sha256 = $manifestSha
}
$metadata | ConvertTo-Json | Set-Content -LiteralPath $metadataPath -Encoding utf8
if ($exitCode) { Write-Warning "Coleta retornou $exitCode. O avaliador manterá a evidência incompleta." }
