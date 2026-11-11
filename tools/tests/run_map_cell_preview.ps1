param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '../../build-mingw-preset'),
    [string]$FixtureDirectory = (Join-Path $PSScriptRoot '../../build-mingw-preset/map-cell-native-preview'),
    [string]$AssetsDirectory = 'D:\Spel\OldRO'
)
$ErrorActionPreference = 'Stop'
$buildRoot = [IO.Path]::GetFullPath($BuildDirectory)
$fixtureRoot = [IO.Path]::GetFullPath($FixtureDirectory)
$installedAssets = [IO.Path]::GetFullPath($AssetsDirectory)
$executable = Join-Path $buildRoot 'open-midgard.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw 'Build open-midgard.exe before running this fixture.' }
if (-not (Test-Path -LiteralPath $installedAssets -PathType Container)) { throw "Installed asset directory not found: $installedAssets" }
if (-not (Test-Path -LiteralPath (Join-Path $installedAssets 'data.grf') -PathType Leaf)) { throw 'Installed data.grf is required.' }

$stage = Join-Path $fixtureRoot ("stage-" + [Guid]::NewGuid().ToString('N'))
$settings = Join-Path $fixtureRoot 'settings'
New-Item -ItemType Directory -Force -Path $stage,$settings | Out-Null
foreach ($pak in @('data.grf','data_hp.grf','event.grf','fdata.grf')) {
    $sourcePak = Join-Path $installedAssets $pak
    if (Test-Path -LiteralPath $sourcePak -PathType Leaf) {
        New-Item -ItemType HardLink -Path (Join-Path $stage $pak) -Target $sourcePak | Out-Null
    }
}

function Get-BmpPixels([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 54 -or $bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4D) { throw "Capture is not a Windows BMP: $Path" }
    $width = [BitConverter]::ToInt32($bytes,18)
    $height = [Math]::Abs([BitConverter]::ToInt32($bytes,22))
    $bitsPerPixel = [BitConverter]::ToUInt16($bytes,28)
    $compression = [BitConverter]::ToUInt32($bytes,30)
    $offset = [BitConverter]::ToInt32($bytes,10)
    if ($width -le 0 -or $height -le 0 -or $bitsPerPixel -ne 32 -or $compression -ne 0) {
        throw "Expected an uncompressed 32-bit BMP capture: $Path"
    }
    $rowStride = [int]([Math]::Ceiling(($width * 4) / 4.0) * 4)
    $pixelBytes = $rowStride * $height
    if ($offset -lt 0 -or $offset + $pixelBytes -gt $bytes.Length) { throw "Truncated BMP pixels: $Path" }
    return @{ width=$width; height=$height; bytes=$bytes; offset=$offset; length=$pixelBytes }
}

function Compare-BmpPixels([string]$First,[string]$Second) {
    $a = Get-BmpPixels $First
    $b = Get-BmpPixels $Second
    if ($a.width -ne $b.width -or $a.height -ne $b.height -or $a.length -ne $b.length) {
        throw "Capture dimensions differ: $First and $Second"
    }
    $differentPixels = 0
    for ($i = 0; $i -lt $a.length; $i += 4) {
        if ($a.bytes[$a.offset+$i] -ne $b.bytes[$b.offset+$i] -or
            $a.bytes[$a.offset+$i+1] -ne $b.bytes[$b.offset+$i+1] -or
            $a.bytes[$a.offset+$i+2] -ne $b.bytes[$b.offset+$i+2] -or
            $a.bytes[$a.offset+$i+3] -ne $b.bytes[$b.offset+$i+3]) { $differentPixels++ }
    }
    return @{ width=$a.width; height=$a.height; differentPixels=$differentPixels }
}

$environmentNames = @('OPEN_MIDGARD_DATA_DIR','OPEN_MIDGARD_RENDER_BACKEND','RO_HEAL_PREVIEW_ASSETS',
    'RO_HEAL_PREVIEW_OUT','RO_MAP_CELL_PREVIEW')
$savedEnvironment = @{}
foreach ($name in $environmentNames) { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
$executableHash = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
$runs = @()
try {
    $env:OPEN_MIDGARD_DATA_DIR = $settings
    $env:OPEN_MIDGARD_RENDER_BACKEND = 'vulkan'
    $env:RO_HEAL_PREVIEW_ASSETS = $stage
    foreach ($scene in @('baseline','blocked','restored','no-hover')) {
        $capture = Join-Path $fixtureRoot "map-cell-$scene.bmp"
        if (Test-Path -LiteralPath $capture) { Remove-Item -LiteralPath $capture -Force }
        $env:RO_MAP_CELL_PREVIEW = $scene
        $env:RO_HEAL_PREVIEW_OUT = $capture
        $started = [DateTime]::UtcNow
        $process = Start-Process -FilePath $executable -WorkingDirectory $stage -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(60000)) {
            Stop-Process -Id $process.Id -Force
            throw "Native map-cell preview timed out: $scene"
        }
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $capture -PathType Leaf)) {
            throw "Native map-cell preview failed: $scene (exit $($process.ExitCode))"
        }
        if ((Get-Item -LiteralPath $capture).LastWriteTimeUtc -lt $started.AddSeconds(-1)) {
            throw "Native map-cell preview did not produce a fresh capture: $scene"
        }
        $bmp = Get-BmpPixels $capture
        $runs += @{scene=$scene;pid=$process.Id;exit=$process.ExitCode;capture=$capture;
            width=$bmp.width;height=$bmp.height;startedUtc=$started.ToString('o');executableSha256=$executableHash}
    }
    $baselineBlocked = Compare-BmpPixels (Join-Path $fixtureRoot 'map-cell-baseline.bmp') (Join-Path $fixtureRoot 'map-cell-blocked.bmp')
    $baselineRestored = Compare-BmpPixels (Join-Path $fixtureRoot 'map-cell-baseline.bmp') (Join-Path $fixtureRoot 'map-cell-restored.bmp')
    $blockedNoHover = Compare-BmpPixels (Join-Path $fixtureRoot 'map-cell-blocked.bmp') (Join-Path $fixtureRoot 'map-cell-no-hover.bmp')
    if ($baselineBlocked.differentPixels -le 0) { throw 'Expected baseline and blocked hover frames to differ.' }
    if ($baselineRestored.differentPixels -ne 0) { throw 'Restored hover frame differs from baseline.' }
    if ($blockedNoHover.differentPixels -ne 0) { throw 'Blocked hover frame differs from no-hover frame.' }
    if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -ne $executableHash) { throw 'Fixture executable changed during capture.' }
    $result = @{outcome='PASS';backend='vulkan';assetStage=$stage;executableSha256=$executableHash;
        comparisons=@{baselineVsBlocked=$baselineBlocked;baselineVsRestored=$baselineRestored;blockedVsNoHover=$blockedNoHover};runs=$runs}
    $json = $result | ConvertTo-Json -Depth 6 -Compress
    $json | Set-Content -LiteralPath (Join-Path $fixtureRoot 'map-cell-preview-results.json') -Encoding utf8
    Write-Output $json
} finally {
    foreach ($name in $environmentNames) { [Environment]::SetEnvironmentVariable($name,$savedEnvironment[$name],'Process') }
}
