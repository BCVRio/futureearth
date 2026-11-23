# Rebuilds the C++ module, regenerates both levels from scratch, and captures
# reference screenshots into design/captures. The climate service must be running
# (services/climate) and GOOGLE_MAPS_API_KEY must be set for the real-world level.
#
#   powershell -File Scripts\rebuild_and_capture.ps1 [-Engine <UE root>] [-SkipBuild] [-SkipGenerate]
param(
    [string]$Engine = "F:\Epic511\Unreal511\UE_5.8",
    [switch]$SkipBuild,
    [switch]$SkipGenerate,
    [int]$Width = 1920,
    [int]$Height = 1080
)

$root = Split-Path -Parent $PSScriptRoot
$proj = Join-Path $root "FutureEarth.uproject"
$captures = Join-Path (Split-Path -Parent (Split-Path -Parent $root)) "design\captures"
$cmd = Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$editor = Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor.exe"
# Never echo the key: strip it from any log line we print.
function Hide-Key($line) { $line -replace 'key=[^\s&"'']+', 'key=REDACTED' -replace 'AIza[\w-]+', 'REDACTED' }

if (-not $SkipBuild) {
    & (Join-Path $Engine "Engine\Build\BatchFiles\Build.bat") FutureEarthEditor Win64 Development "-Project=$proj" -WaitMutex -NoHotReload |
        Out-String -Stream | Select-String -Pattern "error|Result:" | ForEach-Object { $_.Line }
    if ($LASTEXITCODE -ne 0) { Write-Output "BUILD FAILED"; exit 1 }
}

if (-not $SkipGenerate) {
    $generated = Join-Path $root "Content\FutureEarth"
    if (Test-Path $generated) { Remove-Item -Recurse -Force $generated -Confirm:$false }
    foreach ($script in "build_test_street.py", "build_real_world.py") {
        $log = Join-Path $env:TEMP "futureearth-$script.log"
        Start-Process -FilePath $cmd -ArgumentList "`"$proj`"", "-ExecutePythonScript=`"$PSScriptRoot\$script`"", "-unattended", "-nullrhi", "-nosplash", "-nosound", "-nopause", "-abslog=`"$log`"" -Wait -WindowStyle Hidden
        $lines = Select-String -Path $log -Pattern "LogPython: Error|built OK|LogMaterial: (Error|Warning)|error X\d+" | Select-Object -Last 12 | ForEach-Object { Hide-Key $_.Line }
        $lines
        if (-not ($lines -match "built OK")) { Write-Output "GENERATION FAILED: $script"; exit 1 }
    }
}

if (-not $env:GOOGLE_MAPS_API_KEY) { $env:GOOGLE_MAPS_API_KEY = [Environment]::GetEnvironmentVariable("GOOGLE_MAPS_API_KEY", "User") }
New-Item -ItemType Directory -Force $captures | Out-Null
$shots = Join-Path $root "Saved\Screenshots"
$runs = @(
    @{ map = "TestStreet"; name = "test-street"; year = 2026; wait = 12 },
    @{ map = "TestStreet"; name = "test-street"; year = 2526; wait = 12 },
    @{ map = "RealWorld";  name = "ocean-drive"; year = 2026; wait = 60 },
    @{ map = "RealWorld";  name = "ocean-drive"; year = 2526; wait = 60 }
)
foreach ($run in $runs) {
    if (Test-Path $shots) { Remove-Item -Recurse -Force $shots -Confirm:$false }
    $log = Join-Path $env:TEMP "futureearth-$($run.name)-$($run.year).log"
    $p = Start-Process -FilePath $editor -ArgumentList "`"$proj`"", "/Game/FutureEarth/Maps/$($run.map)", "-game", "-windowed", "-ResX=$Width", "-ResY=$Height", "-FutureYear=$($run.year)", "-FutureEarthCapture=$($run.wait)", "-unattended", "-nosplash", "-nosound", "-abslog=`"$log`"" -PassThru
    if (-not $p.WaitForExit(300000)) { $p.Kill(); Write-Output "TIMEOUT $($run.name) $($run.year)" }
    Select-String -Path $log -Pattern "FutureEarth applied|LogFutureEarth: Warning|FutureEarth: .* has no map tiles|LogMaterial: Error|LogShaderCompilers: (Error|Warning)" | Select-Object -Last 4 | ForEach-Object { Hide-Key $_.Line }
    $png = Get-ChildItem $shots -Recurse -Filter *.png -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($png) { Copy-Item $png.FullName (Join-Path $captures "$($run.name)-$($run.year).png") -Force; Write-Output "saved $($run.name)-$($run.year).png" }
    else { Write-Output "NO SCREENSHOT for $($run.name) $($run.year)" }
}
