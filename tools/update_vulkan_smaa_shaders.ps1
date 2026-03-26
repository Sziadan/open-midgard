$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$shaderFile = Join-Path $repoRoot 'src/render3d/shaders/vulkan_post_smaa.hlsl'
$outputHeader = Join-Path $repoRoot 'src/render3d/VulkanSmaaShaders.generated.h'
$buildDir = Join-Path $repoRoot 'build'
$vertexOutput = Join-Path $buildDir 'vulkan_post_smaa_dxc.vert.spv'
$edgeOutput = Join-Path $buildDir 'vulkan_post_smaa_edge_dxc.frag.spv'

$dxc = (Get-Command dxc -ErrorAction Stop).Source

function Compile-Spirv {
    param(
        [string]$EntryPoint,
        [string]$Target,
        [string]$OutputPath
    )

    & $dxc '-spirv' '-fspv-target-env=vulkan1.0' '-fvk-use-dx-layout' '-T' $Target '-E' $EntryPoint '-Fo' $OutputPath $shaderFile | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "dxc failed for $EntryPoint"
    }
}

function Convert-ToByteArrayText {
    param(
        [string]$Name,
        [byte[]]$Bytes
    )

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("static const uint8_t ${Name}[] = {")
    for ($i = 0; $i -lt $Bytes.Length; $i += 12) {
        $count = [Math]::Min(12, $Bytes.Length - $i)
        $slice = $Bytes[$i..($i + $count - 1)]
        $formatted = $slice | ForEach-Object { ('0x{0:X2}' -f $_) }
        $lines.Add('    ' + ($formatted -join ', ') + ',')
    }
    $lines.Add('};')
    $lines.Add("static const size_t ${Name}Size = sizeof(${Name});")
    return $lines
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Compile-Spirv -EntryPoint 'VSMainPost' -Target 'vs_6_0' -OutputPath $vertexOutput
Compile-Spirv -EntryPoint 'PSMainSMAAEdge' -Target 'ps_6_0' -OutputPath $edgeOutput

$headerLines = New-Object System.Collections.Generic.List[string]
$headerLines.Add('#pragma once')
$headerLines.Add('')
$headerLines.Add('#include <cstddef>')
$headerLines.Add('#include <cstdint>')
$headerLines.Add('')
$vertexLines = Convert-ToByteArrayText -Name 'kVulkanPostSmaaVsSpirv' -Bytes ([System.IO.File]::ReadAllBytes($vertexOutput))
foreach ($line in $vertexLines) {
    $headerLines.Add([string]$line)
}
$headerLines.Add('')
$edgeLines = Convert-ToByteArrayText -Name 'kVulkanPostSmaaEdgePsSpirv' -Bytes ([System.IO.File]::ReadAllBytes($edgeOutput))
foreach ($line in $edgeLines) {
    $headerLines.Add([string]$line)
}

[System.IO.File]::WriteAllLines($outputHeader, $headerLines)