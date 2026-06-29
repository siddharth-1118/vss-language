# install.ps1
# official VSS installer for Windows

$vssDir = Join-Path $HOME ".vss"
$binDir = Join-Path $vssDir "bin"

if (!(Test-Path $binDir)) {
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
}

# Create vss.bat wrapper in bin directory pointing to the compiled compiler under WSL
$batContent = @'
@echo off
wsl sh -c "cd '$(wslpath -u "%CD%")' && /mnt/e/vss-language/vss/vss %*"
'@

$batPath = Join-Path $binDir "vss.bat"
Set-Content -Path $batPath -Value $batContent

# Update PATH globally for current user
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($userPath -notlike "*$binDir*") {
    [Environment]::SetEnvironmentVariable("Path", "$userPath;$binDir", "User")
    $env:Path = "$env:Path;$binDir"
    Write-Host "`nVSS has been successfully installed globally!" -ForegroundColor Green
    Write-Host "Please restart your terminal to make the 'vss' command available.`n"
} else {
    Write-Host "`nVSS is already configured in system PATH.`n" -ForegroundColor Yellow
}
