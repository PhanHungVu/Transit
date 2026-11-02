param(
    [ValidateSet('Build','Inspect','Apply','Verify','Gameplay')]
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
$engineArgs = @($projectFile, '-unattended', '-nosound', '-nop4', '-nosplash', '-DDC=(InstalledEnginePak,Local)', "-LocalDataCachePath=$projectRoot\DerivedDataCache", "-abslog=$projectRoot\Saved\Logs\Kiosk$Mode.log")
switch ($Mode) {
    'Inspect' { $engineArgs += @('-run=TransitKiosk', '-nullrhi') }
    'Apply' { $engineArgs += @('-run=TransitKiosk', '-Apply', '-nullrhi') }
    'Verify' { $engineArgs += @('-run=TransitKiosk', '-Verify', '-Render', '-AllowCommandletRendering', '-RenderOffscreen', '-d3d11') }
    'Gameplay' {
        $engineArgs += @('-game', '-KioskSmokeTest', '-RenderOffscreen', '-windowed', '-ResX=1280', '-ResY=720', '-NoVSync')
        $engineArgs += $(if ($RenderGameplay) { '-d3d11' } else { '-nullrhi' })
    }
}
& $editor @engineArgs
exit $LASTEXITCODE
