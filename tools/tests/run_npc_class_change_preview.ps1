param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '../../build-mingw-preset'),
    [string]$FixtureDirectory = (Join-Path $PSScriptRoot '../../build-mingw-preset/npc-class-change-preview'),
    [string]$Gr2StageDirectory = (Join-Path $BuildDirectory 'review-2026-11-05/gr2-corpse-native/stage'),
    [string]$OrkAssetsDirectory = (Join-Path $BuildDirectory 'review-2026-11-05/corpse-native/stage/data/sprite'),
    [string]$PetAssetsDirectory = (Join-Path $BuildDirectory 'packet-pet-window-renders/pet-window-assets')
)
$ErrorActionPreference = 'Stop'
$buildRoot = [IO.Path]::GetFullPath($BuildDirectory)
$fixtureRoot = Join-Path ([IO.Path]::GetFullPath($FixtureDirectory)) ([guid]::NewGuid().ToString('N'))
$gr2StageRoot = [IO.Path]::GetFullPath($Gr2StageDirectory)
$petAssetsRoot = [IO.Path]::GetFullPath($PetAssetsDirectory)
$executable = Join-Path $buildRoot 'open-midgard.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Build open-midgard.exe before running this fixture: $executable"
}

$stage = Join-Path $fixtureRoot 'working'
$settings = Join-Path $fixtureRoot 'settings'
$modelSource = Join-Path $gr2StageRoot 'data/model'
$spriteSource = [IO.Path]::GetFullPath($OrkAssetsDirectory)
$modelDestination = Join-Path $stage 'data/model'
$monsterDestination = Join-Path $stage 'data/sprite/monster'
if (-not (Test-Path -LiteralPath $modelSource -PathType Container)) {
    throw "GR2 model source directory not found: $modelSource"
}
if (-not (Test-Path -LiteralPath $spriteSource -PathType Container)) {
    throw "Ork Warrior sprite source directory not found: $spriteSource"
}
if (-not (Test-Path -LiteralPath $petAssetsRoot -PathType Container)) {
    throw "Pet asset source directory not found: $petAssetsRoot"
}

New-Item -ItemType Directory -Force -Path $monsterDestination,$settings,
    (Join-Path $modelDestination '3dmob'),(Join-Path $modelDestination '3dmob_bone') | Out-Null
# The treasure box model is deliberately absent to exercise a failed GR2 replacement.
foreach ($model in @('aguardian90_8.gr2','kguardian90_7.gr2')) {
    Copy-Item -LiteralPath (Join-Path $modelSource "3dmob/$model") -Destination (Join-Path $modelDestination '3dmob')
}
Get-ChildItem -LiteralPath (Join-Path $modelSource '3dmob_bone') -File | Where-Object Name -Match '^[78]_' |
    Copy-Item -Destination (Join-Path $modelDestination '3dmob_bone')

foreach ($extension in @('.act','.spr')) {
    $source = Join-Path $spriteSource ('ork_warrior' + $extension)
    Copy-Item -LiteralPath $source -Destination (Join-Path $monsterDestination ('ORK_WARRIOR' + $extension)) -Force
}

# Locate the legacy Korean-named sprite folder without depending on its codepage spelling.
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

function Get-BmpInfo([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 54 -or $bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4D) {
        throw "Capture is not a Windows BMP: $Path"
    }
    $width = [BitConverter]::ToInt32($bytes,18)
    $height = [Math]::Abs([BitConverter]::ToInt32($bytes,22))
    $bitsPerPixel = [BitConverter]::ToUInt16($bytes,28)
    $compression = [BitConverter]::ToUInt32($bytes,30)
    if ($width -le 0 -or $height -le 0 -or $bitsPerPixel -ne 32 -or $compression -ne 0) {
        throw "Expected a nonempty uncompressed 32-bit BMP capture: $Path"
    }
    return @{width=$width;height=$height}
}

