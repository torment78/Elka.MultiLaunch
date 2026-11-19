param([ValidateSet('Debug','Release')][string]$Configuration = 'Release', [switch]$Package)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Install Visual Studio with Desktop development with C++.' }
$version = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
$toolset = if ([version]$version -ge [version]'18.0') { 'v145' } else { 'v143' }
if (-not (Test-Path (Join-Path $root 'assets\app.ico'))) { & "$PSScriptRoot\New-Icon.ps1" }
& "$vs\MSBuild\Current\Bin\MSBuild.exe" "$root\Elka.MultiLaunch.sln" /m /nologo "/p:Configuration=$Configuration" /p:Platform=x64 "/p:PlatformToolset=$toolset" /v:minimal
if ($LASTEXITCODE) { throw "Build failed ($LASTEXITCODE)." }
& "$root\build\$Configuration\Elka.MultiLaunch.Tests.exe"
if ($LASTEXITCODE) { throw "Tests failed ($LASTEXITCODE)." }
if ($Package) {
    $out = Join-Path $root 'artifacts'
    New-Item -ItemType Directory -Path $out -Force | Out-Null
    Copy-Item -LiteralPath "$root\build\$Configuration\Elka.MultiLaunch.exe" -Destination "$out\Elka.MultiLaunch.exe" -Force
    Compress-Archive -LiteralPath "$out\Elka.MultiLaunch.exe", "$root\README.md", "$root\VALIDATION.md" -DestinationPath "$out\Elka.MultiLaunch-0.2.0-dev.3-win-x64.zip" -Force
    $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
    $releaseFiles = @("$out\Elka.MultiLaunch.exe", "$out\Elka.MultiLaunch-0.2.0-dev.3-win-x64.zip")
    if (Test-Path $iscc) { & $iscc "/DConfiguration=$Configuration" "$root\installer\Elka.MultiLaunch.iss"; if ($LASTEXITCODE) { throw 'Installer build failed.' }; $releaseFiles += "$out\Elka.MultiLaunch-0.2.0-dev.3-setup-win-x64.exe" }
    Get-FileHash -LiteralPath $releaseFiles | ForEach-Object { $_.Hash + '  ' + [IO.Path]::GetFileName($_.Path) } | Set-Content -LiteralPath "$out\SHA256SUMS-0.2.0-dev.3.txt"
    Copy-Item -LiteralPath "$out\SHA256SUMS-0.2.0-dev.3.txt" -Destination "$out\SHA256SUMS.txt" -Force
    Get-ChildItem -LiteralPath $out -File | Select-Object Name, Length
}
