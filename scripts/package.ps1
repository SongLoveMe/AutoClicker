[CmdletBinding()]
param(
    [string]$InnoSetupPath
)

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$qtRoot = 'D:\Qt\6.5.3\mingw_64'
$mingwRoot = 'D:\Qt\Tools\mingw1310_64'
$cmake = 'D:\Qt\Tools\CMake_64\bin\cmake.exe'
$ninja = 'D:\Qt\Tools\Ninja\ninja.exe'
$gcc = Join-Path $mingwRoot 'bin\gcc.exe'
$gpp = Join-Path $mingwRoot 'bin\g++.exe'
$cmakeLists = Join-Path $projectRoot 'CMakeLists.txt'
$buildDir = Join-Path $projectRoot 'build-release'
$distDir = Join-Path $projectRoot 'dist\AutoClicker'
$releaseDir = Join-Path $projectRoot 'release'
$installerScript = Join-Path $projectRoot 'installer\AutoClicker.iss'

if ([string]::IsNullOrWhiteSpace($InnoSetupPath)) {
    $innoCandidates = @(
        'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
        'C:\Program Files\Inno Setup 6\ISCC.exe'
    )
    $InnoSetupPath = $innoCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

$deploymentSources = @(
    # This project currently links Qt Core, Gui, and Widgets only.
    (Join-Path $qtRoot 'bin\Qt6Core.dll'),
    (Join-Path $qtRoot 'bin\Qt6Gui.dll'),
    (Join-Path $qtRoot 'bin\Qt6Widgets.dll'),
    (Join-Path $qtRoot 'plugins\platforms\qwindows.dll'),
    (Join-Path $mingwRoot 'bin\libgcc_s_seh-1.dll'),
    (Join-Path $mingwRoot 'bin\libstdc++-6.dll'),
    (Join-Path $mingwRoot 'bin\libwinpthread-1.dll')
)
$requiredPaths = @($cmake, $ninja, $gcc, $gpp, $installerScript) + $deploymentSources
$missingPaths = $requiredPaths | Where-Object { [string]::IsNullOrWhiteSpace($_) -or -not (Test-Path -LiteralPath $_) }
if ([string]::IsNullOrWhiteSpace($InnoSetupPath) -or -not (Test-Path -LiteralPath $InnoSetupPath)) {
    throw "Inno Setup 6 was not found. Install it from https://jrsoftware.org/isdl.php or pass -InnoSetupPath <path-to-ISCC.exe>."
}
if ($missingPaths.Count -gt 0) {
    throw "Required release tool or file was not found:`n$($missingPaths -join "`n")`nInstall Inno Setup 6 or pass -InnoSetupPath <path-to-ISCC.exe>."
}

$projectText = Get-Content -LiteralPath $cmakeLists -Raw
$versionMatch = [regex]::Match($projectText, 'project\s*\(\s*AutoClicker\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)\s+LANGUAGES', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
if (-not $versionMatch.Success) {
    throw "CMakeLists.txt must declare project(AutoClicker VERSION x.y.z LANGUAGES CXX)."
}
$version = $versionMatch.Groups[1].Value
$executable = Join-Path $buildDir 'AutoClicker.exe'
$installer = Join-Path $releaseDir "AutoClicker-Setup-$version.exe"
$hashFile = "$installer.sha256"

# Use the Qt MinGW runtime while configuring, deploying, and smoke-testing the release.
$env:PATH = "$($mingwRoot)\bin;$($qtRoot)\bin;$env:PATH"

foreach ($directory in @($buildDir, $distDir)) {
    if (Test-Path -LiteralPath $directory) {
        Remove-Item -LiteralPath $directory -Recurse -Force
    }
}
New-Item -ItemType Directory -Path $distDir -Force | Out-Null
New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null
Remove-Item -LiteralPath $installer, $hashFile -Force -ErrorAction SilentlyContinue

& $cmake -S $projectRoot -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$qtRoot" "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_C_COMPILER=$gcc" "-DCMAKE_CXX_COMPILER=$gpp"
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }

& $cmake --build $buildDir
if ($LASTEXITCODE -ne 0) { throw "Release build failed." }

if (-not (Test-Path -LiteralPath $executable)) {
    throw "Expected executable was not generated: $executable"
}

Copy-Item -LiteralPath $executable -Destination $distDir
Copy-Item -LiteralPath $deploymentSources[0..2] -Destination $distDir
New-Item -ItemType Directory -Path (Join-Path $distDir 'platforms') -Force | Out-Null
Copy-Item -LiteralPath $deploymentSources[3] -Destination (Join-Path $distDir 'platforms')
Copy-Item -LiteralPath $deploymentSources[4..6] -Destination $distDir

$requiredDeploymentFiles = @(
    (Join-Path $distDir 'AutoClicker.exe'),
    (Join-Path $distDir 'Qt6Core.dll'),
    (Join-Path $distDir 'Qt6Gui.dll'),
    (Join-Path $distDir 'Qt6Widgets.dll'),
    (Join-Path $distDir 'platforms\qwindows.dll')
)
$missingDeploymentFiles = $requiredDeploymentFiles | Where-Object { -not (Test-Path -LiteralPath $_) }
if ($missingDeploymentFiles) {
    throw "Deployment is incomplete:`n$($missingDeploymentFiles -join "`n")"
}

& $InnoSetupPath "/DMyAppVersion=$version" $installerScript
if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed." }

if (-not (Test-Path -LiteralPath $installer)) {
    throw "Expected installer was not generated: $installer"
}

$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $(Split-Path -Leaf $installer)" | Set-Content -LiteralPath $hashFile -Encoding ASCII

Write-Host "Release package created: $installer"
Write-Host "SHA-256 file created: $hashFile"
