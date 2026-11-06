# Packet contract index

[PK09 room extension](../evidence/PK09.21-room.md) records reviewed reception, manager5, room controls/geometry and server-echo bubble ownership. [Visible title/join/password](../evidence/PK09.21-entry.md) now has managed/native/Qt evidence. Management and whisper flows remain open under PK09.21.

[PK37.NPC-TRANSACTIONS](PK37.NPC-TRANSACTIONS.md) records strict selected sends, signed lengths, quote/stock identity, local cancel, FIFO receipts and native/Qt count lifetimes under PK37.09. Broader server eligibility and independent PK48 remain open.

[PK37.NPC-AMOUNTS](PK37.NPC-AMOUNTS.md) records unsigned/zero quotes, exact amounts and guarded stock/deltas under PK37.08. Broad NPC requests/results and independent PK48 remain open.

[PK37.NPC-ITEM-DETAILS](PK37.NPC-ITEM-DETAILS.md) records catalog and selected buy/sell snapshot ownership, passive input and viewport fixes under PK37.07. Broad NPC transactions, legacy grid fallback geometry and independent PK48 remain open.

[PK09.SPECIAL-ITEM-NAMES](PK09.SPECIAL-ITEM-NAMES.md) verifies named/created/forged names, selected pet intimacy/visual tab and delayed producer refresh. Detail expiry and other contexts remain open.

[PK45.CHARACTER-NAME](PK45.CHARACTER-NAME.md) verifies the selected character-ID lookup request/cache namespace; special-item name/UI integration and broader actor/historical review remain open.

[PK09.CARD-NAME-ASSEMBLY](PK09.CARD-NAME-ASSEMBLY.md) verifies generic card-ID grouping and descriptor ownership across actual native/Qt consumers. Special names, expiry and independent PK48 remain open.

[PK09.ITEM-NAME-CONTEXT](PK09.ITEM-NAME-CONTEXT.md) records ordinary name/refinement/identification and quantity consumer repairs. Full cards/sentinels/expiry and independent PK48 remain open.

[PK37.CART-LIFETIME](PK37.CART-LIFETIME.md) records the selected cart status/delta and legacy option/contents repair. IMPLEMENTED_PENDING_REVIEW; broad commerce and independent PK48 remain open.

[PK14.PARTY-POSITION](PK14.PARTY-POSITION.md) now has selected-source field/caller/state and actual native/QML rendered verification. Status IMPLEMENTED_PENDING_REVIEW; independent PK48 remains open.

Additional reviewed continuation contracts (2026-11-05): [NPC representation lifetime](PK20.NPC-CLASS-LIFETIME.md), [ShowEFST ownership/branches](PK22.SHOWEFST-LIFETIME.md), [timed-status selected applicability](PK22.TIMED-STATUS-043F.md), and [legacy-mail records/transactions](PK32.LEGACY-MAIL-STATE.md). These certify source/design scope only; implementation and applicable runtime/UI acceptance remain open.

This index includes both source-backed PK01 applicability contracts and package-owned implementation contracts. Applicability approval alone does not certify client semantics or UI/runtime behavior. Where implementation evidence exists, the package contract and linked replay define its limits. Independent review is deferred to PK48 and is not a prerequisite to implementation.

