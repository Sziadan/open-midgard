# Ragnarok High Priest 2008 Client Rebuild — TODO

Progress: **33 / 145 modules** have src/ files (most are stubs)

---

## Renderer Backend Roadmap

Goal: ship Direct3D12 and Vulkan as real renderer options, selectable from the in-game option window with a safe automatic relaunch flow when the renderer changes.

### Milestone 0 — Backend Selection Foundation
- [x] D3D11 backend renders the world correctly.
- [x] D3D11 overlays/UI/cursor composition is stable.
- [x] Backend routing exists in the render-device layer.
- [x] Persist preferred renderer selection in client settings.
- [x] Show active vs pending renderer in the option window.

### Milestone 1 — Clean Restart / Relaunch Flow
- [x] Add relaunch request API in the app entry layer.
- [x] Preserve exe path / command line / working directory for relaunch.
- [x] On successful relaunch request, let the current client exit through normal cleanup.
- [x] Prompt from the option window to restart immediately after renderer changes.
- [x] Add in-game specific confirmation text for map-session renderer restarts.
- [x] Add a dedicated `Restart now` control when the selected backend differs from the active backend.

### Milestone 2 — Backend Abstraction Hardening
- [x] Extract backend-neutral modern-render helpers from the D3D11 implementation.
- [x] Standardize modern frame lifecycle: scene render, overlay compose, upload, present.
- [x] Normalize fixed-function state translation for modern backends.
- [x] Keep D3D11 behavior unchanged after refactor.

### Milestone 3 — Direct3D12 Bring-Up
- [x] Add `D3D12RenderDevice` shell to the routed render-device layer.
- [x] Implement DX12 device / queue / swapchain / descriptor heap setup.
- [x] Implement DX12 render-target and depth-buffer lifecycle.
- [x] Get DX12 clear / resize / present working.
- [x] Fall back cleanly to D3D11 when DX12 initialization fails.

### Milestone 4 — Direct3D12 Rendering Parity
- [x] Port transformed-vertex draw path to DX12.
- [x] Port texture creation / upload / binding to DX12.
- [x] Port alpha, blend, depth, and lightmap behavior to DX12.
- [x] Port overlay composition upload before present on DX12.
- [x] Validate login, char select, map load, world render, UI, and cursor on DX12.

### Milestone 5 — Direct3D12 Stability And Diagnostics
- [x] Add DX12 debug-layer support in development builds.
- [x] Add DX12-targeted logs for init, resize, and present failures.
- [x] Validate alt-tab, minimize/restore, resize, and return-to-char-select on DX12.
- [x] Make DX12 a user-facing selectable backend once stable.

### Milestone 6 — Renderer UI Expansion
- [x] Expand the renderer selector to show Direct3D7, Direct3D11, Direct3D12, and Vulkan.
- [x] Surface backend state per entry: active, selected, restart required, unsupported, not implemented.
- [x] Prevent unimplemented backends from behaving like real selections.
- [x] Keep fallback/active-backend state visible in the option window and title bar.

### Milestone 7 — Vulkan Bring-Up
- [x] Add `VulkanRenderDevice` shell to the routed render-device layer.
- [x] Implement Vulkan instance / device / surface / swapchain bring-up.
- [x] Implement Vulkan clear / resize / present path.
- [x] Fall back cleanly to D3D11 when Vulkan initialization fails.

### Milestone 8 — Vulkan Rendering Parity
- [x] Port transformed-vertex draw path to Vulkan.
- [x] Port texture creation / upload / binding to Vulkan.
- [x] Port alpha, blend, depth, and lightmap behavior to Vulkan.
- [x] Port overlay composition upload before present on Vulkan.
- [x] Validate login, char select, map load, world render, UI, and cursor on Vulkan.

### Milestone 9 — Cross-Backend Validation
- [ ] Validate startup, login, char select, map load, and in-game movement on all backends.
- [ ] Validate overlays, cursor, alt-tab, resize, and restart flow on all backends.
- [ ] Validate fallback behavior when a selected backend cannot initialize.

### Milestone 10 — Cleanup And Documentation
- [ ] Remove temporary Vulkan stabilization and frame-timing logs after the current implementation settles.
- [ ] Remove temporary backend bring-up logs that are no longer useful.
- [ ] Document backend selection precedence: env var override, persisted setting, default backend.
- [ ] Document backend fallback order and restart-required behavior.

