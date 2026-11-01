# Ammunition and cursor fixes — 2026-11-01

## Findings and changes

Stackable-item entries carry the allowed equipment slot, but the rebuilt inventory originally left their usable equipment location at zero. Pickup handling also incorrectly marked newly acquired ammunition as already worn. Ammunition has EQP_AMMO (0x8000) as its equipment destination, independently of current wear state. Picked-up stacks start unequipped; adding to an existing equipped stack preserves that stack's state through the existing session merge.

Inventory double-click and dragging to Equipment use the equipment request. Item shortcuts now route equipment, including ammunition, through that same request instead of sending a consumable-use packet. Already-equipped shortcut items are a harmless no-op. Equipment has an ammunition slot using the shared slot display, hover, drag, and unequip behavior; dragging from Equipment back to Inventory sends the existing takeoff request. State changes wait for server acknowledgement.

Ref's ammunition equip command is mode message 100 (`HighPriest.exe.c:353024`): it sends opcode 169 (0x00A9), item index and equipment location. `Ref/UIItemWnd.cpp:11364` routes ammo types 10/16/17 to this command. The rebuilt client normalizes the separate legacy ammunition bookkeeping into its inventory wear-mask representation.

Added the missing four-byte receive handlers:

- 0x013C marks the confirmed ammunition index equipped and clears other ammunition stacks; corroborated by `Ref/GameModePacket.cpp:1293`, `Ref/Session.cpp:25205`, and rAthena `clif_arrowequip`.
- 0x013B handles all Ref `Zc_action_failure` cases 0–3, including job-specific equip notices. Type 0 displays **Please equip the proper ammunition first.** in red in chat. Rejected actions stop automatic attack/chase retries. Ref mapping: `Ref/GameModePacket.cpp:31530-31583`, canonical `HighPriest.exe.c:368514`; English strings corroborated by supplied rAthena `src/map/clif.cpp:4219`. Packet lengths: `Ref/RunningServer/packet_db.txt:231-232`.

The supplied gameplay log `debug_hp_54528.log` reports an unhandled 0x013B on a bow attack. Double Strafe requests later receive zero-duration 0x013E cast announcements and no skill damage result. The old cast handler starts a pose at that announcement, before the server finishes checking the skill. Instant local skills now wait for the skill result to animate; positive-duration casts retain the normal wind-up. Existing successful skill result handlers remain responsible for attack/use animation and effects. This deliberate refinement of Ref's cast-announcement behavior is recorded in MISMATCHES.md.

Cursor drawing previously accepted a snap only if a later, separately sampled mouse position exactly matched the picking sample. Movement between these samples alternated drawing between raw and snapped positions. Drawing now retains the snap selected for that frame, without repicking after world-render submission or moving the OS pointer.

## Verification

`tools/tests/check_ammo_cursor_regressions.py --cxx C:/Qt/Tools/mingw1310_64/bin/g++.exe` compiles current production bodies in a temporary directory and passes checks for stackable ammo decoding, inventory/shortcut equip packets, already-equipped shortcut behavior, dedicated equip acknowledgement, unique ammunition-slot placement, unequip requests waiting for confirmation, missing-ammo chat text/color and retry cancellation, malformed/unknown rejection packets, instant versus timed casts, and a cursor snap retained across changing raw samples. Source checks additionally cover pickup state, both double-click paths, drag routing and packet registration.

The MinGW Qt client preset builds successfully. Live gameplay verification remains for the user; the agent does not launch the game executable. These are focused regression results, not a full CTest-suite result.

## Relogin correction

The next login log, `debug_hp_11372.log`, shows normal inventory packet 0x02E8, ammunition acknowledgement 0x013C (index 9), then equipment list 0x02D0. `ClearEquipmentInventoryItems` incorrectly deleted ammunition because its newly normalized equipment destination was nonzero. It now preserves ammunition types 10/16/17 when replacing the equipment-list portion of inventory. Its stack count and acknowledged equipped state survive. The focused regression compiles this actual cleanup body and both windows' actual visibility/slot-assignment bodies, then verifies the retained equipped arrow stack appears in ETC and the ammunition slot. It also verifies ordinary stale equipment is removed by the refresh.

## Shared option removal button

Equipment uses one control below the preview for falcons, carts and Peco mounts. Labels are Release Falcon, Remove cart and Unmount; it is hidden when none is active. The former separate header OFF control is removed. All draw and mouse paths share one rectangle, positioned relative to the full window height and above the Qt body layer.

Canonical `HighPriest.exe.c:133578` / `135221` expose one option-off button for cart, bird or chicken. Canonical mode cases 0x58/0x91/0x92 (`HighPriest.exe.c:352669`) all send opcode 298 (0x012A). The existing two-byte removal request therefore serves all three states. The supplied rAthena `clif_parse_RemoveOption` confirms the behavior and removes riding/falcon before a cart if states coexist; the label follows that priority. Flags are OPTION_FALCON=0x10 and OPTION_RIDING=0x20. Focused production-body checks cover both labels, every existing cart flag, tracked cart fallback and mount priority.

## Unequipped arrows incorrectly shown on Wizard login

The supplied server's clif_item_normal writes item_data::equip into NORMALITEM_INFO::WearState (clif.cpp:3001). Unlike clif_item_equip's current item equip mask, this is an allowed slot, so every carried arrow stack advertises 0x8000 even when unequipped. The previous decoder interpreted that capability as current wear state. Normal stackable decoding now stores it as m_location and starts m_wearLocation at zero. Actual ammunition state comes from 0x013C / successful equip acknowledgement, as Ref's separate m_equipArrowIndex does. The normal-list -> ammo-ack -> equipment-refresh order still preserves legitimately equipped arrows on relogin.

The regression fixture now sends the real nonzero 0x8000 capability field. It verifies a Wizard carrying Fire Arrows sees them in ETC but not Equipment without an equip acknowledgement, and retains the existing equipped-arrow relogin test. No job-specific rendering filter is used to conceal incorrect item state.

## Shift-bound skill targeting

World left-click dispatch now handles an active skill-target cursor before Shift/seated turn-only handling. Targeting consumes the click even when the clicked target is invalid, retaining the pending skill without falling through to turn, walk, pickup, talk or normal attack. Ordinary Shift-click turning is unchanged when no skill is armed; NPC modal handling remains first. SkillTargetClickTests.cpp compiles the production left-click case, HasPendingSkillUse and ShouldUseTurnOnlyGroundClick bodies. It verifies actor/ground skill clicks with and without Shift, invalid-target retention, ordinary Shift/seated turning, normal movement and NPC gating. The focused regression script runs these checks alongside the existing ammunition/cursor checks.
