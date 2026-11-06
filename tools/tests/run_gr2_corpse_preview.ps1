param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '../../build-mingw-preset'),
    [string]$FixtureDirectory = (Join-Path $PSScriptRoot '../../build-mingw-preset/gr2-corpse-native-proof')
)
$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$buildRoot = [IO.Path]::GetFullPath($BuildDirectory)
$fixtureRoot = [IO.Path]::GetFullPath($FixtureDirectory)
$executable = Join-Path $buildRoot 'packet_stream_replay_checks.exe'
if (-not (Test-Path -LiteralPath $executable)) { throw 'Build the production packet replay target first.' }
if (-not (Select-String -LiteralPath (Join-Path $buildRoot 'CMakeCache.txt') -SimpleMatch 'RO_ENABLE_DEV_DEPLOY:BOOL=OFF' -Quiet)) {
    throw 'Native proof requires deployment OFF.'
}
$stage = Join-Path $fixtureRoot 'stage'
$settings = Join-Path $fixtureRoot 'settings'
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'data/model'),$settings | Out-Null
foreach ($folder in @('3dmob','3dmob_bone')) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot "Ref/GRF-Content/data/model/$folder") `
        -Destination (Join-Path $stage 'data/model') -Recurse -Force
}
$hash = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
$names = @('OPEN_MIDGARD_DATA_DIR','OPEN_MIDGARD_RENDER_BACKEND','RO_HEAL_PREVIEW_OUT','RO_GR2_CORPSE_COUNT','RO_GR2_CORPSE_JOB','RO_GR2_CORPSE_AGE','RO_GR2_CORPSE_TRACE')
$saved = @{}
foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
$cases = @()
foreach ($count in @(1,356,357,500,600,601,602)) { $cases += @{job=1286;count=$count;age=5000} }
foreach ($job in @(1285,1287,1324,1288,722,1830)) { $cases += @{job=$job;count=1;age=5000} }
foreach ($age in @(0,250,60000)) { $cases += @{job=1286;count=1;age=$age} }
$runs = @()
try {
    $env:OPEN_MIDGARD_DATA_DIR = $settings
    $env:OPEN_MIDGARD_RENDER_BACKEND = 'vulkan'
    $env:RO_GR2_CORPSE_TRACE = '1'
    foreach ($case in $cases) {
        $scene = "$($case.job)-$($case.count)"
        if ($case.age -ne 5000) { $scene += "-age$($case.age)" }
        $env:RO_GR2_CORPSE_AGE = [string]$case.age
        $env:RO_HEAL_PREVIEW_OUT = Join-Path $fixtureRoot "$scene.bmp"
        $env:RO_GR2_CORPSE_COUNT = [string]$case.count
        $env:RO_GR2_CORPSE_JOB = [string]$case.job
        $started = [DateTime]::UtcNow
        $stdout = Join-Path $fixtureRoot "$scene.stdout"
        $stderr = Join-Path $fixtureRoot "$scene.stderr"
        $process = Start-Process -FilePath $executable -ArgumentList '--corpse-gr2-native' `
            -WorkingDirectory $stage -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        if (-not $process.WaitForExit(45000)) {
            Stop-Process -Id $process.Id
            throw "Owned model fixture timed out: $scene"
        }
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $env:RO_HEAL_PREVIEW_OUT)) {
            throw "Model fixture failed: $scene; see $stderr"
        }
        if ((Get-Item -LiteralPath $env:RO_HEAL_PREVIEW_OUT).LastWriteTimeUtc -lt $started.AddSeconds(-1)) {
            throw "Model fixture capture is stale: $scene"
        }
        $runs += @{job=$case.job;count=$case.count;ageMilliseconds=$case.age;scene=$scene;pid=$process.Id;exit=$process.ExitCode;startedUtc=$started.ToString('o');executableSha256=$hash}
        Write-Output "PASS native model corpse $scene"
    }
    if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -ne $hash) { throw 'Fixture binary changed during capture.' }
    $runs | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $fixtureRoot 'run-manifest.json') -Encoding utf8
    & python (Join-Path $PSScriptRoot 'check_gr2_corpse_evidence.py') --capture-dir $fixtureRoot --output (Join-Path $fixtureRoot 'results.json')
    if ($LASTEXITCODE -ne 0) { throw 'Native model corpse evidence validation failed.' }
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
}
