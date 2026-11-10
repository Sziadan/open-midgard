# Alternate pet property summary in the shared item detail

Root implementation/review, 2026-11-10, baseline `f330222b`. This continues the actual PetInfo checkpoint without closing PK27, PK09.21, PK09.04 or the full packet plan.

## Source contract and ownership

Primary `Zc_Property_Pet`5A60B0 (Ref/GameModePacket.cpp24392+) reads a signed level word. Levels>=10000 normalize by subtracting10000 and update source factory12/UIItemCollectionWnd/message29. Other levels update normal pet state. The branch does not replace normal pet name, actor/job/egg identity or status fields.

Source and port class/enum names differ. The existing port UIItemInfoWnd/WID_ITEMINFOWND24 already represents source factory12's ordinary item detail, as proven in [PK09.14](../contracts/PK09.ITEM-DETAIL-GEOMETRY.md). Port UIItemCollectionWnd/WID_ITEMCOLLECTIONWND25 is the separate illustration preview. The alternate property now reaches the existing detail owner through its factory, reuses/raises it and adds summary state. It does not create a competing summary singleton or an illustration preview.

Ref/UIFrameWnd2.cpp message29 at9476+ preserves the existing copied ITEM_INFO and artwork and prepends six viewer rows: name, modified/unmodified marker, decimal normalized level, fullness label/state, relationship label/state, accessory equipped/unequipped. It then appends the existing item's description. Message24 at9335+ clears the pet-level sentinel only on actual item replacement; same-item toggle/close occurs before replacement. The port preserves its existing MatchesSelection/toggle path and resets the summary sentinel in SetItemInfo. It retains the source detail geometry, fixed182x80 viewer and separate card-detail owner.

The summary retains signed16 fullness/relationship/normalized level, unsigned16 accessory, byte modified flag and bounded24-byte wire name. A summary arriving over a live item binding retains the current copied item as a snapshot; later inventory removal does not retire the summary. Ordinary selection restores its normal binding and display. Normal pet property packets do not refresh an existing alternate summary.

## Text and rendering

Unlike PetInfo's MsgStr status labels, message29 uses hardcoded CP949 strings. `SourcePetSummaryText.generated.inc` retains exact selected-PE bytes for seven row labels/markers and twelve hunger/relationship state entries. The generator verifies full selected PE hash, literal VAs, primary function provenance and source references. Hunger bands are0..10/11..25/26..75/76..90/91..100; relationship bands0..100/101..250/251..750/751..900/901..1000; both preserve their distinct out-of-range strings. There is no special player-job relationship branch in this summary helper.

On Windows, hardcoded labels decode from949 and wire names decode from the current game code page when received. Item description bytes retain the current item-data code page. The shared native colored-text renderer adds an explicit UTF8 option for this summary; other callers retain their existing default behavior. Qt native rendering uses explicit UTF8 decoding; the non-Qt Windows path uses wide-character metrics/drawing. Long-word splitting preserves codepoint boundaries. Qt projection consumes the same UTF8 description and actual shared owner. Summary name decoding remains stable after receipt when the active game code page changes.

## Verification scope and continuation

Actual production framing/router replay covers signed levels9999/10000/32767/-32768, normalized values0/22767, normal-state independence, all band boundaries, byte255/accessory65535 and zero branches, full24-byte names, DBCS names, singleton reuse/raise, retained item/description, inventory binding retirement and ordinary item replacement. Native/passive Qt checks add fresh-summary, retained-item and125% frame pairs to the seven PetInfo pairs, exercise UTF8 wrapping at a one-pixel width, validate actual owner/viewer geometry/content and retire the projected owner on deletion. A separate Windows non-Qt build/run verifies codepoint wrapping, wide-character drawing, actual red color-switch pixels and DC clipping. Final suite counts, retained frames and hashes are recorded with the adjacent provenance and logs.

This is functional receiver/controller/text-data acceptance at the current item-detail representation. Visual inspection confirms the summary is present in both backends; the existing test font lacks Korean glyphs and draws missing-glyph boxes. Full glyph/font/locale/skin parity remains unaccepted, along with the existing source text viewer, renderer and input gates. Non-Windows text conversion/non-Qt UTF8 drawing is not certified by these Windows checks. No live server or installed world/UI acceptance is claimed.

Next restore the actual mode26 pet context-menu producers and IDs29..33, then capture019E/01A0, feeding01A3, egg list/selection01A6/01A7, rename outcomes01AA and remaining actor/follow/reset/failure lifetimes. Those menu IDs are not TALKTYPE emotion IDs. Preserve source action ordering, local optimistic mutations and actual owner teardown, plus all broader item/context, command68/87, PK47 and independent PK48/49/live/deployed gates.

## Final validation

Deploy-OFF all-target and standalone builds pass; focused2/2 in9.73s, application164/164 in198.60s and standalone473/473 in75.45s. Ten native/Qt frame pairs include three alternate-summary states. Separate Windows non-Qt UTF8 drawing/wrapping/color/clip check passes. Generator/source and plan checks pass separately. Deployment ON restored configure-only; OldRO unchanged. Full PK27/PK09.21 and broader gates remain open. [Provenance](PK27-pet-summary-provenance.json), [application results](PK27-pet-summary-root-ctest.log), [standalone results](PK27-pet-summary-unit-ctest.log), [non-Qt Windows check](PK27-pet-summary-native-text-check.log) and [render results](PK27-pet-summary-renders/pet-window-render-results.json) retain the exact checkpoint.
