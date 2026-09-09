# Build and Deploy Script for Agy Universal Firmwares
# Targets: ESP8266 & ESP32
# Destination: D:\iot\iot-server\firmwares

param (
    [string]$OutputDir = "D:\iot\iot-server\firmwares"
)

$ErrorActionPreference = "Stop"
$env:PYTHONIOENCODING = "utf-8"
$pioExe = "C:\Users\simahfud\.platformio\penv\Scripts\pio.exe"

Write-Host "`n=======================================================" -ForegroundColor Cyan
Write-Host "  AGY UNIVERSAL FIRMWARE COMPILER & DEPLOYER" -ForegroundColor Cyan
Write-Host "=======================================================`n" -ForegroundColor Cyan

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
    Write-Host "[INIT] Membuat folder tujuan: $OutputDir" -ForegroundColor Yellow
}

# 1. Build ESP8266
Write-Host "[1/3] Mengompilasi Universal Firmware untuk ESP8266..." -ForegroundColor Green
& $pioExe run -e esp8266
if ($LASTEXITCODE -ne 0) { throw "Gagal mengompilasi firmware ESP8266!" }

$esp8266Bin = ".pio\build\esp8266\firmware.bin"
$destEsp8266 = Join-Path $OutputDir "agy_universal_esp8266.bin"
Copy-Item -Path $esp8266Bin -Destination $destEsp8266 -Force
Write-Host " -> Berhasil deploy: $destEsp8266 ($( [math]::Round((Get-Item $destEsp8266).Length / 1KB, 1) ) KB)" -ForegroundColor Cyan

# 2. Build ESP32
Write-Host "`n[2/3] Mengompilasi Universal Firmware untuk ESP32..." -ForegroundColor Green
& $pioExe run -e esp32
if ($LASTEXITCODE -ne 0) { throw "Gagal mengompilasi firmware ESP32!" }

$esp32Bin = ".pio\build\esp32\firmware.bin"
$destEsp32 = Join-Path $OutputDir "agy_universal_esp32.bin"
Copy-Item -Path $esp32Bin -Destination $destEsp32 -Force
Write-Host " -> Berhasil deploy (App/OTA): $destEsp32 ($( [math]::Round((Get-Item $destEsp32).Length / 1KB, 1) ) KB)" -ForegroundColor Cyan

# 3. Create ESP32 Factory Merged Binary (Single 0x0 Flash Image)
Write-Host "`n[3/3] Menghasilkan ESP32 Factory Merged Binary (0x0 Flash Image)..." -ForegroundColor Green

$bootloaderBin = ".pio\build\esp32\bootloader.bin"
$partitionsBin = ".pio\build\esp32\partitions.bin"
$bootApp0 = "C:\Users\simahfud\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"
$destEsp32Factory = Join-Path $OutputDir "agy_universal_esp32_factory.bin"
$esptoolPy = "C:\Users\simahfud\.platformio\packages\tool-esptoolpy\esptool.py"
$pythonExe = "C:\Users\simahfud\.platformio\penv\Scripts\python.exe"

if ((Test-Path $esptoolPy) -and (Test-Path $bootloaderBin) -and (Test-Path $partitionsBin) -and (Test-Path $bootApp0)) {
    Write-Host " -> Menggabungkan bootloader, partitions, boot_app0, dan app via esptool..." -ForegroundColor Yellow
    & $pythonExe $esptoolPy --chip esp32 merge_bin -o $destEsp32Factory --flash_mode dio --flash_size 4MB 0x1000 $bootloaderBin 0x8000 $partitionsBin 0xe000 $bootApp0 0x10000 $destEsp32
    Write-Host " -> Berhasil deploy (Factory 0x0): $destEsp32Factory ($( [math]::Round((Get-Item $destEsp32Factory).Length / 1KB, 1) ) KB)" -ForegroundColor Cyan
} else {
    Write-Host " [WARN] Komponen merge esptool tidak lengkap, melewati factory binary." -ForegroundColor Yellow
}

Write-Host "`n=======================================================" -ForegroundColor Green
Write-Host "  SEMUA FIRMWARE UNIVERSAL BERHASIL DIDEPLOY!" -ForegroundColor Green
Write-Host "=======================================================" -ForegroundColor Green
Get-ChildItem -Path $OutputDir -Filter "*.bin" | Select-Object Name, Length, LastWriteTime | Format-Table -AutoSize
