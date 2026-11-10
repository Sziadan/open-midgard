# Ground skill prompts in the modern gameplay overlay

The Windows modern gameplay queue now draws Talkie Box and Graffiti above the base gameplay overlay and below the cursor. The prompt layer takes the same route after the Qt texture, Qt CPU bridge, or GDI overlay. It uploads premultiplied ARGB and uses the matching blend factors, which keeps drag opacity and the transparency around the prompts. The GDI base overlay skips the prompts when building the modern frame to avoid painting them twice. The quad uses the same logical UI scale as pointer input when the Qt runtime is active.

The replay exercises the production texture compositor. It checks exact premultiplied upload pixels for the frame, edit, and button at alpha 128, bounds cropped to the visible prompt, and the candidate popup's opaque selection color over a transparent scene. All 164 application checks, 473 standalone checks, and 3 focused checks passed. The skill tree window source and catalog match the baseline. The OldRO executable matched its pre-test hash.

The compositor crops the upload to the prompt bounds when IME popups are closed and uses the logical viewport while they are open. Live GPU readback, scaled interaction, IME, and non-Windows gameplay rendering remain open.

[Source review](PK27-prompt-modern-overlay-source-review.json), [render result](PK27-prompt-modern-overlay-render.json), [focused checks](PK27-prompt-modern-overlay-focused.log), [application checks](PK27-prompt-modern-overlay-root-ctest.log), [standalone checks](PK27-prompt-modern-overlay-unit-ctest.log).
