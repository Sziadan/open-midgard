# Ground skill prompt native composition paint

The native prompt display inserts preedit at the committed cursor in a temporary copy and paints the full selected span with colorchip `(6,2)`. With a collapsed selection, it paints a composition highlight at least two pixels wide during the visible half of the 500 ms blink cycle. The edit continues to scroll by the committed cursor. Qt retains its input-method formatting and replacement ranges.

The replay checks that selected text remains in the temporary display, the highlight covers the combined span, the committed text is intact, and a stable sampled blink phase produces the expected palette pixel. All 164 application checks, 473 standalone checks, and 3 focused checks passed. The skill tree window source and catalog match the baseline. The OldRO executable matched its pre-test hash.

Live Windows IME and clipboard verification remain open. The prompt still uses port font metrics and UTF-8 display boundaries. Exact original codepage clipping, language-prefix drawing and popup placement while text is selected need separate verification.

[Source review](PK27-prompt-native-paint-source-review.json), [render result](PK27-prompt-native-paint-render.json), [focused checks](PK27-prompt-native-paint-focused.log), [application checks](PK27-prompt-native-paint-root-ctest.log), [standalone checks](PK27-prompt-native-paint-unit-ctest.log).
