param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '../../build-mingw-preset'),
    [string]$FixtureDirectory = (Join-Path $PSScriptRoot '../../build-mingw-preset/siege-emblem-preview')
)
$ErrorActionPreference = 'Stop'
$buildRoot = [IO.Path]::GetFullPath($BuildDirectory)
$fixtureRoot = [IO.Path]::GetFullPath($FixtureDirectory)
$executable = Join-Path $buildRoot 'open-midgard.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw 'Build open-midgard.exe before running this fixture.'
}

# Use an isolated working directory and settings root. This fixture is a
# synthetic UI overlay and deliberately does not stage or read GRFs.
$workingRoot = Join-Path $fixtureRoot 'working'
$settings = Join-Path $fixtureRoot 'settings'
New-Item -ItemType Directory -Force -Path $workingRoot,$settings | Out-Null
$effect = Join-Path $workingRoot 'data/texture/effect'
New-Item -ItemType Directory -Force -Path $effect | Out-Null
# The same red-square 24x24 BMP as the packet fixture, compressed in the Ref EBM format.
$ebm = 'eAFz8v3GxgAGZkBSA4gloJgRzAJLYRD/Gf6PoiEdAgwM/wkiYjxI0BCgglFz8IfSaPgMv/AhJk5H1QxUCAAAJhvucA=='
[IO.File]::WriteAllBytes((Join-Path $effect 'flagGraffiti12.ebm'), [Convert]::FromBase64String($ebm))

function Get-BmpPixels([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 54 -or $bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4D) {
        throw "Capture is not a Windows BMP: $Path"
    }
    $width = [BitConverter]::ToInt32($bytes,18)
    $signedHeight = [BitConverter]::ToInt32($bytes,22)
    $height = [Math]::Abs($signedHeight)
    $bitsPerPixel = [BitConverter]::ToUInt16($bytes,28)
    $compression = [BitConverter]::ToUInt32($bytes,30)
    $offset = [BitConverter]::ToInt32($bytes,10)
    if ($width -ne 640 -or $height -ne 480 -or $bitsPerPixel -ne 32 -or $compression -ne 0) {
        throw "Expected an uncompressed 640x480 32-bit BMP capture: $Path"
    }
    $rowStride = [int]([Math]::Ceiling(($width * 4) / 4.0) * 4)
    $pixelBytes = $rowStride * $height
    if ($offset -lt 0 -or $offset + $pixelBytes -gt $bytes.Length) {
        throw "Truncated BMP pixels: $Path"
    }
    return @{ width=$width; height=$height; signedHeight=$signedHeight;
        bytes=$bytes; offset=$offset; stride=$rowStride }
}

function Compare-BmpPixels([string]$First,[string]$Second) {
    $a = Get-BmpPixels $First
    $b = Get-BmpPixels $Second
    if ($a.width -ne $b.width -or $a.height -ne $b.height) {
        throw "Capture dimensions differ: $First and $Second"
    }
    $differentPixels = 0
    $minX = $a.width
    $minY = $a.height
    $maxX = -1
    $maxY = -1
    for ($y = 0; $y -lt $a.height; $y++) {
        $rowA = if ($a.signedHeight -gt 0) { $a.height - 1 - $y } else { $y }
        $rowB = if ($b.signedHeight -gt 0) { $b.height - 1 - $y } else { $y }
        for ($x = 0; $x -lt $a.width; $x++) {
            $pixelA = $a.offset + $rowA * $a.stride + $x * 4
            $pixelB = $b.offset + $rowB * $b.stride + $x * 4
            if ($a.bytes[$pixelA] -ne $b.bytes[$pixelB] -or
                $a.bytes[$pixelA+1] -ne $b.bytes[$pixelB+1] -or
                $a.bytes[$pixelA+2] -ne $b.bytes[$pixelB+2] -or
                $a.bytes[$pixelA+3] -ne $b.bytes[$pixelB+3]) {
                $differentPixels++
                $minX = [Math]::Min($minX,$x)
                $minY = [Math]::Min($minY,$y)
                $maxX = [Math]::Max($maxX,$x)
                $maxY = [Math]::Max($maxY,$y)
            }
        }
    }
    $bbox = $null
    if ($differentPixels -gt 0) {
        $bbox = @{x=$minX;y=$minY;width=($maxX-$minX+1);height=($maxY-$minY+1)}
    }
    return @{ width=$a.width; height=$a.height; differentPixels=$differentPixels; bbox=$bbox }
}

