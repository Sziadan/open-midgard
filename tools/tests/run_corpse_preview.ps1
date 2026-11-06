param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '../../build-mingw-preset'),
    [string]$FixtureDirectory = (Join-Path $PSScriptRoot '../../build-mingw-preset/corpse-native-proof')
)
$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$buildRoot = [IO.Path]::GetFullPath($BuildDirectory)
$fixtureRoot = [IO.Path]::GetFullPath($FixtureDirectory)
$executable = Join-Path $buildRoot 'open-midgard.exe'
if (-not (Test-Path -LiteralPath $executable)) { throw 'Build open-midgard.exe before running this fixture.' }
if (-not (Select-String -LiteralPath (Join-Path $buildRoot 'CMakeCache.txt') -SimpleMatch 'RO_ENABLE_DEV_DEPLOY:BOOL=OFF' -Quiet)) {
    throw 'This fixture requires a deployment-disabled build.'
}
$executableHash = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
$referenceExe = Join-Path $sourceRoot 'HighPriest.exe'
if ((Test-Path -LiteralPath $referenceExe) -and $executableHash -eq (Get-FileHash -LiteralPath $referenceExe -Algorithm SHA256).Hash) {
    throw 'The reference executable must never be launched.'
}
$stage = Join-Path $fixtureRoot 'stage'
$sprites = Join-Path $stage 'data/sprite'
$settings = Join-Path $fixtureRoot 'settings'
New-Item -ItemType Directory -Force -Path $sprites,$settings | Out-Null
foreach ($extension in @('act','spr')) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot "Ref/GRF-Content/data/sprite/ork_warrior.$extension") `
        -Destination (Join-Path $sprites "ork_warrior.$extension")
}
$environmentNames = @('OPEN_MIDGARD_DATA_DIR','OPEN_MIDGARD_RENDER_BACKEND','RO_HEAL_PREVIEW_ASSETS',
    'RO_HEAL_PREVIEW_OUT','RO_HEAL_PREVIEW_CORPSE','RO_HEAL_PREVIEW_CORPSE_AGE','RO_HEAL_PREVIEW_CORPSE_TRANSFORM')
$savedEnvironment = @{}
foreach ($name in $environmentNames) { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
$runs = @()
try {
    $env:OPEN_MIDGARD_DATA_DIR = $settings
    $env:OPEN_MIDGARD_RENDER_BACKEND = 'vulkan'
    $env:RO_HEAL_PREVIEW_ASSETS = $stage
    $env:RO_HEAL_PREVIEW_CORPSE = '1'
    foreach ($scene in @('0','3060','6000','6119','6120','transform')) {
        $env:RO_HEAL_PREVIEW_CORPSE_AGE = if ($scene -eq 'transform') { '0' } else { $scene }
        $env:RO_HEAL_PREVIEW_CORPSE_TRANSFORM = if ($scene -eq 'transform') { '1' } else { $null }
        $env:RO_HEAL_PREVIEW_OUT = Join-Path $fixtureRoot "corpse-$scene.bmp"
        $started = [DateTime]::UtcNow
        $process = Start-Process -FilePath $executable -ArgumentList '/pc' -WorkingDirectory $stage -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(45000)) {
            Stop-Process -Id $process.Id
            throw 'The owned corpse fixture timed out.'
        }
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $env:RO_HEAL_PREVIEW_OUT)) {
            throw "Native corpse capture failed: $scene"
        }
        if ((Get-Item -LiteralPath $env:RO_HEAL_PREVIEW_OUT).LastWriteTimeUtc -lt $started.AddSeconds(-1)) {
            throw 'The fixture did not produce a fresh capture.'
        }
        $runs += @{scene=$scene;pid=$process.Id;exit=$process.ExitCode;
            startedUtc=$started.ToString('o');executableSha256=$executableHash}
        Write-Output "PASS native corpse $scene"
    }
    if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -ne $executableHash) {
        throw 'The fixture executable changed during capture.'
    }
    $runs | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $fixtureRoot 'run-manifest.json') -Encoding utf8
    & python (Join-Path $PSScriptRoot 'check_corpse_render_evidence.py') --capture-dir $fixtureRoot --output (Join-Path $fixtureRoot 'results.json')
    if ($LASTEXITCODE -ne 0) { throw 'Native corpse evidence validation failed.' }
} finally {
    foreach ($name in $environmentNames) { [Environment]::SetEnvironmentVariable($name,$savedEnvironment[$name],'Process') }
}
