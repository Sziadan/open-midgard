# Ground skill prompt IME composition

Talkie Box and Graffiti now render transient IME composition separately from the committed text. The native route reads composition and result text through IMM. The Qt route handles cursor attributes, preedit formats, surrounding-text queries, and UTF-16 replacement ranges. The cursor rectangle follows the rendered edit, including horizontal scrolling. [Qt input method events](https://doc.qt.io/qt-6/qinputmethodevent.html), [Windows IME context](https://learn.microsoft.com/en-us/windows/win32/intl/wm-ime-setcontext).

The replay checks that preedit stays out of committed text, the caret rectangle advances with composition, cancel restores the underlying selection, and a range spanning a supplementary character replaces it intact. All 164 application checks, 473 standalone checks, and 3 focused checks passed. The skill tree window source and catalog match the baseline.

Live IME and OS clipboard acceptance, original candidate and keystroke windows, exact native preedit highlighting, and full keyboard parity remain open. The render path currently previews composition over the selected span and preserves it on cancel. The source display-text helper inserts preedit at the current language cursor, and its edit drawing includes palette-based blinking highlights. Verify those source details separately. The OldRO executable matched its pre-test hash.

[Source review](PK27-prompt-ime-source-review.json), [render result](PK27-prompt-ime-render.json), [focused checks](PK27-prompt-ime-focused.log), [application checks](PK27-prompt-ime-root-ctest.log), [standalone checks](PK27-prompt-ime-unit-ctest.log).
