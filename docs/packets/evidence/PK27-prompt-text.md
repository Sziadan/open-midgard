# Ground prompt character input

All 164 application checks and 473 standalone checks passed. The focused suite passed 3 checks in 16.99s.

Both prompts now receive native UTF-16 text and committed Qt input at the focused editor. Character insertion keeps the 78-byte limit without splitting a UTF-8 sequence. Backspace removes one complete code point. The Windows handler accepts IME result strings and consumes the result flag to avoid default redelivery. The Qt host sends printable key text and committed IME events to the same prompt model.

The primary `CLanguage::OnChar` switch returns immediately for Enter and Escape. The prompt no longer converts physical Enter or Escape into button messages. Its explicit default-push and default-cancel message handlers remain available through the source window contract.

The replay exercises native `WM_CHAR`, surrogate pairs, Backspace, the byte boundary, and Enter and Escape. It also sends real Qt key and input-method events to the Qt host. Live Windows IME composition and selection, service-codepage encoding for packet 007E, and the original global key actions remain open.

[Source and test evidence](PK27-prompt-text-source-review.json), [application checks](PK27-prompt-text-root-ctest.log), [standalone checks](PK27-prompt-text-unit-ctest.log).
