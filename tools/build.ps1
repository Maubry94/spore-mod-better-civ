[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SporeSdkPath,
    [Parameter(Mandatory)][string]$SporeLauncherPath,
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [string]$ReferenceExecutable,
    [string]$MSBuildPath
)
$ErrorActionPreference = 'Stop'
$repoPath = Split-Path -Parent $PSScriptRoot
if (-not $MSBuildPath) {
    $command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($command) { $MSBuildPath = $command.Source }
    else {
        $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (Test-Path -LiteralPath $vswherePath) {
            $MSBuildPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild/Current/Bin/MSBuild.exe' | Select-Object -First 1
        }
    }
}
if (-not $MSBuildPath) { throw 'Install Visual Studio C++ Build Tools, or pass -MSBuildPath.' }
$sdkPath = (Resolve-Path -LiteralPath $SporeSdkPath).Path
$launcherPath = (Resolve-Path -LiteralPath $SporeLauncherPath).Path
$sdkOutput = Join-Path $repoPath 'build/Sdk/'
$sdkIntermediate = Join-Path $repoPath 'build/Sdk/obj/'
$sdkOverrides = Join-Path $PSScriptRoot 'SdkBuildOverrides.props'
& $MSBuildPath (Join-Path $sdkPath 'Spore ModAPI/Spore ModAPI.vcxproj') /m /nologo /v:minimal /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 "/p:OutDir=$sdkOutput" "/p:IntDir=$sdkIntermediate" "/p:ForceImportBeforeCppTargets=$sdkOverrides"
if ($LASTEXITCODE -ne 0) { throw "SDK static library build failed ($LASTEXITCODE)." }
$arguments = @('/m', '/nologo', '/v:minimal', "/p:Configuration=$Configuration", '/p:Platform=Win32', "/p:SporeSdkPath=$sdkPath", "/p:SporeLauncherPath=$launcherPath")
& $MSBuildPath (Join-Path $repoPath 'BetterCiv.vcxproj') @arguments
if ($LASTEXITCODE -ne 0) { throw "DLL build failed ($LASTEXITCODE)." }
if ($ReferenceExecutable) {
    $referencePath = (Resolve-Path -LiteralPath $ReferenceExecutable).Path
    & $MSBuildPath (Join-Path $repoPath 'BetterCiv.Tests.vcxproj') @arguments
    if ($LASTEXITCODE -ne 0) { throw "Test build failed ($LASTEXITCODE)." }
    & $MSBuildPath (Join-Path $repoPath 'BetterCiv.LoaderFixture.vcxproj') @arguments
    if ($LASTEXITCODE -ne 0) { throw "Loader fixture build failed ($LASTEXITCODE)." }
    & (Join-Path $repoPath 'tests/mLibs/test-better-civ.exe') $referencePath (Join-Path $repoPath 'tests/mLibs/SporeModAPI.dll')
    if ($LASTEXITCODE -ne 0) { throw "Tests failed ($LASTEXITCODE)." }
    & (Join-Path $repoPath 'tests/mLibs/test-better-civ.exe') --loader (Join-Path $repoPath 'tests/mLibs/SporeModAPI.dll') (Join-Path $repoPath "dist/$Configuration/BetterCiv.dll")
    if ($LASTEXITCODE -ne 0) { throw "DLL loader test failed ($LASTEXITCODE)." }
}
