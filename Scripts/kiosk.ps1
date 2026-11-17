param(
    [ValidateSet('Build','Inspect','Apply','Migrate','Hardware','Interaction','Verify','Gameplay','Package')]
    [string]$Mode = 'Verify',
    [string]$EngineRoot = 'E:\Unreal\UE_5.8',
    [switch]$RenderGameplay
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'Transit.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editor)) { throw "Unreal Editor not found at $editor" }
if ($Mode -eq 'Build') {
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') TransitEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE
    exit $LASTEXITCODE
}
if ($Mode -eq 'Package') {
    # Keep cooked output/cache on the project drive and bypass a full global Zen store.
    # This packages the current prototype's Demo map without changing project settings.
    $packageArgs = @("-ScriptsForProject=$projectFile", 'BuildCookRun', "-project=$projectFile",
        '-target=Transit', "-unrealexe=$editor", '-platform=Win64', '-clientconfig=Shipping',
        '-nop4', '-utf8output', '-nocompileeditor', '-skipbuildeditor', '-installed', '-cook',
        '-map=/Game/Kiosk/Maps/Demo', "-CookOutputDir=$projectRoot\Saved\CookedPOVFiles\Windows",
        '-DDC=(InstalledEnginePak,Local)', "-AdditionalCookerOptions=-SkipZenStore -LocalDataCachePath=$projectRoot\DerivedDataCache",
        '-stage', "-stagingdirectory=$projectRoot\Saved\StagedPOV", '-archive', '-package', '-build',
        '-pak', '-iostore', '-compressed', '-prereqs', '-nodebuginfo', '-nocompile', '-nocompileuat',
        "-archivedirectory=$projectRoot\Saved\Packaged\POVVerified")
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat') @packageArgs
    exit $LASTEXITCODE
}
$engineArgs = @($projectFile, '-unattended', '-nosound', '-nop4', '-nosplash', '-DDC=(InstalledEnginePak,Local)', "-LocalDataCachePath=$projectRoot\DerivedDataCache", "-abslog=$projectRoot\Saved\Logs\Kiosk$Mode.log")
switch ($Mode) {
    'Inspect' { $engineArgs += @('-run=TransitKiosk', '-nullrhi') }
    'Apply' { $engineArgs += @('-run=TransitKiosk', '-Apply', '-nullrhi') }
    'Migrate' { $engineArgs += @('-run=TransitKiosk', '-MigrateDesign', '-nullrhi') }
    'Hardware' { $engineArgs += @('-run=TransitKiosk', '-UpdateHardware', '-nullrhi') }
    'Interaction' { $engineArgs += @('-run=TransitKiosk', '-UpdateInteraction', '-nullrhi') }
    'Verify' { $engineArgs += @('-run=TransitKiosk', '-Verify', '-Render', '-AllowCommandletRendering', '-RenderOffscreen', '-d3d11') }
    'Gameplay' {
        $engineArgs += @('-game', '-KioskSmokeTest', '-RenderOffscreen', '-windowed', '-ResX=1280', '-ResY=720', '-ForceRes', '-NoVSync')
        # Real world-space pointer tests need a rendered widget hit-test grid.
        # Keep RenderGameplay accepted for older commands; Gameplay now always renders.
        $engineArgs += '-d3d11'
    }
}
& $editor @engineArgs
exit $LASTEXITCODE
