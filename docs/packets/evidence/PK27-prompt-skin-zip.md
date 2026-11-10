# Ground skill prompt ZIP skins

Set `SkinName` under `[Graphics]` in `open-midgard.ini` to the basename of a skin folder or ZIP in the game's `skin` directory. The prompt tries the directory first and reads ZIP entries directly into memory. Missing, damaged, or undecodable entries fall back through the ordinary asset lookup. Reopening the prompt refreshes its cached artwork after a selection change.

The replay uses stored and deflated bitmap entries with case differences, slash variants, and nested skin folders. It verifies the archive palette, frame, and button art, plus the default artwork for an entry with a bad CRC. All 164 application checks, 473 standalone checks, and 3 focused checks passed. The skill tree window source and catalog match the baseline. The OldRO executable matched its pre-test hash.

The original skin picker and live selection remain open.

[Source review](PK27-prompt-skin-zip-source-review.json), [render result](PK27-prompt-skin-zip-render.json), [focused checks](PK27-prompt-skin-zip-focused.log), [application checks](PK27-prompt-skin-zip-root-ctest.log), [standalone checks](PK27-prompt-skin-zip-unit-ctest.log).