| Permanent ID | Identity | Owner | Status |
|---|---|---|---|
| PK01.P01.HANDSHAKE | Character S2C raw four-byte account ID | PK04; review PK44/PK47 | Approved for applicability only |
| PK04.RAW-AID | Character S2C raw four-byte account ID, compared before framing | PK04; cross-review PK44/PK47 | Implementation verified; PK48 pending |
| PK01.P02.FRAMING | Cross-stage receive framing state machine | PK05 | Approved for applicability boundary only |
| PK01.P03.01F2 | Map S2C 0x01F2, 20-byte guild status | PK06; review PK47 | Approved for applicability only |
| PK01.P04.01A2-20080910 | Map S2C 0x01A2, 35-byte pet status | PK07; lifecycle PK27; review PK47 | Approved for applicability only |
| PK47.P04.01A2-later | Map S2C 0x01A2 with guarded job field, 37 bytes | PK47 | Open historical variant |
| PK01.P05.0190 | Map C2S 0x0190, 19-byte ActionRequest | PK46; remediation dependency PK08; review PK47 | Approved for applicability only |
| PK01.P05.007E | Map C2S 0x007E, 102-byte UseSkillToPosMoreInfo | PK08; review PK46/PK47 | Approved for applicability only |
| PK01.P05.0437 | Map C2S 0x0437, 7-byte profile-23 ActionRequest | PK46; review PK47 | Approved for applicability only |
| PK44.CLAN.0988-20131223+ | Map S2C 0x0988 clan online count; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.CLAN.0989-20131223+ | Map S2C 0x0989 clan leave acknowledgement; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.CLAN.098E-20131223+ | Map S2C 0x098E clan chat; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ROULETTE.0A1A-C2S-INPUT | Map C2S 0x0A1A null-parser framing; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ROULETTE.0A1A-S2C-OPEN | Map S2C 0x0A1A open result; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ROULETTE.0A1C-C2S-INPUT | Map C2S 0x0A1C null-parser framing; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ROULETTE.0A1C-S2C-INFO | Map S2C 0x0A1C info result; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ROULETTE.0A20-C2S-INPUT | Map C2S 0x0A20 null-parser framing; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ROULETTE.0A20-S2C-GENERATE | Map S2C 0x0A20 generate result; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ROULETTE.0A22-C2S-INPUT | Map C2S 0x0A22 null-parser framing; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ROULETTE.0A22-S2C-ITEM-RESULT | Map S2C 0x0A22 item result; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ACH.0A23-C2S-INPUT | Map C2S 0x0A23 null-parser framing; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ACH.0A23-S2C-LIST | Map S2C 0x0A23 achievement list; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ACH.0A24-C2S-INPUT | Map C2S 0x0A24 null-parser framing; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ACH.0A24-S2C-UPDATE | Map S2C 0x0A24 achievement update; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ACH.0A26-C2S-INPUT | Map C2S 0x0A26 null-parser framing; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ACH.0A26-S2C-REWARD-RESULT | Map S2C 0x0A26 achievement reward result; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.ATTENDANCE.0AEF-C2S-CLAIM | Map C2S 0x0AEF attendance claim; guarded at 20180307, no discovery parent in 20080910 inventory | PK44; review PK48 | Related later-profile identity only |
| PK44.ATTENDANCE.0AF0-C2S-INPUT | Map C2S 0x0AF0 null-parser framing; inactive in selected 20080910 source config | PK24 primary; PK44; review PK48 | Source-config applicability only |
| PK44.ATTENDANCE.0AF0-S2C-RESPONSE | Map S2C 0x0AF0 attendance response; inactive in selected 20080910 source config | PK24 primary; PK44; review PK48 | Source-config applicability only |
| PK44.DRESSROOM.0A02-S2C-OPEN | Map S2C 0x0A02 dressroom open; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |

Independent reviewer: `/root/pk01_p0_review` (gpt-6-astra, high reasoning). Review summary and limits: [PK01.03 applicability approval](../evidence/PK01.03-applicability-approval.md). Server source file hashes, no-Git-metadata limitation, and effective registry evidence: [PK01.02 evidence](../evidence/PK01.02.md).
| PK44.GSTORAGE.09DA-S2C-USAGE-LOG | Map S2C 0x09DA guild-storage usage log; inactive in selected 20080910 source config | PK12 primary; PK44; review PK48 | Source-config applicability only; later item/card width variants traced |
| PK44.REPUTE.0B8D-S2C-LIST | Map S2C 0x0B8D variable reputation list; inactive in selected 20080910 source config | PK47 primary; PK44; review PK48 | Source-config applicability only |
| PK44.REPUTE.0B8D-S2C-UPDATE | Map S2C 0x0B8D single reputation type/points update; inactive in selected 20080910 source config | PK47 primary; PK44; review PK48 | Source-config applicability only |
| PK44.LOGIN.0064-C2S-RAW | Login-server C2S X0064 request; Ref send path confirmed | PK44; review PK48 | Source identity/applicability only |
| PK44.LOGIN.01DB-C2S-REQ-HASH | Login-server C2S X01DB request; selected source registers it; client reachability remains open | PK44; review PK48 | Source identity/applicability only |
| PK44.LOGIN.01DD-C2S-MD5 | Login-server C2S X01DD request; selected source registers it; client reachability remains open | PK44; review PK48 | Source identity/applicability only |
| PK44.LOGIN.01FA-C2S-MD5-CLIENTINFO | Login-server C2S X01FA request; selected source registers it; client reachability remains open | PK44; review PK48 | Source identity/applicability only |
| PK44.LOGIN.0200-C2S-CONNECT-INFO | Login-server C2S X0200 request; Ref send path confirmed | PK44; review PK48 | Source identity/applicability only |
| PK44.LOGIN.0204-C2S-EXE-HASH | Login-server C2S X0204 request; selected source registers it; client reachability remains open | PK44; review PK48 | Source identity/applicability only |
| PK44.LOGIN.0277-C2S-PCBANG-RAW | Login-server C2S X0277 request; selected source registers it; client reachability remains open | PK44; review PK48 | Source identity/applicability only |
| PK44.LOGIN.027C-C2S-MD5-MAC | Login-server C2S X027C request; selected source registers it; client reachability remains open | PK44; review PK48 | Source identity/applicability only |
| PK44.LOGIN.02B0-C2S-CHANNEL-RAW | Login-server C2S X02B0 request; selected source registers it; client reachability remains open | PK44; review PK48 | Source identity/applicability only |
| PK44.LOGIN.0825-C2S-SSO-VARIABLE | Login-server C2S X0825 request; selected source registers it; client reachability remains open | PK44; review PK48 | Source identity/applicability only |

