# Ground skill text encoding

All 164 application checks and 473 standalone checks passed. The focused suite passed 3 checks in 17.03s.

The ground prompts keep UTF-8 for editing and measure the 78-byte limit in the selected service codepage. The `007E` sender encodes the text before applying the manner filter and writing the packet. The `0191` receiver decodes its bounded field for the skill bubble. The selected 20080910 lengths and offsets follow the effective registry ledger. [Server registry](PK01.02-server-registry-assignments.txt), [contract manifest](PK02.02-contract-manifest.json).

The replay verifies Japanese and Western wire bytes through the sender, a Japanese reply through the registered receiver, rejection by the encoded manner filter, and a Korean editor boundary where UTF-8 and wire sizes differ. Windows uses the original system conversion and replacement behavior. The iconv path accepts representable text and uses Windows-1252 for the default ANSI page. A malformed reply leaves the current bubble in place.

Live server encoding and world entry still need independent evidence. Live IME composition, selection, original global key actions, skin selection, translucency, and snapping remain open alongside the rest of the packet plan.

[Source and test evidence](PK27-prompt-codec-source-review.json), [application checks](PK27-prompt-codec-root-ctest.log), [standalone checks](PK27-prompt-codec-unit-ctest.log).
