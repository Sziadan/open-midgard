# Blocked skill-system items

Runtime-name entries require a producer port; they do not establish that an asset name is unrecoverable.
Source anchors already ported are listed separately in `generated/runtime_wave_callsites.tsv`; none is a live-verification claim.

<!-- generated:begin -->
None.
<!-- generated:end -->

## Manually reviewed dependencies

- WP10.3 `EffectTextureSet` resource arguments for effect 681, `Ref/RagEffect.cpp:12532–12535`: canonical PE symbols `aEffect_76`, `aEffect_7`, `aEffect_9`, and `aEffect_118` each resolve only to the byte string `effect\\` (`HighPriest.exe.c:33208–33214`); no texture filename is present. Do not guess names. Keep the handler unported until the exact resources are recovered.
- WP10.3 `EffectTextureSet` resource arguments for effect 700, `Ref/RagEffect.cpp:12607–12610`: canonical PE symbols `aEffect_127`, `aEffect_7`, `aEffect_42`, and `aEffect_25` each resolve only to the byte string `effect\\` (`HighPriest.exe.c:33208–33214`); no texture filename is present. Do not guess names. Keep the handler unported until the exact resources are recovered.
- WP10.4 effect 635 and 636 wave filenames: canonical PE symbols `aEffect_91` and `aEffect_40` resolve only to `effect\\` (`HighPriest.exe.c:33192–33193`). Their frame-zero audio cues and STR continuation are routed, preserving the available bytes; full wave names remain unresolved and must not be guessed.
- WP10.3 effects 269–272 (Strip Weapon/Shield/Armor/Helm) wave filename: canonical PE symbol `aEffect_70` at VA `0x6F1FF0` resolves only to `effect\\`; the state-6 cue is routed with the proven prefix and listener-relative coordinates, but the suffix is unresolved and must not be guessed.
- FireArrow's six CTexMgr sprite strings were already resolved from canonical executable bytes and checked in the FireArrow lifecycle checkpoint (`PARITY_LOG.md`, “FireArrow projectile / trail / impact”). Their addresses are `0x71B700`, `0x71B6EC`, `0x71B6D8`, `0x71B6C4`, `0x71B6B0`, and `0x71B69C`; no blocked dependency remains here.
