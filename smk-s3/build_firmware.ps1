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

# 1. Resolve Tools Path
if (-not $ToolsPath) {
    if ($env:IDF_TOOLS_PATH -and (Test-Path $env:IDF_TOOLS_PATH)) {
        $ToolsPath = $env:IDF_TOOLS_PATH
    } elseif ($env:USERPROFILE -and (Test-Path "$env:USERPROFILE\.espressif")) {
        $ToolsPath = "$env:USERPROFILE\.espressif"
    } elseif (Test-Path 'C:\Espressif\tools') {
        $ToolsPath = 'C:\Espressif\tools'
    }
}

# 2. Resolve IDF Path
if (-not $IdfPath) {
    if ($env:IDF_PATH -and (Test-Path $env:IDF_PATH)) {
        $IdfPath = $env:IDF_PATH
    } elseif ($env:USERPROFILE -and (Test-Path "$env:USERPROFILE\esp\esp-idf")) {
        $IdfPath = "$env:USERPROFILE\esp\esp-idf"
    } elseif (Test-Path 'C:\Espressif\frameworks\esp-idf') {
        $IdfPath = 'C:\Espressif\frameworks\esp-idf'
    } else {
        $fwCandidate = Get-ChildItem -Path 'C:\Espressif\frameworks' -Directory -Filter 'esp-idf*' -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($fwCandidate) {
            $IdfPath = $fwCandidate.FullName
        }
    }
}

if (-not $IdfPath -or -not (Test-Path (Join-Path $IdfPath "tools\idf.py"))) {
    Write-Error "ESP-IDF not found. Please provide -IdfPath <path_to_esp_idf> or set the IDF_PATH environment variable."
    exit 1
}

$hasExport = Test-Path (Join-Path $IdfPath "export.ps1")

if ($hasExport -and -not $PSBoundParameters.ContainsKey('PythonEnvPath')) {
    Remove-Item Env:\IDF_PYTHON_ENV_PATH -ErrorAction SilentlyContinue
    $env:IDF_PATH = $IdfPath
    if ($ToolsPath) { $env:IDF_TOOLS_PATH = $ToolsPath }
    Write-Host "Activating ESP-IDF environment via export.ps1..."
    . (Join-Path $IdfPath "export.ps1")
} else {
    # Manual environment configuration fallback
    # 3. Resolve Python Environment Path
    if (-not $PythonEnvPath) {
        if ($env:IDF_PYTHON_ENV_PATH -and (Test-Path $env:IDF_PYTHON_ENV_PATH)) {
            $PythonEnvPath = $env:IDF_PYTHON_ENV_PATH
        } elseif ($ToolsPath -and (Test-Path "$ToolsPath\python_env")) {
            $pyEnvs = Get-ChildItem -Path "$ToolsPath\python_env" -ErrorAction SilentlyContinue | Sort-Object Name -Descending
            foreach ($envDir in $pyEnvs) {
                if (Test-Path (Join-Path $envDir.FullName "Scripts\python.exe")) {
                    $PythonEnvPath = $envDir.FullName
                    break
                }
            }
        }
    }

    # 4. Resolve Python executable
    $python = 'python'
    if ($PythonEnvPath -and (Test-Path (Join-Path $PythonEnvPath "Scripts\python.exe"))) {
        $python = Join-Path $PythonEnvPath "Scripts\python.exe"
    }

    # 5. Configure environment variables
    $env:IDF_PATH = $IdfPath
    if ($ToolsPath) { $env:IDF_TOOLS_PATH = $ToolsPath }
    if ($PythonEnvPath) {
        $env:IDF_PYTHON_ENV_PATH = $PythonEnvPath
        $env:PATH = "$PythonEnvPath\Scripts;$env:PATH"
    }
}

# Add tool directories to PATH without recursive disk scans
if ($ToolsPath -and (Test-Path "$ToolsPath\tools")) {
    $toolDirs = Get-ChildItem -Path "$ToolsPath\tools" -Directory -ErrorAction SilentlyContinue
    $allPaths = @()
    foreach ($tool in $toolDirs) {
        $versions = Get-ChildItem -Path $tool.FullName -Directory -ErrorAction SilentlyContinue
        foreach ($ver in $versions) {
            $bin1 = Join-Path $ver.FullName "bin"
            $bin2 = Join-Path $ver.FullName (Join-Path $tool.Name "bin")
            if (Test-Path $bin1) {
                $allPaths += $bin1
            } elseif (Test-Path $bin2) {
                $allPaths += $bin2
            } elseif (Test-Path $ver.FullName) {
                $allPaths += $ver.FullName
            }
        }
    }
    if ($allPaths.Count -gt 0) {
        $env:PATH = ($allPaths -join ';') + ';' + $env:PATH
    }
}

$env:IDF_MAINTAINER = '1'

$python = 'python'
if ($env:IDF_PYTHON_ENV_PATH -and (Test-Path "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe")) {
    $python = "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe"
}
$idf_py = Join-Path $env:IDF_PATH "tools\idf.py"

Write-Host "Using IDF_PATH: $env:IDF_PATH"
Write-Host "Using Python: $python"
Write-Host "Running idf.py with args: $IdfArgs"

Push-Location $PSScriptRoot
try {
    & $python $idf_py @IdfArgs
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
