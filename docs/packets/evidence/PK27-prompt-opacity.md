# Ground skill prompt drag opacity

Talkie Box and Graffiti use the selected client's per-draw alpha step of elapsed milliseconds divided by 16. The prompt fades toward 128 during frame capture and returns toward 255 on release. The port clamps the step to the remaining distance so a long interruption cannot underflow alpha.

The Win32 renderer draws the body, artwork, edit, and buttons into a single layer before compositing it over the scene. The replay samples the body, edit, and button over a contrasting background and checks a clipped drag, a release, the unaffected scene outside the prompt, and redraw scheduling after the overlay cache clears ordinary dirty state. All 164 application checks, 473 standalone checks, and 3 focused checks passed. The skill tree window source and catalog match the baseline. The OldRO executable matched its pre-test hash.

The IME candidate and keystroke popups use their own layers. The active Qt gameplay texture and CPU bridge omit these native prompt windows. The color-keyed modern GDI overlay flattens partial alpha during conversion. The replay verifies the native HDC compositor and the redraw request, so modern gameplay routing and live drag acceptance remain open.

[Source review](PK27-prompt-opacity-source-review.json), [render result](PK27-prompt-opacity-render.json), [focused checks](PK27-prompt-opacity-focused.log), [application checks](PK27-prompt-opacity-root-ctest.log), [standalone checks](PK27-prompt-opacity-unit-ctest.log).
