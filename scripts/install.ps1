# Mach Installer for Windows PowerShell
# Usage: irm https://raw.githubusercontent.com/HiteshGorana/mach/main/scripts/install.ps1 | iex
#        & ([scriptblock]::Create((irm <url>/install.ps1))) -Uninstall          # keep history
#        & ([scriptblock]::Create((irm <url>/install.ps1))) -Uninstall -Purge   # also delete ~/.mach

param([switch]$Uninstall, [switch]$Purge)

$ErrorActionPreference = "Stop"

$repo = "HiteshGorana/mach"
$installDir = "$env:USERPROFILE\bin"
$filename = "mach-windows-x86_64.exe"

if ($Uninstall) {
    $exe = "$installDir\mach.exe"
    if (Test-Path $exe) {
        Remove-Item $exe -Force
        Write-Host "🗑️  Removed $exe" -ForegroundColor Yellow
    } else {
        Write-Host "   No binary found at $exe"
    }

    $userPath = [Environment]::GetEnvironmentVariable("PATH", "User")
    $newPath = (($userPath -split ";") | Where-Object { $_ -and ($_ -ne $installDir) }) -join ";"
    if ($newPath -ne $userPath) {
        [Environment]::SetEnvironmentVariable("PATH", $newPath, "User")
        Write-Host "   Removed $installDir from PATH" -ForegroundColor Yellow
    }

    $dataDir = "$env:USERPROFILE\.mach"
    if ($Purge -and (Test-Path $dataDir)) {
        Remove-Item $dataDir -Recurse -Force
        Write-Host "🗑️  Removed $dataDir (history and tags)" -ForegroundColor Yellow
    } else {
        Write-Host "   Kept history and tags in $dataDir (pass -Purge to delete)"
    }

    Write-Host "✅ Mach uninstalled." -ForegroundColor Green
    exit 0
}

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
