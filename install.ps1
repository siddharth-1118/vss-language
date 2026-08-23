# VSS Windows Installer Script
$installDir = "$env:USERPROFILE\.vss"
$zipPath = "$env:TEMP\vss-windows-x64.zip"
$downloadUrl = "https://github.com/siddharth-1118/vss-language/releases/latest/download/vss-windows-x64.zip"

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
