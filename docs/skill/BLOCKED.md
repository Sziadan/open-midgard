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
- WP10.3 effect 361: PE/Ref OnProcess2 case0x169 plays aEffect_93 at state20; that symbol at HighPriest.exe.c:33108 resolves only to effect\. Exact cue suffix remains unresolved; do not guess.
- WP10.3 effect 365: PE/Ref OnProcess2 case0x16D plays aEffect_86 at state3; that symbol at HighPriest.exe.c:33107 resolves only to effect\. Pressure and GroundShake texture resources are resolved and routed; the handler preserves the known `effect\` audio prefix. The exact cue suffix remains unresolved; do not guess.
- WP5F effects 462/463: PE inline handlers emit 30 `PeongUp` primitives and play `aEffect_3`; effect463 also forces master state/animation. `aEffect_3` at `HighPriest.exe.c:29019` contains only `effect\\`, so the exact wave suffix cannot be recovered. Keep both handlers unported until the complete wave names are resolved; do not guess them.
- WP5F effect 665 `Bubble_Drop`: PE/Ref handler `HighPriest.exe.c:465331–465365` / `Ref/RagEffect2.cpp:24389–24427` uses `aEffect_139`, whose PE definition at `HighPriest.exe.c:34342` contains only `effect\\`. Keep the handler unported until the texture filename is recovered.
- WP5F effects 687/688 `Map_MagicZone`: PE and primary Ref caller sequences agree, but all six texture symbols (`aEffect_157`, `_129`, `_115`, `_92`, `_45`, `_60`) resolve only to `effect\\` at `HighPriest.exe.c:33215–33220`. Keep both handlers unported until the exact texture names are recovered.
