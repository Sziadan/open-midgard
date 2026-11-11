# Native siege emblem verification

The [native runner](../../../tools/tests/run_siege_emblem_preview.ps1) drives the application mode loop and production `CView::OnRender` on Vulkan. It sends the original actor and map-property packets through production framing and routing. The downloaded image comes through the `0152` decoder. The fixture bitmap is a 12-by-12 red square on a 24-by-24 magenta-keyed image.

The selected `CSession::GetEmblemFileName` at `Ref/Session.cpp:17450` uses `flagGraffiti<guild>.ebm` for nonpositive versions. The selected `CEmblemRes::Load` at `Ref/Emblem.cpp:77` reads a compressed bitmap and calls `UnCompressEmblem` at `:7`, which calls zlib. The port reads that file through the normal asset path and bounds the compressed input and decoded bitmap to the source's 8192-byte buffers. The 24-by-24 draw region retains the existing magenta key.

Downloaded emblems still require the advertised version. For guild flag actors, a local version-zero graffiti texture takes priority when present. Without it, the flag retains the existing current-emblem request for an unspecified version. The local asset and downloaded texture caches are independent.

The OldRO `data.grf`, `event.grf`, and `fdata.grf` indexes and the loose resource scan found no `flagGraffiti<guild>.ebm`. The event archive was checked with the repository's legacy GRF filename decoder. The existing `rg_flaggraffiti` BMP, SPR, and ACT files belong to the item presentation. The native runner stages a source-format synthetic EBM to exercise the fallback without editing the installation.

Six 640-by-480 Vulkan frames verified the shared gameplay path. The valid download and staged EBM frames matched pixel for pixel. Each added exactly 144 opaque red pixels, forming a 12-by-12 square at (320, 142). Ordinary-map, hidden-actor, wrong-version and missing-graffiti frames were byte-identical. The visible and EBM BMP SHA-256 is `08e116ea8f417f2ec7ddfa23220c7e1ef10c9527a524d9fea4e14a71424a82fb`. The four controls have SHA-256 `cd56d14c900a74112118425b24c5f648f7efce6ec3fa52c9e19c009ac56b2d70`. The tested executable SHA-256 is `80c8391e11c8ded2208bd14cb835d43593911953a7a594bee04e70b95113a1ce`.

All 164 application and 473 standalone checks passed with deployment disabled. The player skill tree and installed OldRO executable matched the prior checkpoint. Live siege transitions and a rendered guild flag model using a real local EBM remain outside this capture.