$environmentNames = @('OPEN_MIDGARD_DATA_DIR','OPEN_MIDGARD_RENDER_BACKEND',
    'RO_HEAL_PREVIEW_OUT','RO_HEAL_PREVIEW_ASSETS','RO_SIEGE_EMBLEM_PREVIEW')
$savedEnvironment = @{}
foreach ($name in $environmentNames) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$executableHash = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
$runs = @()
try {
    $env:OPEN_MIDGARD_DATA_DIR = $settings
    $env:OPEN_MIDGARD_RENDER_BACKEND = 'vulkan'
    $env:RO_HEAL_PREVIEW_ASSETS = $workingRoot
    foreach ($scene in @('ordinary','hidden','wrong-version','visible','graffiti','missing-graffiti')) {
        $capture = Join-Path $fixtureRoot "siege-emblem-$scene.bmp"
        if (Test-Path -LiteralPath $capture) { Remove-Item -LiteralPath $capture -Force }
        $env:RO_SIEGE_EMBLEM_PREVIEW = $scene
        $env:RO_HEAL_PREVIEW_OUT = $capture
        $started = [DateTime]::UtcNow
        $process = Start-Process -FilePath $executable -WorkingDirectory $workingRoot -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(60000)) {
            Stop-Process -Id $process.Id -Force
            throw "Native siege-emblem preview timed out: $scene"
        }
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $capture -PathType Leaf)) {
            throw "Native siege-emblem preview failed: $scene (exit $($process.ExitCode))"
        }
        if ((Get-Item -LiteralPath $capture).LastWriteTimeUtc -lt $started.AddSeconds(-1)) {
            throw "Native siege-emblem preview did not produce a fresh capture: $scene"
        }
        $bmp = Get-BmpPixels $capture
        $runs += @{scene=$scene;pid=$process.Id;exit=$process.ExitCode;capture=$capture;
            width=$bmp.width;height=$bmp.height;startedUtc=$started.ToString('o');
            executableSha256=$executableHash}
    }

    $ordinary = Join-Path $fixtureRoot 'siege-emblem-ordinary.bmp'
    $hidden = Join-Path $fixtureRoot 'siege-emblem-hidden.bmp'
    $wrongVersion = Join-Path $fixtureRoot 'siege-emblem-wrong-version.bmp'
    $visible = Join-Path $fixtureRoot 'siege-emblem-visible.bmp'
    $ordinaryHidden = Compare-BmpPixels $ordinary $hidden
    $ordinaryWrongVersion = Compare-BmpPixels $ordinary $wrongVersion
    $ordinaryVisible = Compare-BmpPixels $ordinary $visible
    $visibleGraffiti = Compare-BmpPixels $visible (Join-Path $fixtureRoot 'siege-emblem-graffiti.bmp')
    $ordinaryMissingGraffiti = Compare-BmpPixels $ordinary (Join-Path $fixtureRoot 'siege-emblem-missing-graffiti.bmp')
    if ($ordinaryHidden.differentPixels -ne 0) { throw 'Hidden capture differs from ordinary capture.' }
    if ($ordinaryWrongVersion.differentPixels -ne 0) { throw 'Wrong-version capture differs from ordinary capture.' }
    if ($ordinaryVisible.differentPixels -ne 144 -or $ordinaryVisible.bbox.width -ne 12 -or
        $ordinaryVisible.bbox.height -ne 12) {
        throw "Visible emblem changed $($ordinaryVisible.differentPixels) pixels; expected one 12x12 square."
    }
    if ($visibleGraffiti.differentPixels -ne 0) { throw 'Graffiti capture differs from the matching download.' }
    if ($ordinaryMissingGraffiti.differentPixels -ne 0) { throw 'Missing graffiti changed the ordinary frame.' }
    if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -ne $executableHash) {
        throw 'Fixture executable changed during capture.'
    }
    $result = @{outcome='PASS';backend='vulkan';workingDirectory=$workingRoot;
        settingsDirectory=$settings;executableSha256=$executableHash;
        comparisons=@{ordinaryVsHidden=$ordinaryHidden;ordinaryVsWrongVersion=$ordinaryWrongVersion;
            ordinaryVsVisible=$ordinaryVisible;visibleVsGraffiti=$visibleGraffiti;
            ordinaryVsMissingGraffiti=$ordinaryMissingGraffiti};runs=$runs}
    $json = $result | ConvertTo-Json -Depth 6 -Compress
    $json | Set-Content -LiteralPath (Join-Path $fixtureRoot 'siege-emblem-preview-results.json') -Encoding utf8
    Write-Output $json
} finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name,$savedEnvironment[$name],'Process')
    }
}