### Milestone 11 — Modern Anti-Aliasing Roadmap

Goal: ship true configurable 3D anti-aliasing for modern backends without affecting the composed UI or software cursor paths.

#### Phase A — Finish The Current D3D11 FXAA Path
- [x] Replace the old placeholder AA setting with a real backend-driven option.
- [x] Implement D3D11 scene-to-offscreen rendering plus FXAA resolve before UI composition.
- [x] Hide the AA option on unsupported backends instead of exposing dead settings.
- [x] Validate D3D11 FXAA across login, char select, in-game world render, overlays, and restart flow.
- [ ] Check D3D11 FXAA image quality on GND edges, map-object silhouettes, foliage alpha edges, and animated effects.
- [ ] Remove any D3D11-specific temporary AA diagnostics once stable.

#### Phase B — Add True SMAA Option
- [x] Extend `AntiAliasingMode` to support both `FXAA` and `SMAA` as real selectable modes.
- [x] Add SMAA presets or a single production default and document the choice.
- [x] Add SMAA shader resources and generation flow for D3D11/D3D12/Vulkan.
- [x] Implement SMAA edge detection pass.
- [x] Implement SMAA blend-weight calculation pass.
- [x] Implement SMAA neighborhood blending pass.
- [ ] Ensure the SMAA passes operate only on the 3D scene target and never on the final UI/cursor composite.
- [ ] Expose SMAA in the option window only on backends where the full pass chain is implemented.

#### Phase C — Port The 3D AA Pipeline To D3D12
- [x] Add a DX12 scene color target separate from the swapchain back buffer.
- [x] Route DX12 world rendering into the scene target while keeping overlays/UI on the existing post-scene path.
- [x] Add a DX12 fullscreen post-process pass abstraction for FXAA/SMAA.
- [x] Port the current FXAA implementation to DX12.
- [ ] Hook the DX12 capture/snapshot path so overlays and UI compose over the AA-resolved scene, not the raw scene.
- [ ] Re-validate resize, alt-tab, return-to-char-select, and restart-required behavior on DX12 with AA enabled.
- [x] Only enable the option-window AA entry for DX12 after the resolved scene path is proven stable.

#### Phase D — Port The 3D AA Pipeline To Vulkan
- [x] Add a Vulkan scene color image separate from the swapchain image.
- [x] Route Vulkan world rendering into the scene target before overlay composition.
- [ ] Add Vulkan fullscreen post-process pipeline support for FXAA/SMAA.
- [x] Port the current FXAA implementation to Vulkan.
- [x] Integrate the resolved scene into the existing Vulkan overlay/UI upload path without reintroducing the prior UI scaling issues.
- [ ] Re-validate swapchain resize, present, alt-tab, cursor, and UI composition behavior with AA enabled on Vulkan.
- [x] Only enable the option-window AA entry for Vulkan after the resolved scene path is proven stable.

#### Phase E — Backend Capability And UX Cleanup
- [x] Split AA capability reporting into explicit per-backend mode support instead of a single boolean.
- [x] Keep unsupported AA modes hidden rather than greyed out in the option window.
- [x] Preserve restart-required behavior for AA modes that require backend reinitialization.
- [x] Ensure renderer switches clamp or clear unsupported AA modes when changing between backends.
- [x] Document which AA modes are supported by each backend and which render path they affect.

#### Phase F — Validation Matrix
- [ ] Validate `Off`, `FXAA`, and `SMAA` on each supported backend.
- [ ] Validate that UI, text, mouse cursor, and software-composed overlays are unchanged by 3D AA modes.
- [ ] Validate world geometry, models, sprites in 3D space, particles, lightmaps, and alpha-tested surfaces under each AA mode.
- [ ] Validate save/load/restart flow for AA settings from the option window.
- [ ] Capture comparison screenshots and short implementation notes for final cleanup/documentation.

---

## Packet Version Alignment Plan

Goal: align the client to one intentional packet profile instead of the current mixed old/new behavior, while keeping compatibility with `Ref/RunningServer` and preserving the now-improved actor visibility work.

### Phase A — Lock The Target Profile
- [x] Choose the exact intended packet profile to emulate for client-originating packets.
- [x] Record the decision in repo notes with the exact `packet_ver`, date window, and source references used.
- [x] Define the migration boundary explicitly:
	- [x] client-send packets must match the chosen profile
	- [x] client receive table must be validated against the server's actual outgoing packet family
	- [x] mixed-mode packet behavior is no longer acceptable except where explicitly documented