| PK44.LOGIN.0069-S2C-ACCEPT | Login-server successful authentication and character-server list | PK44; review PK48 | Source identity/layout; runtime semantic mismatch recorded |
| PK44.LOGIN.006A-S2C-REFUSE | Login-server authentication refusal and result codes | PK44; review PK48 | Source identity/layout; client result-code mismatch recorded |
| PK44.LOGIN.0081-S2C-NOTIFY-BAN | Login-server operational refusal/disconnect result | PK44; review PK48 | Source identity/layout only |
| PK44.LOGIN.01DC-S2C-ACK-HASH | Login-server variable salt response to X01DB | PK44; review PK48 | Source identity/layout only |

| PK44.CHAR-RAW-AID-HANDSHAKE | Character-stage raw four-byte account-ID echo (non-opcode; cross-package trace) | PK44; PK04 primary; review PK48 | Source identity and client receive gap; PK04 implementation/replay remains open |

| PK44.CHAR-LIST-ENTRY | Character list, character selection, refusal and zone handoff | PK44; review PK48 | Selected legacy path traced; guarded/paged variants and historical reachability remain open |

| PK44.CHAR-CREATE | Selected legacy character creation request and response | PK44; review PK48 | Source registration and current-client paths; result-code mismatch and character record layout remain open |

| PK44.CHAR-DELETE | Legacy immediate and scheduled character deletion transactions | PK44; review PK48 | Server layouts/results traced; active client opcode mismatch and scheduled client reachability remain open |

| PK44.CHAR-RENAME | Legacy two-step character name validation and rename | PK44; review PK48 | Server protocol traced; current-client reachability and request pairing remain open |

| PK44.CHAR-PINCODE | Character-server PIN challenge and update family | PK44; review PK48 | All assigned handlers/registrations compile out for selected PACKETVER=20080910 source config |

| PK44.CHAR-SLOT-MOVE | Character-slot move request/reply transaction | PK44; review PK48 | Server protocol traced; handler contains unchecked source-slot read on malformed request |

| PK44.CHAR-BLOCK-INFO | Character-server variable character-block/unban-time status packet | PK44; review PK48 | Sender returns before construction under selected legacy-charinfo macro; inactive for selected source config |

[PK09.ITEM-TAB-STARTUP](PK09.ITEM-TAB-STARTUP.md) records the late cold-start override, manager-owned runtime selection, native/Qt sharing and storage independence. Full original geometry remains open.

[PK09.INVENTORY-GEOMETRY](PK09.INVENTORY-GEOMETRY.md) defines complete inventory rectangle validation, source quantization,17-pixel mini, stored/live heights and native/Qt close and resize controls. Detail and map/show acceptance remain open.

[PK09.ITEM-DETAIL-GEOMETRY](PK09.ITEM-DETAIL-GEOMETRY.md) records parent geometry, strict saved shapes, clipped scrolling and visible socket rules; source card hit testing and WID82 remain a separate required path.

[PK09.CARD-DETAIL](PK09.CARD-DETAIL.md) records the separate WID82 snapshot, rectangle, hit testing and native/Qt tooltip lifecycle accepted by PK09.15.

[PK09.INVENTORY-SHOW](PK09.INVENTORY-SHOW.md) records separate remembered/live visibility and source map reconstruction accepted by PK09.16.

