# Chat fonts

Unmodified variable TrueType fonts from the official Google Fonts repository:

- [Noto Sans](https://github.com/google/fonts/tree/main/ofl/notosans),
  `NotoSans[wdth,wght].ttf`, renamed to `NotoSans.ttf`.
- [Noto Sans JP](https://github.com/google/fonts/tree/main/ofl/notosansjp),
  `NotoSansJP[wght].ttf`, renamed to `NotoSansJP.ttf`.

Downloaded 2026-11-03. Both are licensed under the SIL Open Font License 1.1;
the corresponding `*-OFL.txt` notices are deployed in `licenses/fonts/`.
Fonts are embedded in the Qt resources and do not need installation in Windows.
Chat uses Noto Sans, with Noto Sans JP as the Han/Hiragana/Katakana fallback;
Qt retains system font fallback for other scripts.

SHA-256:

```
bfb7bb691513f12e734dc346c03a03f784912432d7e3fa8e56efcf906fe86b3d  NotoSans.ttf
c2f3b4d463500a2ddcd3849cded1fceeb9fd6d1c32e6cbecd568453ba50fc68f  NotoSansJP.ttf
```
