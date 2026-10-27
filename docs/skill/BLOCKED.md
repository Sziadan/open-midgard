# Blocked skill-system items

Runtime-name entries require a producer port; they do not establish that an asset name is unrecoverable.
Source anchors already ported are listed separately in `generated/runtime_wave_callsites.tsv`; none is a live-verification claim.

<!-- generated:begin -->
- PlayWave at 0x5D6A53 in CSoundMaker::OnProcess uses a runtime name; trace and port its producer (not an unrecoverable literal)
- PlayWave at 0x5D9E95 in CPc::OnProcess uses a runtime name; trace and port its producer (not an unrecoverable literal)
- PlayWave at 0x5E0FD6 in CGrannyPc::OnProcess uses a runtime name; trace and port its producer (not an unrecoverable literal)
- PlayWave at 0x5E48A1 in CPlayer::OnProcess uses a runtime name; trace and port its producer (not an unrecoverable literal)
- PlayWave at 0x5F963C in CRagEffect::FireArrow uses a runtime name; trace and port its producer (not an unrecoverable literal)
- PlayWave at 0x5FD22D in CRagEffect::EarthSpike uses a runtime name; trace and port its producer (not an unrecoverable literal)
- PlayWave at 0x606D01 in CRagEffect::FireHit uses a runtime name; trace and port its producer (not an unrecoverable literal)
- PlayWave at 0x60A8EE in CRagEffect::Kouenka uses a runtime name; trace and port its producer (not an unrecoverable literal)
- PlayWave at 0x642CAD in CEffectPrim::Prim3DParticleGravity uses a runtime name; trace and port its producer (not an unrecoverable literal)
<!-- generated:end -->

## Manually reviewed dependencies

- WP5 FireArrow (`Ref/RagEffect.cpp:3983-3988`, 0x5F95B0): `aEffect_149`, `aEffect_14`, `aEffect_77`, `aEffect_119`, `aEffect_131`, `aEffect_33` resolve through SymbolTable only to the truncated `effect\` prefix. The six CTexMgr string pushes still need call-site resolution before porting that branch. Do not infer paths from the `_N` suffixes.
