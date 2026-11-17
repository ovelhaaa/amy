[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Port,
    [Parameter(Mandatory)][string]$Bundle,
    [ValidateSet('guition-editor-8','guition-editor-8-bench')][string]$Profile = 'guition-editor-8-bench',
    [string]$Pio = 'pio',
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$pioVersion = & $Pio --version
if ($LASTEXITCODE) { throw 'PlatformIO indisponível' }
if ($Port -notmatch '^COM[1-9][0-9]*$' -or $Port -notin [IO.Ports.SerialPort]::GetPortNames()) {
    throw "Porta $Port ausente. Identifique a Guition desconectando e reconectando somente o cabo de console."
}
$environment = Select-String -LiteralPath "$PSScriptRoot/../../platformio.ini" -Pattern "^\[env:$Profile\]$" -Quiet
if (!$environment) { throw "Ambiente $Profile ausente" }
$json = & $Python "$PSScriptRoot/firmware.py" check --profile $Profile --bundle $Bundle
if ($LASTEXITCODE) { throw 'Manifesto ou firmware inválido' }
$manifest = $json | ConvertFrom-Json -AsHashtable
$manifest.platformio = $pioVersion
$manifest.port = $Port
$manifest.operations = @('verify_local_bundle', 'enumerate_serial_ports')
$manifest
