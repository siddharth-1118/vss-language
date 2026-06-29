$ErrorActionPreference = 'Stop'

$packageArgs = @{
  packageName   = 'vss'
  unzipLocation = "$(Split-Path -Parent $MyInvocation.MyCommand.Definition)"
  url64         = 'https://github.com/siddharth-1118/vss-language/releases/download/v1.0.0/vss-windows-amd64.zip'
  checksum64    = 'PLACEHOLDER_CHECKSUM_SHA256' # Replace with actual release zip checksum
  checksumType64= 'sha256'
}

Install-ChocolateyZipPackage @packageArgs