[PK09.INVENTORY-ACKS](PK09.INVENTORY-ACKS.md) records selected acknowledgement state, counts, metadata and source notices accepted by PK09.17.

[PK09.ITEM-OBTAIN](PK09.ITEM-OBTAIN.md) records the WID58 item acquisition snapshot, sizing, text, colors, overflow and lifetime accepted by PK09.18.

[PK09.ITEM-USE-FEEDBACK](PK09.ITEM-USE-FEEDBACK.md) records source use-effect/action selection, duplicate replacement, exact-AID notices and compatibility recovery feedback accepted by PK09.19.

[PK09.RECOVERY-DELTA](PK09.RECOVERY-DELTA.md) records selected signed013D state, feedback, ordering and independent maxima accepted by PK09.20.

[PK45.SPRITE-CORPSE](PK45.SPRITE-CORPSE.md) records reviewed sprite corpse ownership, original clock and per-clip rendering. PK45.06 now also accepts its distinct GR2 model branch below; no discovery-row closure.

[PK45.GR2-CORPSE](PK45.GR2-CORPSE.md) records completed model death handoff, independent instance/resource ownership, byte-alpha counter,2x elapsed animation and original cutout geometry. Broad actor/model and independent/deployed certification remain open.

[PK09.CHAT-STATE](PK09.CHAT-STATE.md) records the reviewed main state phase: signed cycles, source heights, bottom anchoring and shared native/Qt editor geometry. PK09.21 remains IN_PROGRESS for sticky/mini/detached-history ownership and source stream routing; no discovery-row closure.

[PK09.21 family ownership evidence](../evidence/PK09.21-family.md) extends the partial chat contract with managed auxiliary ownership, sticky transitions and native capture proof. Source routing and remaining lifecycle gates stay open.

[PK09.CHAT-NOTICES](PK09.CHAT-NOTICES.md) records typed source notice parsing, split/unsplit owner rows, backlog/generation lifetime and selected producer proof. The subsequent primary checkpoint implements source tabs; full PK09.21 acceptance remains open.

[PK09.21 primary histories](../evidence/PK09.21-primary.md) records actual source ST/BT view integration, reviewed speech producers and strong source/PE/render corrections. Full detached interaction/persistence and remaining producer/lifecycle acceptance stay open.

[PK09.21 detached visual histories](../evidence/PK09.21-wrapped.md) records separate logical/visual retention, source wrapping/resize replay, shared view/input and retained-owner interaction. Options139, persistence, exact art and broader producer/lifecycle gates remain open.

[PK09.21 battle-message options](../evidence/PK09.21-options.md) records manager139, session filtering, close-only preferences, cross-process startup reload and native/QML UI checks. Shared UIScrollBar fidelity, remaining window persistence/labels/producers and full lifecycle acceptance remain open.

[PK09.21 source scrollbar controls](../evidence/PK09.21-scroll.md) records reviewed base/history subclass geometry, relative drag and captured-child lifetime, plus separate thumb/text offsets after trim. It supersedes the earlier primary clamp-first assumption. Remaining persistence/labels/producers/art and full acceptance stay open.

[PK09.21 labels and restore](../evidence/PK09.21-restore.md) records selected language IDs, empty/missing semantics, source physical-height placement order, write-only startup status, geometry/toggle/show preferences and full teardown PE adjudication. Room/whisper/producers/bubbles, exact skin/locale and full acceptance remain open.

[PK09.21 room creation and settings](../evidence/PK09.21-settings.md) records source validation/filter, real D5/DE, pending metadata, modal/capture/native-QML checks and strong-review corrections. Owner menus/actions and broad acceptance remain open.

[Room member actions](../evidence/PK09.21-actions.md) now implement source-indexed role menus, E0/E2, friend0202, unsupported modal and room-history export. Preserve the corrected self/settings branches. Dedicated whisper and full chat acceptance remain open.

[Dedicated whisper routing](../evidence/PK09.21-whisper.md) now supplies exact-name owners, selected0097/0098 routing, bounded source0096/auto-reply/autosave, preferences and native/QML capture/modal checks. Preserve exact membership versus lookalike warnings, pending outgoing metadata, hidden-owner delivery and source-specific save branches. The latest PE oracle corrects earlier draw-slot prose: detached6D2024+50 ->49F760, main6D1F84+54 ->49CE80, +4C generic no-op. Private menus/configuration/full commands and broad acceptance remain open; follow the current work item.
