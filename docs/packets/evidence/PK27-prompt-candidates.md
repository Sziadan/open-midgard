# Ground skill prompt IME candidates

The focused ground prompt can display the first Windows IMM candidate list with up to nine entries. Japanese uses a vertical list and palette selection. Korean and Chinese use a horizontal list and page indicators. The popup uses the original sysbox frame tiles when available, positions itself below the edit, and flips above the edit near the bottom edge. The Windows route leaves unsupported profiles and unavailable candidate lists with the platform IME. [Windows candidate notification](https://learn.microsoft.com/en-us/windows/win32/intl/imn-opencandidate).

The replay checks Japanese palette selection, the bottom-edge flip, and dismissal through the native close notification. All 164 application checks, 473 standalone checks, and 3 focused checks passed. The skill tree window source and catalog match the baseline. The OldRO executable matched its pre-test hash.

Live IMM open and change, both horizontal layouts, full source frame and font parity, and the Traditional Chinese keystroke window remain open. Non-Windows Qt continues to use its platform candidate UI.

[Source review](PK27-prompt-candidates-source-review.json), [render result](PK27-prompt-candidates-render.json), [focused checks](PK27-prompt-candidates-focused.log), [application checks](PK27-prompt-candidates-root-ctest.log), [standalone checks](PK27-prompt-candidates-unit-ctest.log).
