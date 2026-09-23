[CmdletBinding()]
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$IdfArgs,
    [string]$IdfPath,
    [string]$ToolsPath,
    [string]$PythonEnvPath
)

if (-not $IdfArgs -or $IdfArgs.Count -eq 0) {
    $IdfArgs = @('build')
}

# Fallbacks for local environment if not already defined
if (-not $ToolsPath) {
    if (Test-Path "$env:USERPROFILE\.espressif") {
        $ToolsPath = "$env:USERPROFILE\.espressif"
    } elseif (Test-Path 'C:\Users\devx\.espressif') {
        $ToolsPath = 'C:\Users\devx\.espressif'
    }
}

if (-not $IdfPath) {
    if (Test-Path 'X:\tools\idf.py') {
        $IdfPath = 'X:\'
    } elseif ($env:IDF_PATH) {
        $IdfPath = $env:IDF_PATH
    }
}

if (-not $PythonEnvPath -and $ToolsPath) {
    if (Test-Path "$ToolsPath\python_env\idf6.0_py3.11_env") {
        $PythonEnvPath = "$ToolsPath\python_env\idf6.0_py3.11_env"
    } else {
        $pyEnvs = Get-ChildItem -Path "$ToolsPath\python_env" -Directory -ErrorAction SilentlyContinue
        if ($pyEnvs) {
            $PythonEnvPath = $pyEnvs[0].FullName
        }
    }
}

# Resolve Python executable
$python = 'python'
if ($PythonEnvPath -and (Test-Path "$PythonEnvPath\Scripts\python.exe")) {
    $python = "$PythonEnvPath\Scripts\python.exe"
}

# Resolve idf.py
$idf_py = 'idf.py'
if ($IdfPath -and (Test-Path "$IdfPath\tools\idf.py")) {
    $idf_py = "$IdfPath\tools\idf.py"
}

# Setup environment variables
if ($IdfPath) { $env:IDF_PATH = $IdfPath }
if ($ToolsPath) { $env:IDF_TOOLS_PATH = $ToolsPath }
if ($PythonEnvPath) {
    $env:IDF_PYTHON_ENV_PATH = $PythonEnvPath
    $env:PATH = "$PythonEnvPath\Scripts;$env:PATH"
}

# Add tool directories to PATH if found in ToolsPath
if ($ToolsPath -and (Test-Path "$ToolsPath\tools")) {
    $binPaths = Get-ChildItem -Path "$ToolsPath\tools" -Recurse -Filter "bin" -Directory -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
    $ninjaPath = Get-ChildItem -Path "$ToolsPath\tools\ninja" -Recurse -Directory -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
    $allPaths = @()
    if ($binPaths) { $allPaths += $binPaths }
    if ($ninjaPath) { $allPaths += $ninjaPath }
    if ($allPaths.Count -gt 0) {
        $env:PATH = ($allPaths -join ';') + ';' + $env:PATH
    }
}

$env:IDF_MAINTAINER = '1'

Write-Host "Using IDF_PATH: $env:IDF_PATH"
Write-Host "Using Python: $python"
Write-Host "Running idf.py with args: $IdfArgs"

& $python $idf_py @IdfArgs
exit $LASTEXITCODE
