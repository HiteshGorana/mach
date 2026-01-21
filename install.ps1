# Mach Installer for Windows PowerShell
# Usage: irm https://raw.githubusercontent.com/HiteshGorana/mach/main/install.ps1 | iex

$ErrorActionPreference = "Stop"

$repo = "HiteshGorana/mach"
$installDir = "$env:USERPROFILE\bin"
$filename = "mach-windows-x86_64.exe"

Write-Host "⚡ Installing Mach..." -ForegroundColor Cyan

# Create install directory
if (!(Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
}

# Get latest release
$release = Invoke-RestMethod "https://api.github.com/repos/$repo/releases/latest"
$asset = $release.assets | Where-Object { $_.name -eq $filename }

if (!$asset) {
    Write-Host "❌ Could not find release for $filename" -ForegroundColor Red
    exit 1
}

Write-Host "   Downloading: $($asset.browser_download_url)"

# Download
$outPath = "$installDir\mach.exe"
Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $outPath

Write-Host "✅ Installed to $outPath" -ForegroundColor Green

# Add to PATH if not already there
$currentPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($currentPath -notlike "*$installDir*") {
    [Environment]::SetEnvironmentVariable("PATH", "$currentPath;$installDir", "User")
    Write-Host "   Added $installDir to PATH (restart terminal to use)" -ForegroundColor Yellow
}

Write-Host ""
& $outPath version
Write-Host ""
Write-Host "🚀 Run 'mach http://example.com' to get started!" -ForegroundColor Cyan
