Set-Location $PSScriptRoot
cls

# & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -p COM12 -b 115200
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -p COM13 -b 115200
