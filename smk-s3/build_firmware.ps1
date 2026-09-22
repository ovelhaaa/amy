$tools = @(
    'C:\Users\devx\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin',
    'C:\Users\devx\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin',
    'C:\Users\devx\.espressif\tools\esp32ulp-elf\2.38_20240113\esp32ulp-elf\bin',
    'C:\Users\devx\.espressif\tools\cmake\3.30.2\bin',
    'C:\Users\devx\.espressif\tools\ninja\1.12.1',
    'C:\Users\devx\.espressif\tools\idf-exe\1.0.3',
    'C:\Users\devx\.espressif\python_env\idf6.0_py3.11_env\Scripts'
)

$env:PATH = ($tools -join ';') + ';' + $env:PATH
$env:IDF_PATH = 'X:/'
$env:IDF_TOOLS_PATH = 'C:\Users\devx\.espressif'
$env:IDF_PYTHON_ENV_PATH = 'C:\Users\devx\.espressif\python_env\idf6.0_py3.11_env'
$env:IDF_MAINTAINER = '1'
$python = 'C:\Users\devx\.espressif\python_env\idf6.0_py3.11_env\Scripts\python.exe'
$idf_py = 'X:\tools\idf.py'

& $python $idf_py build
exit $LASTEXITCODE
