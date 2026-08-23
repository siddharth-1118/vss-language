# VSS Windows Installer Script
$installDir = "$env:USERPROFILE\.vss"
$zipPath = "$env:TEMP\vss-windows-x64.zip"
$downloadUrl = "https://github.com/siddharth-1118/vss-language/releases/latest/download/vss-windows-x64.zip"
$icoUrl = "https://raw.githubusercontent.com/siddharth-1118/vss-language/main/assets/vss.ico"

Write-Host "[*] Downloading VSS Programming Language..." -ForegroundColor Cyan
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $downloadUrl -OutFile $zipPath -ErrorAction Stop
} catch {
    Write-Host "[x] Error: Failed to download VSS. Please check your internet connection." -ForegroundColor Red
    exit 1
}

Write-Host "[*] Extracting files to $installDir..." -ForegroundColor Cyan
if (Test-Path $installDir) {
    Remove-Item -Recurse -Force $installDir -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Path $installDir -Force | Out-Null

try {
    Expand-Archive -Path $zipPath -DestinationPath $installDir -Force
    Remove-Item $zipPath -ErrorAction SilentlyContinue
} catch {
    Write-Host "[x] Error: Extraction failed." -ForegroundColor Red
    exit 1
}

# Download Icon
Write-Host "[*] Downloading official VSS icon..." -ForegroundColor Cyan
try {
    Invoke-WebRequest -Uri $icoUrl -OutFile "$installDir\vss.ico" -ErrorAction Stop
} catch {
    Write-Host "[!] Warning: Failed to download VSS icon, skipping file association icon." -ForegroundColor Yellow
}

Write-Host "[*] Registering VSS File Associations (.vss, .vssc)..." -ForegroundColor Cyan
try {
    # Set up .vss and .vssc extension association for current user
    New-Item -Path "HKCU:\Software\Classes\.vss" -Value "VSSFile" -Force | Out-Null
    New-Item -Path "HKCU:\Software\Classes\.vssc" -Value "VSSFile" -Force | Out-Null

    # Set up VSSFile description and icon
    New-Item -Path "HKCU:\Software\Classes\VSSFile" -Value "VSS Script File" -Force | Out-Null
    if (Test-Path "$installDir\vss.ico") {
        New-Item -Path "HKCU:\Software\Classes\VSSFile\DefaultIcon" -Value "$installDir\vss.ico" -Force | Out-Null
    }

    # Set up default open command (double-click to run with VSS compiler)
    $openCmd = "`"$installDir\vss.exe`" `"%1`""
    New-Item -Path "HKCU:\Software\Classes\VSSFile\shell\open\command" -Value $openCmd -Force | Out-Null
} catch {
    Write-Host "[!] Warning: Failed to register file associations: $_" -ForegroundColor Yellow
}

Write-Host "[*] Adding VSS to environment PATH..." -ForegroundColor Cyan
$userPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($userPath -notlike "*$installDir*") {
    $newPath = "$userPath;$installDir"
    [Environment]::SetEnvironmentVariable("PATH", $newPath, "User")
    $env:PATH += ";$installDir"
}

Write-Host "`n[+] VSS Installed Successfully!" -ForegroundColor Green
Write-Host "----------------------------------------"
Write-Host "Version: v3.0.0 (Latest)"
Write-Host "Installed at: $installDir"
Write-Host "`nTo start using VSS, restart your terminal and type:"
Write-Host "  vss help" -ForegroundColor Yellow
