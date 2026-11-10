# Ground skill prompt clipboard

The Talkie Box and Graffiti prompts use Ctrl+C, Ctrl+X, Ctrl+V, Shift+Delete, and Shift+Insert. Shift takes precedence when combined with Ctrl. The Win32 and Qt Windows hosts use CF_TEXT with the selected service codepage. The non-Windows Qt path uses the platform text clipboard and converts through the service codepage.

The replay exercises a reverse selection with exact CP932 copy bytes, Ctrl+Shift+X precedence on the Qt host, replacement with pasted CP932 and CRLF, truncation at NUL, and the 78-byte edit limit. All 164 application checks, 473 standalone checks, and 3 focused checks passed. The replay leaves the system clipboard intact. Direct OS clipboard verification, the non-Windows Qt path, and live IME acceptance remain open. The skill tree window source and catalog match the baseline.

[Source review](PK27-prompt-clipboard-source-review.json), [render result](PK27-prompt-clipboard-render.json), [focused checks](PK27-prompt-clipboard-focused.log), [application checks](PK27-prompt-clipboard-root-ctest.log), [standalone checks](PK27-prompt-clipboard-unit-ctest.log).
