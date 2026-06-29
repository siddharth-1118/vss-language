# uninstall.ps1
# official VSS uninstaller for Windows

$vssDir = Join-Path $HOME ".vss"
$binDir = Join-Path $vssDir "bin"

if (Test-Path $vssDir) {
    Remove-Item -Recurse -Force $vssDir
    Write-Host "VSS installation files deleted." -ForegroundColor Green
}

# Remove bin directory from user PATH
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($userPath -like "*$binDir*") {
    $cleanPath = ($userPath -split ';' | Where-Object { $_ -ne $binDir }) -join ';'
    [Environment]::SetEnvironmentVariable("Path", $cleanPath, "User")
    Write-Host "VSS has been successfully removed from PATH." -ForegroundColor Green
} else {
    Write-Host "VSS is not found in PATH." -ForegroundColor Yellow
}
