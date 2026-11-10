# Ground skill prompt selection

All 164 application checks and 473 standalone checks passed. The focused suite passed all 3 checks.

The Talkie Box and Graffiti prompts select existing text on focus entry. Home, End, Left, and Right move the caret. Shift extends the selection, and typing or a committed IME string replaces it. Backspace and Delete respect UTF-8 character boundaries. The 78-byte editor limit follows the service codepage. Text that overflows the limit is clipped on a character boundary and padded to the byte limit.

The replays exercise the Qt and native routes, selection across multibyte text, native Delete, replacement on focus entry, and the selection palette. The renderer uses the existing host font metrics. The skill tree source and catalog match the baseline.

Clipboard shortcuts, live IME preedit, candidate positioning, replacement ranges, original font metrics, and full native keyboard acceptance remain open. Live world entry needs a separate check. The installed OldRO executable matched its pre-test hash.

[Source review](PK27-prompt-selection-source-review.json), [render result](PK27-prompt-selection-render.json), [focused checks](PK27-prompt-selection-focused.log), [application checks](PK27-prompt-selection-root-ctest.log), [standalone checks](PK27-prompt-selection-unit-ctest.log).