### Phase B — Build A Packet Matrix
- [x] Inventory every client-originating packet currently emitted by the rebuilt client.
- [x] Inventory the exact opcode, fixed length, and struct layout currently used for each send packet.
- [x] Build a comparison table against `Ref/RunningServer/packet_db.txt` for the chosen profile.
- [x] Split the matrix into three buckets:
	- [x] already correct
	- [x] wrong opcode or wrong length
	- [x] missing / not implemented / ambiguous
- [ ] Build a second matrix for server-to-client packets actually observed in logs.
- [ ] Mark which receive packets are governed by compiled eAthena actor-family behavior versus pure packet-db parsing.

### Phase C — Add Versioned Packet Definitions
- [x] Introduce one central place in the client that declares the intended packet profile.
- [x] Stop scattering raw packet-version assumptions across `LoginMode`, `GameMode`, and helper send functions.
- [x] Add named packet definitions for each migrated client-send packet instead of embedding magic numbers and ad hoc struct layouts.
- [x] Ensure packet structs are checked for exact size on MSVC/Win32.
- [x] Keep packet serialization explicit so field order and padding are visible and reviewable.

### Phase D — Migrate Handshake / Session Bootstrap First
- [x] Update zone enter / WantToConnection to the chosen profile.
- [x] Update any packet-version-sensitive login or map-server bootstrap packets that must match the same profile.
- [x] Confirm how the server infers packet version from the bootstrap packet family and layout.
- [x] Verify the server still accepts login, char select, and map entry with the migrated handshake.
- [x] Capture fresh logs proving what packet family the server now sends back after handshake.
- [x] Keep server-move reconnect on the same zone-enter packet family as initial map entry.

### Phase E — Migrate Core Gameplay Send Packets
- [x] Action request
- [x] Walk / move request
- [ ] Use skill on target
- [ ] Use item
- [x] Name request
- [x] Chat / broadcast packets that are version-sensitive
- [ ] Any actor-interaction packets used during normal play (attack, sit/stand, pickup, drop, equip, unequip, NPC interaction)
- [ ] Any remaining packets used during the current repro scenarios even if they are not yet fully implemented elsewhere

### Phase F — Reconcile The Receive Side
- [x] Re-validate `src/network/GronPacket.cpp` against the intended compatibility target after send-side migration.
- [ ] Keep the receive table aligned with what the server actually sends, not with assumptions about what it ought to send.
- [x] Add any still-missing receive packet sizes that can desync the stream on crowded maps.
- [ ] For each added receive packet, decide whether it needs:
	- [ ] full handler
	- [x] safe ignore handler
	- [ ] temporary trace-only handler
- [x] Re-test that no unknown or invalid variable-length receive packets appear during login, map load, idle standing, walking, and crowded scenes.

### Phase G — Remote Player Visibility Validation
- [ ] Add focused tracing for remote-player bootstrap/hydration using GID-based logs.
- [ ] Verify which packet first creates each remote player shell.
- [ ] Verify which packet supplies job/class, sex, head/body style, move state, and visibility state.
- [ ] Verify standing players are hydrated without requiring a later movement packet.
- [ ] Verify moving players do not get misclassified as monsters or partial shells.
- [ ] Reproduce with at least two remote players and repeated out-of-sight / in-sight transitions.

### Phase H — Guardrails And Cleanup
- [ ] Remove or consolidate temporary packet tracing once the migration is stable.
- [ ] Document every intentional protocol deviation that remains.
- [ ] Add assertions or helper checks so future packet edits cannot silently change struct sizes.
- [ ] Add comments only where the protocol choice would otherwise be easy to break later.
- [ ] Update repo memory / notes with the final chosen packet profile and known server behavior.

### Phase I — Build / Deploy / Verify Each Step
- [ ] After each meaningful migration step, build the client.
- [ ] After each successful build, deploy to `D:\Spel\OldRO\HighPriest.exe`.
- [ ] Keep one short verification checklist for every step:
	- [ ] can login
	- [ ] can reach char select
	- [ ] can enter map
	- [ ] can move
	- [ ] can see monsters
	- [ ] can see remote players standing still
	- [ ] can see remote players moving
