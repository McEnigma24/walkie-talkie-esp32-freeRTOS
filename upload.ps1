Set-Location $PSScriptRoot
cls

& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload --upload-port COM13
