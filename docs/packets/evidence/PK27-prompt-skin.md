# Ground skill prompt skin selection

The ground prompts use the `SkinName` value under `[Graphics]` in `open-midgard.ini`. The value names a directory under the game's `skin` directory. A blank value or `<Basic Skin>` restores the default artwork. The selected folder applies in gameplay. Missing or invalid skin bitmaps fall back one asset at a time. The frame, button states, color chip, and IME lattice refresh when the prompt is opened again after a setting change.

The replay switches between two directory skins, verifies gameplay-only precedence and fallback, checks the custom button and palette, then samples both cached candidate lattices after reopening the same prompt. All 164 application checks, 473 standalone checks, and 3 focused checks passed. The skill tree window source and catalog match the baseline. The OldRO executable matched its pre-test hash.

ZIP skin loading, the original skin picker, and live selection remain open.

[Source review](PK27-prompt-skin-source-review.json), [render result](PK27-prompt-skin-render.json), [focused checks](PK27-prompt-skin-focused.log), [application checks](PK27-prompt-skin-root-ctest.log), [standalone checks](PK27-prompt-skin-unit-ctest.log).
