# Owned pet actor menu and mode26 dispatch

Root implementation/review, 2026-11-10, baseline `1e42eca1`. This continues the existing PetInfo and alternate-summary owners. PK27, PK09.21, PK09.04 and the whole packet plan remain open.

## Producer and source ownership

Primary `CGameMode::MakeMenu`576440 (Ref/GameMode.cpp12008+, pet branch12219–12260), called from ProcessInput, builds the owned pet actor menu on release. It resolves the actor through the world, checks the session pet GID and actual actor head marker100, and uses the preceding job/pick-class/storage gate. Ref/GameActor.cpp5256+ derives pick class4 for mercenary/homunculus jobs or object type9; the corresponding port predicate is job45..3999, job6001..6047, or object type9. Normal right-button release now tries this pet producer before the ordinary player context path. Dragging does not open the context menu; Shift suppresses the actor-menu path. Modal input ownership is also preserved in the port's asynchronous UI.

The producer replaces source menu18, writes selectors `[33,29,30,32,31]`, sets the picked actor GID as menu target and places the popup at mouse+1 without clamping it back into the client. The existing port UIChatRoomMenuWnd is the source shared menu18 equivalent; its mode-caller path is reused. Labels are MsgStr596/592/593/595/594: Info, Feeding, Performance, Accessory Off, Return Egg. Selected-PE static disassembly corroborates the head marker comparison at5767E6 and the exact paired selector stores/message pushes at576855–5769B3; the bounded disassembly is retained. There is no new generic player menu or competing pet popup owner.

Primary UIMenuWnd constructor48A710, SendMsg48B0D0 and OnLBtnUp48A810 (Ref/UIFrameWnd2.cpp2092+,11366+,25643+) establish this popup. Each label is inserted twice into7-pixel source rows, producing one14-pixel logical action. An outside release leaves it open. A valid action dispatches message26 with its logical row index and deletes the popup. Secondary CEGUI Rolemenu code is not authority for this native popup; its outside-click behavior must not replace the primary implementation.

## Recipients, confirmations and local effects

Primary mode26 (Ref/GameMode2.cpp3590+,3844–3883) deletes menu18, checks storage and then indexes current mode-owned selectors. Production preserves current vector semantics, existing whisper selectors11/21 and bounded-index safety. Selector29 confirms MsgStr601 and sends action1 only on acceptance;30 sends2;32 sends4 (Accessory Off);31 sends3 then clears the exact remembered egg's damaged flag, pet job/index and actual PetInfo owner;33 sends0 before toggling the real PetInfo owner. Info request0 is sent even when the factory's job gate then refuses construction. These selectors are unrelated to TALKTYPE emotion IDs.

PetInfo's four-row controller and the actor menu now share feed-confirmation and local egg-return effects. Return applies local cleanup after a failed immediate send as the primary caller does; missing egg rows do not fabricate items. Feed prompts can exist without a PetInfo window. A context prompt does not capture a mode/window pointer, and acceptance checks the mode-manager operation generation before dispatch. PetInfo-owned prompts retain owner teardown behavior. This is a managed asynchronous adaptation, not certification of the original nested blocking-dialog scheduling.

## Presentation and verification

Root visual inspection caught a shared Qt menu hidden behind commerce panes despite correct projected geometry. Shared menu18 now projects into the root popup plane above ordinary commerce panes and below modal prompts, retaining source-managed relative menu layering and scale. A native/Qt pixel assertion at the popup itself prevents this visibility leak from passing geometry-only checks. This corrects the shared popup plane, not the entire application's still-open cross-family window stacking model.

Production replay drives the actual producer, native manager popup selection, mode26 and real socket peer. It covers selector order/labels/target/position; registered, owned, marked and eligible actor gates; modal blocking; Info request/toggle and job rejection; feed cancellation/acceptance without a PetInfo owner; exact actions0/1/2/4/3; exact local egg mutation; invalid indices; and failed-send Info/return semantics. Existing direct PetInfo regressions continue to pass. Native WindowProc checks cover outside release/capture, context Feed confirmation/cancellation and100/125% projection. Three context frame pairs extend the existing ten PetInfo/summary pairs. Final counts, hashes and retained source reports appear beside this evidence.

These checks accept the functional selected actor-menu/controller branch at the current representation. Full actor-pick snapshot/render/scale interaction, global stacking across every window family, native skin/font/locale glyph parity, original modal scheduling, all player/homunculus/mercenary menu recipients and live/deployed behavior remain unaccepted. Synthetic message/art resources are not production font/skin proof. OldRO is not modified by validation.

## Continue with the correct packet identities

The next receiver audit is preparatory, not implementation acceptance. Primary `Zc_Pet_Act`5A64A0 at Ref/GameModePacket.cpp24223+ proves that01AA is pet actor action/dialogue (AID at2, action/speech code at6), not a rename result. Rename requests use01A5 and name/property receipts use01A2, already implemented at the declared normal/alternate scope. Older continuation prose calling01AA a rename result is superseded.

Next restore capture019E/01A0 and source targeting/taming UI, feeding01A3, egg list/selection01A6/01A7 and actor action/dialogue01AA, followed by full actor/follow/reset/failure lifetimes. Preserve the prior PetInfo/shared summary/source request and receive proofs. Command68/87 and all broader item/context, PK47 and independent PK48/49/live/deployed gates remain open; no whole package or discovery row is closed here.

## Final validation

Deploy-OFF all-target/standalone builds pass; focused2/2 in4.27s, application164/164 in200.57s and standalone473/473 in74.87s. Thirteen native/Qt frame pairs and zero QML warnings cover the previous pet owners plus actor menus and ownerless feed confirmation. Selected-PE selector/MsgStr order and source/plan checks pass. Deploy ON restored configure-only; OldRO unchanged. PK27/PK09.21 and the whole plan remain open. [Provenance](PK27-pet-context-provenance.json), [application log](PK27-pet-context-root-ctest.log), [standalone log](PK27-pet-context-unit-ctest.log), [producer disassembly](PK27-pet-context-producer-pe.log) and [render results](PK27-pet-context-menu-renders/pet-window-render-results.json) retain this checkpoint.
