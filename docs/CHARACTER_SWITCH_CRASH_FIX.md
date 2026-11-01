# Character-switch crash: stale ground-skill index

## Evidence

User sequence: play wizard, return to character selection, enter world as assassin.

`D:\Spel\OldRO\debug_hp_38868.log` ends with the assassin finishing `prontera.rsw` loading at tick 273597703, followed by access violation `0xC0000005` in `msvcrt.dll+0x7B7C0` at tick 273597812. The matching dump is `crash-dumps\open-midgard-crash-20261101-205558-pid38868.dmp`. The deployed binary matched the build by SHA256 before investigation.

The dump's main-thread stack contains the following executable return addresses, mapped against that exact binary's symbol table and disassembly:

- RVA `0x3CA839`: `QtUiStateAdapter::syncGameplay`, immediately after converting a ground skill's `GetChatBubbleText()` to QString.
- RVA `0x37A4D8`: `ToQString(const std::string&)`.
- RVA `0x4244FE`: `QString::fromUtf8`.
- RVA `0x4135DF`: `qstrlen`.
- RVA `0xA4D29`: `CWorld::GetSkillSpeechBubbleAnchor`.

These are recovered stack values corroborated with the disassembled calls, not a claim of a complete debugger-unwound stack; this fallback minidump lacks an exception stream.

## Cause and fix

`m_gameObjectList` owns the ground-skill objects. `m_skillList` is a non-owning index used by packets, world anchors and the Qt overlay. Individual removal paths already erase the skill index entry, but `CWorld::ClearFixedObjects` deleted every world object without clearing that index. Since the global world is reused for the next character/map, those freed skill pointers remained queryable. Reused memory could pass the visibility/chat checks and reach the string conversion shown in the dump.

Clear `m_skillList` before deleting world objects. This applies both to full character teardown and to map transitions that preserve attached actor effects: the preservation branch retains only live `CMsgEffect` objects, never ground skills. Ownership and effect-preservation behavior are otherwise unchanged. Clearing before deletion also avoids exposing stale entries to destruction callbacks.

## Validation

`python tools/tests/check_world_cleanup.py --cxx C:/Qt/Tools/mingw1310_64/bin/g++.exe` compiles the actual production cleanup function with ownership-aware objects. It verifies removal of the skill index before destruction, repeated cleanup, a new character repopulating the index, map cleanup preserving attached effects, removal of unowned/detached effects, and deletion of ground items exactly once.

The MinGW Qt client build passes. The game is not launched by the agent; the reported character-switch sequence remains for live confirmation by the user.
