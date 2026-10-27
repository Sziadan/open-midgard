# Blocked skill-system items

Runtime-name entries require a producer port; they do not establish that an asset name is unrecoverable.
Source anchors already ported are listed separately in `generated/runtime_wave_callsites.tsv`; none is a live-verification claim.

<!-- generated:begin -->
- PlayWave at 0x555B8E in CRenderObject::ProcessSound uses a runtime name; trace and port its producer (not an unrecoverable literal)
- PlayWave at 0x555CCF in CRenderObject::ProcessSound uses a runtime name; trace and port its producer (not an unrecoverable literal)
- PlayWave at 0x555E5F in CRenderObject::ProcessSound uses a runtime name; trace and port its producer (not an unrecoverable literal)
- PlayWave at 0x57E27C in CGameMode::ProcessDamageSituation uses a runtime name; trace and port its producer (not an unrecoverable literal)
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

## Open function ports

- EF_STATUS_STATE (164), canonical `CRagEffect::StatusState` at `HighPriest.exe.c:429596-429694`: the effect HUD branch depends on the canonical `CSession::GetStateFileName` map for 186 (priority,status) pairs. A mechanical audit found every canonical pair in readable `Ref/dhxj/Session2.cpp:4080+`, whose direct literal paths could support a crosswalk, but the canonical symbol-to-literal equivalence has not been directly verified. The readable variant has a `GetBufIcon` override that is absent from the canonical function and must not be ported as canonical behavior. The 24-entry Qt catalog is incomplete. Do not port only the reset/notification body or substitute the partial Qt catalog. Resolve the canonical symbol strings or mechanically establish the crosswalk before implementation.