- [ ] If a step breaks bootstrap, revert only that step and capture the exact packet mismatch before proceeding.

### Recommended Execution Order
- [ ] 1. Freeze target packet profile and write the packet matrix.
- [ ] 2. Introduce central versioned packet definitions.
- [ ] 3. Migrate handshake/bootstrap packets.
- [ ] 4. Confirm server acceptance and observe returned actor packet family.
- [ ] 5. Migrate core gameplay send packets.
- [ ] 6. Reconcile receive packet sizes/handlers.
- [ ] 7. Validate remote-player hydration thoroughly.
- [ ] 8. Remove tracing and document the final state.

---

## Phase 0 — Fix Build
- [x] Fix `Types.h` / `<windows.h>` include ordering (DWORD redefinition cascade)
- [x] Rebuild from current workspace path and get clean compilation
- [x] Verify zlib dependency links correctly

---

## Phase 1 — Core Infrastructure (already partly done)

### Core (`src/core/`)
- [x] Timer.h / Timer.cpp
- [x] Hash.h / Hash.cpp
- [x] GPak.h / GPak.cpp — GRF archive reader
- [x] File.h / File.cpp — CFile, CMemFile, CMemMapFile, CFileMgr
- [x] DllMgr.h / DllMgr.cpp
- [x] Globals.h / Globals.cpp
- [x] Locale.h / Locale.cpp
- [x] Xml.h / Xml.cpp
- [ ] Common.h / Common.cpp
- [ ] Util.h / Util.cpp
- [ ] RegMgr.h / RegMgr.cpp — Registry manager
- [ ] SimpleAssert.h / SimpleAssert.cpp
- [ ] ExceptionHandler.h / ExceptionHandler.cpp

### Cipher / Encryption (`src/cipher/`)
- [x] CDec.h / CDec.cpp — DES decryption
- [ ] Cipher.h / Cipher.cpp
- [ ] CipherImpl.h / CipherImpl.cpp
- [ ] md5.h / md5.cpp
- [ ] Padding.h / Padding.cpp

### Entry Point (`src/main/`)
- [x] WinMain.h / WinMain.cpp — Window init, registry, main loop

---

## Phase 2 — Rendering

### 2D Rendering (`src/render/`)
- [x] Renderer.h / Renderer.cpp — Texture management, render pipeline
- [x] DrawUtil.h / DrawUtil.cpp
- [x] DC.h / DC.cpp
- [x] Prim.h / Prim.cpp

### 3D Rendering (`src/render3d/`)
- [x] Device.h / Device.cpp — DirectDraw / Direct3D init
- [x] D3dutil.h / D3dutil.cpp
- [ ] 3dActor.h / 3dActor.cpp — 3D actor rendering
- [ ] 3dGround.h / 3dGround.cpp — 3D ground/terrain rendering
- [ ] View.h / View.cpp — Camera / viewport
- [ ] Skybox.h / Skybox.cpp
- [ ] mesh.h / mesh.cpp
- [ ] skin.h / skin.cpp
- [ ] Model.h / Model.cpp
- [ ] Picker.h / Picker.cpp — Mouse picking / raycasting

### Resources (`src/res/`)
- [x] Bitmap.h / Bitmap.cpp
- [x] ImfRes.h / ImfRes.cpp
- [x] PaletteRes.h / PaletteRes.cpp
- [x] Res.h / Res.cpp
- [x] Sprite.h / Sprite.cpp
- [x] Texture.h / Texture.cpp
- [x] Ijl.h / Ijl.cpp
- [ ] Anim.h / Anim.cpp — Animation data
- [ ] Action.h / Action.cpp — Action/motion data
- [ ] Attr.h / Attr.cpp — Ground attributes

### Effects
- [ ] RagEffect.h / RagEffect.cpp
- [ ] RagEffect2.h / RagEffect2.cpp
- [ ] RagEffectPrim.h / RagEffectPrim.cpp
- [ ] EZeffect.h / EZeffect.cpp

---

## Phase 3 — Networking & Packets

### Network (`src/network/`)
- [x] Connection.h / Connection.cpp — Winsock, packet queue, send/recv
- [x] Packet.h
- [x] PacketQueue.h / PacketQueue.cpp
- [x] GronPacket.h / GronPacket.cpp — Packet definitions
- [x] GameModePacket.h / GameModePacket.cpp — Game-mode packet handlers (enter/map, actor lifecycle, chat/system incl. broadcast/party/battlefield, color/category propagation, return-to-login)
- [ ] EncClient.h / EncClient.cpp — Packet encryption client
- [ ] CReAssemblyPacket.h / CReAssemblyPacket.cpp