$environmentNames = @('OPEN_MIDGARD_DATA_DIR','OPEN_MIDGARD_RENDER_BACKEND',
    'RO_HEAL_PREVIEW_OUT','RO_HEAL_PREVIEW_ASSETS','RO_NPC_CLASS_CHANGE_PREVIEW')
$savedEnvironment = @{}
foreach ($name in $environmentNames) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$capture = Join-Path $fixtureRoot 'npc-class-change.bmp'
$logPattern = Join-Path $stage 'debug_hp_*.log'
if (Test-Path -LiteralPath $capture) { Remove-Item -LiteralPath $capture -Force }
Get-ChildItem -Path $logPattern -ErrorAction SilentlyContinue | Remove-Item -Force
$executableHash = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
try {
    $env:OPEN_MIDGARD_DATA_DIR = $settings
    $env:OPEN_MIDGARD_RENDER_BACKEND = 'vulkan'
    $env:RO_HEAL_PREVIEW_ASSETS = $stage
    $env:RO_HEAL_PREVIEW_OUT = $capture
    $env:RO_NPC_CLASS_CHANGE_PREVIEW = '1'
    $started = [DateTime]::UtcNow
    $process = Start-Process -FilePath $executable -WorkingDirectory $stage -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(55000)) {
        Stop-Process -Id $process.Id -Force
        throw 'Native NPC class-change preview timed out.'
    }
    if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $capture -PathType Leaf)) {
        throw "Native NPC class-change preview failed (exit $($process.ExitCode)); inspect $stage\debug_hp_*.log"
    }
    if ((Get-Item -LiteralPath $capture).LastWriteTimeUtc -lt $started.AddSeconds(-1)) {
        throw 'Native NPC class-change preview did not produce a fresh capture.'
    }
    $bmp = Get-BmpInfo $capture
    $log = Get-ChildItem -Path $logPattern -ErrorAction SilentlyContinue | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if (-not $log) { throw 'Native NPC class-change preview produced no debug log.' }
    $logText = Get-Content -LiteralPath $log.FullName -Raw
    $requiredMarkers = @(
        '[NpcClassChangePreview] initial-sprite=1',
        '[NpcClassChangePreview] sprite-cache-cleared=1',
        '[NpcClassChangePreview] replacement-sprite=1',
        '[NpcClassChangePreview] missing-sprite=1',
        '[NpcClassChangePreview] hidden-hotspot=1',
        '[NpcClassChangePreview] first-gr2=1',
        '[NpcClassChangePreview] same-gr2=1',
        '[NpcClassChangePreview] old-gr2-cleared=1',
        '[NpcClassChangePreview] replacement-gr2=1',
        '[NpcClassChangePreview] missing-gr2=1',
        '[NpcClassChangePreview] gr2-to-sprite-cleared=1',
        '[NpcClassChangePreview] restored-sprite=1',
        '[NpcClassChangePreview] despawn=1',
        '[NpcClassChangePreview] map-exit=1',
        '[NpcClassChangePreview] sequence=1'
    )
    foreach ($marker in $requiredMarkers) {
        if (-not $logText.Contains($marker)) { throw "Native NPC class-change preview log missing marker: $marker (see $($log.FullName))" }
    }
    if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -ne $executableHash) {
        throw 'Fixture executable changed during capture.'
    }
    $result = @{outcome='PASS';backend='vulkan';workingDirectory=$stage;settingsDirectory=$settings;
        capture=$capture;captureWidth=$bmp.width;captureHeight=$bmp.height;log=$log.FullName;
        executableSha256=$executableHash;startedUtc=$started.ToString('o');pid=$process.Id;exit=$process.ExitCode}
    $json = $result | ConvertTo-Json -Depth 4 -Compress
    $json | Set-Content -LiteralPath (Join-Path $fixtureRoot 'npc-class-change-preview-results.json') -Encoding utf8
    Write-Output $json
} finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name,$savedEnvironment[$name],'Process')
    }
}
