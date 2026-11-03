# Unicode chat

The Qt gameplay overlay uses actual TextInput/TextEdit controls, with bundled
Noto Sans and Noto Sans JP fonts. Swedish, Cyrillic, hiragana, katakana and
kanji render without requiring those fonts to be installed on the machine.
Other scripts can use Qt's system font fallback.

The main Windows window and both message pumps use Unicode messages. The
chat bridge forwards UTF-16 characters (including surrogate pairs), editing
keys and mouse events to the offscreen Qt scene. Native IME composition is
forwarded as preedit/commit events, with candidate windows placed at the caret.
Other legacy text fields retain their existing local-code-page interfaces.

Chat drafts and packet payloads use UTF-8. Paste flattens newlines/tabs into
spaces and removes control characters. Whisper names retain the protocol's
24-byte limit; trimming never splits a UTF-8 character. Messages retain the
existing 180-byte limit, with trimming at complete UTF-8 characters. The server and other clients must understand the same
UTF-8 payloads to display multilingual messages correctly.

Both input fields support mouse selection, cursor movement, Shift+arrows,
Ctrl+A/C/X/V and Qt's usual undo/redo shortcuts. Enter sends, Escape leaves
input, Tab switches fields and Up/Down recall sent messages. History is a
read-only selectable document, including channel colours and literal markup;
selection can span messages. Use Ctrl+C to copy it. Mouse wheel and the scrollbar
browse the active channel tab's history. New messages follow the bottom only
when already there; an active selection keeps its document stable until cleared.
Move/resize gestures remain on the window header and frame.

Run the focused native/Qt checks from the repository root:

```
cmake -S tools/tests/chat_unicode -B build-chat-tests -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/mingw_64
cmake --build build-chat-tests
build-chat-tests/chat_unicode_tests.exe
```

Use the matching MinGW compiler and Qt runtime on PATH. For a headless run set
`QT_QPA_PLATFORM=offscreen` and `QT_QUICK_BACKEND=software`. The tests cover font
glyphs, native Unicode input, surrogate pairs, clipboard editing, IME events,
UTF-8 validation/limits, selection across history messages, wheel scrolling and
GUI scaling, and produce `build-chat-tests/chat-unicode.png`.
