# Ground prompt dragging and palette sampling

All 164 application checks and 473 standalone checks passed. The focused suite passed 3 checks in 18.69s, with native input and two palette renders.

The Talkbox and Graffiti prompts now capture blank-frame drags and move from the pointer and frame origins recorded at press time. The edit follows its parent. Releasing outside or deleting the frame clears capture. The prompts record the source translucency targets of 128 and 255. The compositor still needs to render those transitions.

The available color-chip bitmap supplies the background at (2, 2) and side borders at (20, 6). The replay checks distinct colors, alpha removal and the red out-of-bounds result. It also checks typing after clicking the edit and discarding a button press released outside its bounds. The current asset resolver supplies the bitmap, while source custom-skin and language selection remain open.

The primary executable remaps Return to 65 before dispatch and reaches `ExeMenu` at 5C2F60. Trace the separate character and IME route before treating physical Enter as prompt submission. Escape-to-default-cancel routing still needs evidence. Registered-window snapping and broader focus lifetimes remain open.

[Source review](PK27-prompt-input-source-review.json), [render results](PK27-prompt-input-render.json), [application checks](PK27-prompt-input-root-ctest.log), [standalone checks](PK27-prompt-input-unit-ctest.log).
