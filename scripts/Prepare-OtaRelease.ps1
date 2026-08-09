param(
  [Parameter(Mandatory = $true)]
  [string]$BinPath,

  [Parameter(Mandatory = $true)]
  [string]$Version,

  [string]$Notes = "Firmware update"
)

$ErrorActionPreference = "Stop"
$versionNumber = $Version.Trim().TrimStart("v", "V")
if($versionNumber -notmatch '^\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?$') {
  throw "Version must look like 2.2.0"
}
if(-not (Test-Path -LiteralPath $BinPath -PathType Leaf)) {
  throw "Firmware binary was not found: $BinPath"
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$outputDirectory = Join-Path $projectRoot "output\ota\v$versionNumber"
$firmwareDirectory = Join-Path $projectRoot "firmware"
$assetName = "esp32-stock-ticker.bin"
$assetPath = Join-Path $outputDirectory $assetName
$md5Path = "$assetPath.md5"
$manifestPath = Join-Path $firmwareDirectory "manifest.json"

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $firmwareDirectory | Out-Null
Copy-Item -LiteralPath $BinPath -Destination $assetPath -Force

$fileInfo = Get-Item -LiteralPath $assetPath
$md5 = (Get-FileHash -LiteralPath $assetPath -Algorithm MD5).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($md5Path, "$md5  $assetName`n", [Text.UTF8Encoding]::new($false))

$manifest = [ordered]@{
  ready = $true
  version = "v$versionNumber"
  notes = $Notes
  md5 = $md5
  size = $fileInfo.Length
  asset_url = "https://github.com/reckton0852-star/esp32-stock-ticker/releases/download/v$versionNumber/$assetName"
  published_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
  error = ""
}
$json = $manifest | ConvertTo-Json
[IO.File]::WriteAllText($manifestPath, "$json`n", [Text.UTF8Encoding]::new($false))

Write-Host "OTA package prepared:"
Write-Host "  $assetPath"
Write-Host "  $md5Path"
Write-Host "Manifest updated:"
Write-Host "  $manifestPath"
Write-Host ""
Write-Host "Publish the GitHub release and upload both files before pushing manifest.json."