### Security / Packet Encryption
- [ ] cSecureGamePack.h / cSecureGamePack.cpp
- [ ] cKeyProtector.h / cKeyProtector.cpp
- [ ] CSRCryptoR2Client.h / CSRCryptoR2Client.cpp
- [ ] CSRCryptoR2PacketConfiguration.h / CSRCryptoR2PacketConfiguration.cpp
- [ ] CSRPacketR2Client.h / CSRPacketR2Client.cpp
- [ ] CSRPacketR2Var.h / CSRPacketR2Var.cpp
- [ ] GenUserCode.h / GenUserCode.cpp
- [ ] FindHack.h / FindHack.cpp
- [ ] SequenceRandomGenerator.h / SequenceRandomGenerator.cpp

---

## Phase 4 — Lua Scripting VM

> **Option A:** Port all 26 Lua 5.0 source files from Ref/  
> **Option B:** Link against an external Lua 5.0 library (much simpler)

- [ ] lapi.h / lapi.cpp
- [ ] lauxlib.h / lauxlib.cpp
- [ ] lbaselib.h / lbaselib.cpp
- [ ] lcode.h / lcode.cpp
- [ ] ldblib.h / ldblib.cpp
- [ ] ldebug.h / ldebug.cpp
- [ ] ldo.h / ldo.cpp
- [ ] ldump.h / ldump.cpp
- [ ] lfunc.h / lfunc.cpp
- [ ] lgc.h / lgc.cpp
- [ ] liolib.h / liolib.cpp
- [ ] llex.h / llex.cpp
- [ ] lmathlib.h / lmathlib.cpp
- [ ] lmem.h / lmem.cpp
- [ ] loadlib.h / loadlib.cpp
- [ ] lobject.h / lobject.cpp
- [ ] lparser.h / lparser.cpp
- [ ] lstate.h / lstate.cpp
- [ ] lstring.h / lstring.cpp
- [ ] lstrlib.h / lstrlib.cpp
- [ ] ltable.h / ltable.cpp
- [ ] ltablib.h / ltablib.cpp
- [ ] ltm.h / ltm.cpp
- [ ] lundump.h / lundump.cpp
- [ ] lvm.h / lvm.cpp
- [ ] lzio.h / lzio.cpp
- [ ] LuaBridge.h / LuaBridge.cpp — **exists but stub**

---

## Phase 5 — Game World & Actors

### World (`src/world/`)
- [ ] GameWorld.h / GameWorld.cpp — Spatial world management
- [ ] Ground.h / Ground.cpp — Ground/map loading
- [ ] World.h / World.cpp — **exists but stub**

### Actors
- [ ] GameActor.h / GameActor.cpp — **exists but stub**
- [ ] GameActor3d.h / GameActor3d.cpp — 3D actor logic
- [ ] GameActorMsgHandler.h / GameActorMsgHandler.cpp
- [ ] Player.h / Player.cpp
- [ ] Pc.h / Pc.cpp — Player character
- [ ] Npc.h / Npc.cpp
- [ ] MercenaryAI.h / MercenaryAI.cpp
- [ ] Granny.h / Granny.cpp — **exists but stub**

### Session
- [ ] Session.h / Session.cpp — **exists but partial** (XML parsing works)
- [ ] Session2.h / Session2.cpp

---

## Phase 6 — Game Modes & Login

### Game Modes (`src/gamemode/`)
- [ ] Mode.h / Mode.cpp — **exists but partial** (loop structure only)
- [ ] LoginMode.h / LoginMode.cpp — **exists but stub**
- [ ] GameMode.h / GameMode.cpp — **exists but stub**
- [x] GameModePacket.h / GameModePacket.cpp
- [ ] GameMode2.h / GameMode2.cpp

### High Priest Mini-Games
- [ ] HP_Battle.h / HP_Battle.cpp
- [ ] HP_BuySell.h / HP_BuySell.cpp
- [ ] HP_RestTravell.h / HP_RestTravell.cpp
- [ ] HP_SaveLoad.h / HP_SaveLoad.cpp
- [ ] HP_Tetris.h / HP_Tetris.cpp

---

