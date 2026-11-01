# Frozen shell and wizard staff presentation

## Frozen shell

Canonical `HighPriest.exe.c:515214–515222` / `Ref/Skill.cpp:2714–2722` initialize message-effect type 5 with world depth (`m_alwaysTopLayer=0`) and foreground layer offset (`m_offsetOow=5`). The canonical sprite renderer at `HighPriest.exe.c:316524–316552` derives separate top/bottom depths from screen height and applies the layer offset. It does not put the whole shell at the owner's feet or disable scene depth.

Our actor billboard renderer uses a different depth proxy: a vertical plane with a forward bias, and an additional `0.0005` depth bias. The ice used the projected ground depth with only `0.00015` bias, leaving it behind its owner's depth-writing pixels.

`CWorld::RenderActors` now submits the shell with its owner's exact current-frame billboard entry. `RenderFrozenActorEffectPass` preserves the existing ACT/SPR, action, motion, color, alpha, zoom, perspective scale and screen offsets. `QueueMsgSpriteQuad` samples the owner's depth plane at each shell vertex, extends it beyond cropped body bounds, and moves it one representable step forward. It retains depth testing and uses the owner's scene sort position. The ordinary message-effect pass skips owned ice, preventing duplicate draws. Hidden/non-rendered owners do not submit a shell. Other message effects keep their existing paths.

This is a renderer adaptation of the canonical foreground layer relationship, not a claim that our depth representation reproduces the original equations. No effect timing, freeze motion, scale or asset is changed.

## Staff resource

Canonical `HighPriest.exe` file offset `0x31D824` contains `_` followed by `B7 D4 B5 E5 00` (CP949 `_롯드`). `HighPriest.exe.c:475437` and `Ref/Session.cpp:21464–21465` assign `asc_71D824` to weapon types 10 and 23. The previous runtime token was `B7 CE B5 E5` (`로드`), which does not exist as a nul-terminated literal in the executable and does not match the wizard staff files.

Corrected the token in both `GameActor.cpp` (weapon overlay resolver and transient animation resource) and `Session.cpp` (shared weapon-name fallback). Male and female wizard staff ACT/SPR files exist under the recovered name in the extracted GRF. Existing primary/secondary attack selection and numeric item resource precedence are unchanged.

## Verification

- `python tools/tests/check_frozen_staff_presentation.py --cxx C:/Qt/Tools/mingw1310_64/bin/g++.exe`: passes. Compiles the actual resource resolver against male/female wizard asset names, checks types 10/23, inspects ACT/SPR magic bytes, and pins the token to executable bytes. Compiles the actual shell submission and shared depth sampler; checks multiple depths, scales, cropped-bound extensions, foreground occlusion and invalid/hidden submissions.
- `python tools/tests/check_ammo_cursor_regressions.py --cxx C:/Qt/Tools/mingw1310_64/bin/g++.exe`: passes existing ammunition, input, casting-pose and cloud regressions.
- `cmake --build --preset build-mingw-qt-x64 --parallel 6`: passes.

No client was launched and no media was rendered. Live visual acceptance remains for user testing. No full-suite result is claimed.
