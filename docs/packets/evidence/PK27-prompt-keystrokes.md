# Ground skill prompt Traditional Chinese keystrokes

The Win32 game loop reads the original virtual key from `VK_PROCESSKEY` before translating the message. The focused CP950 prompt keeps the most recent key per cell, removes the most recent key on IME Backspace, retains the latest key on composition updates if it maps to a cell, and clears the display when composition ends. The 60-by-18 popup uses the source palette and fixed cell positions. [ImmGetVirtualKey](https://learn.microsoft.com/en-us/windows/win32/api/imm/nf-imm-immgetvirtualkey).

The selected executable supplies 39 fixed byte pairs and a stable final pair from the adjacent slot string. The period-key entry reads uninitialized stack bytes in the executable. The port leaves that cell empty and preserves the key's slot and lifecycle. Invalid CP950 bytes use a visible replacement character. Empty trailing cells display no glyph.

The replay checks selected-client key mapping, same-cell replacement, Backspace, the native composition and end routes, and popup paint. All 164 application checks, 473 standalone checks, and 3 focused checks passed. The skill tree window source and catalog match the baseline. The OldRO executable matched its pre-test hash.

Live Traditional Chinese IME acceptance, live candidate and OS clipboard behavior, and exact source composition highlighting remain open.

[Source review](PK27-prompt-keystrokes-source-review.json), [render result](PK27-prompt-keystrokes-render.json), [focused checks](PK27-prompt-keystrokes-focused.log), [application checks](PK27-prompt-keystrokes-root-ctest.log), [standalone checks](PK27-prompt-keystrokes-unit-ctest.log).
