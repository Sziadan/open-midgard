param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '../../build-mingw-preset'),
    [string]$FixtureDirectory = (Join-Path $PSScriptRoot '../../build-mingw-preset/showefst-blade-stop-preview'),
    [string]$PetAssetsDirectory = (Join-Path $PSScriptRoot '../../build-mingw-preset/packet-pet-window-renders/pet-window-assets')
)
$ErrorActionPreference = 'Stop'
$buildRoot = [IO.Path]::GetFullPath($BuildDirectory)
$fixtureRoot = Join-Path ([IO.Path]::GetFullPath($FixtureDirectory)) ([guid]::NewGuid().ToString('N'))
$petAssetsRoot = [IO.Path]::GetFullPath($PetAssetsDirectory)
$executable = Join-Path $buildRoot 'open-midgard.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Build open-midgard.exe before running this fixture: $executable"
}
if (-not (Test-Path -LiteralPath $petAssetsRoot -PathType Container)) {
    throw "Pet asset source directory not found: $petAssetsRoot"
}

$stage = Join-Path $fixtureRoot 'working'
$settings = Join-Path $fixtureRoot 'settings'
$monsterDestination = Join-Path $stage 'data/sprite/monster'
New-Item -ItemType Directory -Force -Path $monsterDestination,$settings | Out-Null
# Use only the Poring ACT/SPR pair in a clean sprite directory.
$poringFiles = @(Get-ChildItem -LiteralPath $petAssetsRoot -Directory -Recurse | ForEach-Object {
    $candidateDirectory = $_.FullName
    foreach ($extension in @('.ACT','.SPR')) {
        $candidate = Join-Path $candidateDirectory ('PORING' + $extension)
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { Get-Item -LiteralPath $candidate }
    }
})
foreach ($extension in @('.ACT','.SPR')) {
    $spriteMatches = @($poringFiles | Where-Object { $_.Name -ieq ('PORING' + $extension) })
    if ($spriteMatches.Count -ne 1) {
        throw "Expected exactly one PORING$extension under $petAssetsRoot; found $($spriteMatches.Count)."
    }
    Copy-Item -LiteralPath $spriteMatches[0].FullName -Destination (Join-Path $monsterDestination ('PORING' + $extension)) -Force
}

function Read-BmpPixels([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 54 -or $bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4d) {
        throw "Capture is not a Windows BMP: $Path"
    }
    $offset = [BitConverter]::ToInt32($bytes,10)
    $width = [BitConverter]::ToInt32($bytes,18)
    $signedHeight = [BitConverter]::ToInt32($bytes,22)
    $height = [Math]::Abs($signedHeight)
    $bpp = [BitConverter]::ToUInt16($bytes,28)
    $compression = [BitConverter]::ToUInt32($bytes,30)
    if ($width -ne 640 -or $height -ne 480 -or $bpp -ne 32 -or $compression -ne 0) {
        throw "Expected a fresh uncompressed 640x480 32-bit BMP: $Path"
    }
    $stride = $width * 4
    $pixels = [byte[]]::new($stride * $height)
    for ($y = 0; $y -lt $height; ++$y) {
        $sourceY = if ($signedHeight -gt 0) { $height - 1 - $y } else { $y }
        [Array]::Copy($bytes, $offset + $sourceY * $stride, $pixels, $y * $stride, $stride)
    }
    return @{width=$width;height=$height;pixels=$pixels;stride=$stride}
}

$environmentNames = @('OPEN_MIDGARD_DATA_DIR','OPEN_MIDGARD_RENDER_BACKEND',
    'RO_HEAL_PREVIEW_OUT','RO_HEAL_PREVIEW_ASSETS','RO_NPC_CLASS_CHANGE_PREVIEW')
