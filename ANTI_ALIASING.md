# Modern Anti-Aliasing Support

This client currently applies 3D anti-aliasing only on the modern renderer backends. The final UI, text, software cursor, and software-composited overlays are not part of the AA resolve path.

## Supported Modes By Backend

| Backend | Supported modes | Notes |
| --- | --- | --- |
| Direct3D7 | Off only | No modern 3D AA path. The option window hides the anti-aliasing row for this backend. |
| Direct3D11 | Off, FXAA | The world renders into a scene target, FXAA resolves that scene, then overlays/UI compose on top. |
| Direct3D12 | Off, FXAA | Matches the D3D11 scene-target-plus-resolve flow before overlay/UI composition. |
| Vulkan | Off, FXAA | Uses a dedicated scene image and fullscreen post-process resolve before the overlay/UI pass. |

## Behavior Notes

- Anti-aliasing support is backend-specific. Unsupported modes are hidden in the option window instead of being shown as disabled entries.
- Switching to a backend that does not support the selected AA mode clamps the saved mode back to Off.
- Changing the AA mode is restart-required. `GraphicsSettingsRequireRestart(...)` treats AA changes the same way as other renderer reinitialization settings.
- `SMAA` now exists in the AA mode model and settings serialization, but it is not exposed as a supported backend option until the full pass chain is implemented.
- `FXAA` remains the only production AA mode available on the modern backends.

## SMAA Default

When SMAA ships, it will use a single production preset: `SMAA 1x High`.

- `1x` avoids temporal history, motion-vector requirements, and extra resolve state that this client does not currently carry through its classic frame flow.
- `High` is the default because it improves edge quality over FXAA without introducing an extra user-facing quality matrix before the full SMAA path is proven stable.
- No separate SMAA quality selector will be exposed unless later validation shows a concrete need for lower or higher-cost variants.

## Shader Assets

- The checked-in SMAA shader set now covers the edge-detection, blend-weight, and neighborhood-blend passes.
- D3D11 and D3D12 source for those passes lives in the shared post-process HLSL embedded in [src/render3d/RenderDevice.cpp](d:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/render3d/RenderDevice.cpp).
- Vulkan source for those passes lives in [src/render3d/shaders/vulkan_post_smaa.hlsl](d:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/render3d/shaders/vulkan_post_smaa.hlsl).
- The Vulkan generated header for those shaders is emitted by [tools/update_vulkan_smaa_shaders.ps1](d:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/tools/update_vulkan_smaa_shaders.ps1).
- D3D11 and D3D12 now wire the full internal SMAA chain against their offscreen 3D scene targets: edge detection, blend-weight calculation, and neighborhood blending all run before the existing UI/overlay composition path.
- SMAA remains hidden from the option window until the same full chain exists on the other supported modern backends and validation is complete.

## Render Path Scope

The AA pass is intended to affect only the 3D scene content:

- world geometry
- 3D models and sprites placed in the scene
- particles, lightmaps, and alpha-tested scene surfaces

The AA pass is not intended to affect:

- option-window and in-game UI text
- the software cursor path
- software-composited overlays after the scene resolve