## Phase 7 — UI System

### UI Base Classes
- [ ] UISys.h / UISys.cpp — Core UI system
- [ ] UIControl.h / UIControl.cpp — Base control class
- [ ] UIControl2.h / UIControl2.cpp
- [ ] UIRectInfo.h / UIRectInfo.cpp
- [ ] Control.h / Control.cpp
- [ ] UIWindowMgr.h / UIWindowMgr.cpp — **exists but partial** (chat metadata sink + in-memory chat log + preview feed)
- [ ] UIWindow.h / UIWindow.cpp — **exists but stub**

### UI Windows
- [ ] UIFrameWnd.h / UIFrameWnd.cpp — **exists but stub**
- [ ] UIFrameWnd2.h / UIFrameWnd2.cpp
- [ ] UIFrameWnd3.h / UIFrameWnd3.cpp
- [ ] UIWaitWnd.h / UIWaitWnd.cpp — **exists but stub**
- [ ] UIItemWnd.h / UIItemWnd.cpp
- [ ] UIGuildWnd.h / UIGuildWnd.cpp
- [ ] UIMessengerWnd.h / UIMessengerWnd.cpp
- [ ] UIGronMessengerWnd.h / UIGronMessengerWnd.cpp
- [ ] UIEmotionWnd.h / UIEmotionWnd.cpp
- [ ] UIEmotionListWnd.h / UIEmotionListWnd.cpp
- [ ] UIIllustWnd.h / UIIllustWnd.cpp
- [ ] UIImeWnd.h / UIImeWnd.cpp
- [ ] UIMetalWorkWnd.h / UIMetalWorkWnd.cpp

---

## Phase 8 — Game Data & Content

### Skills & Items
- [ ] Skill.h / Skill.cpp — **exists but stub**
- [ ] SkillInfo.h / SkillInfo.cpp
- [ ] Item.h / Item.cpp — **exists but stub**
- [ ] ItemInfo.h / ItemInfo.cpp
- [ ] GuildInfo.h / GuildInfo.cpp
- [ ] Emblem.h / Emblem.cpp

### Messages & Strings
- [ ] MsgStrings.h / MsgStrings.cpp
- [ ] Insult.h / Insult.cpp — Chat filter
- [ ] TipOfTheDay.h / TipOfTheDay.cpp
- [ ] SnapMgr.h / SnapMgr.cpp

---

## Phase 9 — Audio & Input

### Audio (`src/audio/`)
- [ ] Audio.h / Audio.cpp — **exists but partial**
- [ ] Video.h / Video.cpp — **exists in src**
- [ ] Sound.h / Sound.cpp
- [ ] Wave.h / Wave.cpp
- [ ] CBink.h / CBink.cpp — Bink video playback

### Input (`src/input/`)
- [ ] Input.h / Input.cpp — **exists but stub**

---

## Phase 10 — Localization

- [ ] Language.h / Language.cpp
- [ ] LanguageKeyProcess.h / LanguageKeyProcess.cpp
- [ ] ftwbrk.h / ftwbrk.cpp — Word-break for Thai/CJK
- [ ] VietnamLanguage.h / VietnamLanguage.cpp
- [ ] VniInputMode.h / VniInputMode.cpp
- [ ] telexInputMode.h / telexInputMode.cpp

---

## Phase 11 — Security (`src/security/`)
- [ ] Security.h / Security.cpp — **exists but stub**
- [ ] PathFinder.h / PathFinder.cpp — **exists in src**

---

## Summary

| Phase | Description | Files Done | Files Total | % |
|-------|-------------|-----------|-------------|---|
| 0 | Fix Build | 0 | 3 | 0% |
| 1 | Core Infrastructure | 9 | 22 | 41% |
| 2 | Rendering | 12 | 26 | 46% |
| 3 | Networking & Packets | 3 | 12 | 25% |
| 4 | Lua VM | 0 | 27 | 0% |
| 5 | World & Actors | 0 | 12 | 0% |
| 6 | Game Modes & Login | 0 | 9 | 0% |
| 7 | UI System | 0 | 18 | 0% |
| 8 | Game Data & Content | 0 | 11 | 0% |
| 9 | Audio & Input | 0 | 6 | 0% |
| 10 | Localization | 0 | 6 | 0% |
| 11 | Security | 0 | 2 | 0% |
| **Total** | | **~24** | **~154** | **~16%** |