$savedEnvironment = @{}
foreach ($name in $environmentNames) { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
$executableHash = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
$captures = @{}
$scenarioNames = @('blade-baseline','blade-stop','blade-restored')
$logPattern = Join-Path $stage 'debug_hp_*.log'
try {
    $env:OPEN_MIDGARD_DATA_DIR = $settings
    $env:OPEN_MIDGARD_RENDER_BACKEND = 'vulkan'
    $env:RO_HEAL_PREVIEW_ASSETS = $stage
    foreach ($scenario in $scenarioNames) {
        $capture = Join-Path $fixtureRoot ($scenario + '.bmp')
        if (Test-Path -LiteralPath $capture) { Remove-Item -LiteralPath $capture -Force }
        Get-ChildItem -Path $logPattern -ErrorAction SilentlyContinue | Remove-Item -Force
        $env:RO_HEAL_PREVIEW_OUT = $capture
        $env:RO_NPC_CLASS_CHANGE_PREVIEW = $scenario
        $started = [DateTime]::UtcNow
        $process = Start-Process -FilePath $executable -WorkingDirectory $stage -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(55000)) {
            Stop-Process -Id $process.Id -Force
            throw "Native blade-stop preview '$scenario' timed out."
        }
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $capture -PathType Leaf)) {
            throw "Native blade-stop preview '$scenario' failed (exit $($process.ExitCode)); inspect $stage\debug_hp_*.log"
        }
        if ((Get-Item -LiteralPath $capture).LastWriteTimeUtc -lt $started.AddSeconds(-1)) {
            throw "Native blade-stop preview '$scenario' did not produce a fresh capture."
        }
        $log = Get-ChildItem -Path $logPattern -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
        if (-not $log -or -not (Get-Content -LiteralPath $log.FullName -Raw).Contains('[NpcClassChangePreview] blade-stop-render=1')) {
            throw "Native blade-stop preview '$scenario' log is missing blade-stop-render=1."
        }
        $captures[$scenario] = Read-BmpPixels $capture
        $captures[$scenario].path = $capture
    }

    $baseline = $captures['blade-baseline']
    $stopped = $captures['blade-stop']
    $restored = $captures['blade-restored']
    $foreground = 0
    $tintedForeground = 0
    $changed = 0
    $darkened = 0
    $brightened = 0
    $alphaChanged = 0
    $backgroundChanged = 0
    $restoredChanged = 0
    $clearB = 0x20; $clearG = 0x20; $clearR = 0x20
    for ($i = 0; $i -lt $baseline.pixels.Length; $i += 4) {
        $isBackground = $baseline.pixels[$i] -eq $clearB -and $baseline.pixels[$i+1] -eq $clearG -and $baseline.pixels[$i+2] -eq $clearR
        $isTintBackground = $stopped.pixels[$i] -eq $clearB -and $stopped.pixels[$i+1] -eq $clearG -and $stopped.pixels[$i+2] -eq $clearR
        if (-not $isBackground) { ++$foreground }
        if (-not $isTintBackground) { ++$tintedForeground }
        $before = [int]$baseline.pixels[$i] + [int]$baseline.pixels[$i+1] + [int]$baseline.pixels[$i+2]
        $after = [int]$stopped.pixels[$i] + [int]$stopped.pixels[$i+1] + [int]$stopped.pixels[$i+2]
        if ($after -lt $before) { ++$darkened }
        if ($after -gt $before) { ++$brightened }
        if ($baseline.pixels[$i+3] -ne $stopped.pixels[$i+3]) { ++$alphaChanged }
        if ($baseline.pixels[$i] -ne $stopped.pixels[$i] -or
            $baseline.pixels[$i+1] -ne $stopped.pixels[$i+1] -or
            $baseline.pixels[$i+2] -ne $stopped.pixels[$i+2] -or
            $baseline.pixels[$i+3] -ne $stopped.pixels[$i+3]) {
            ++$changed
            if ($isBackground) { ++$backgroundChanged }
        }
        if ($baseline.pixels[$i] -ne $restored.pixels[$i] -or
            $baseline.pixels[$i+1] -ne $restored.pixels[$i+1] -or
            $baseline.pixels[$i+2] -ne $restored.pixels[$i+2] -or
            $baseline.pixels[$i+3] -ne $restored.pixels[$i+3]) { ++$restoredChanged }
    }
    if ($restoredChanged -ne 0) { throw "Blade-restored pixels are not identical to baseline ($restoredChanged changed pixels)." }
    if ($darkened -lt 24 -or $brightened -ne 0 -or $backgroundChanged -ne 0 -or $alphaChanged -ne 0) {
        throw "Blade-stop pixels failed: darkened=$darkened brightened=$brightened backgroundChanged=$backgroundChanged alphaChanged=$alphaChanged."
    }
    if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -ne $executableHash) {
        throw 'Fixture executable changed during capture.'
    }
    $result = @{outcome='PASS';backend='vulkan';scenarios=$scenarioNames;captureWidth=640;captureHeight=480;
        baseline=$baseline.path;bladeStop=$stopped.path;restored=$restored.path;
        baselineActorPixels=$foreground;bladeStopActorPixels=$tintedForeground;changedPixels=$changed;
        darkenedPixels=$darkened;backgroundChangedPixels=$backgroundChanged;alphaChangedPixels=$alphaChanged;
        executableSha256=$executableHash}
    $json = $result | ConvertTo-Json -Depth 4 -Compress
    $json | Set-Content -LiteralPath (Join-Path $fixtureRoot 'showefst-blade-stop-preview-results.json') -Encoding utf8
    Write-Output $json
} finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name,$savedEnvironment[$name],'Process')
    }
}
