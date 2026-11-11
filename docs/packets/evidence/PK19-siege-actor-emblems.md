# Siege actor emblems

Selected actor entry and refresh handlers create an overhead guild emblem in siege for player and guardian actors whose head value differs from 100. The gate appears at `Ref/GameModePacket.cpp:5068`, `:5202`, `:5374`, and `:5458`. Guardian jobs are 1285 through 1287 in `Ref/Session2.cpp:1930`. The window helper at `Ref/GameMode.cpp:11811` requires a guild, excludes Gravity AID actors, and checks hiding, burrow, and special hiding with the local Gravity account exception.

The selected concealment path removes existing emblems and recreates them on reveal. China, Malaysia, and Singapore exclude reveal recreation in `Ref/GameActor.cpp:8231–8239`. The actor packet entry path has no such service exclusion. Draw still requires siege, and actor exit releases the window. `Ref/UIControl.cpp:6228` positions the bitmap from the projected actor anchor with offsets of 5 at 640 pixels horizontally and 100 at 480 pixels vertically, both multiplied by the actor pixel ratio.

The port stores activation with the actor and renders through the shared gameplay render pass. It reuses the existing `0151` request throttle, exact-version `0152` bitmap cache, and keyed texture used for guild flags. The overhead quad samples the original 24-by-24 bitmap region of the padded texture. There is no separate Qt window ownership.

The production replay covers actor entry outside and inside siege, the guardian job branch, head type 100, hidden entry, hide and burrow removal, ordinary reveal, service-restricted reveal, and actor removal. [Native emblem verification](PK19-siege-emblem-native-verification.md) covers the rendered packet bitmap and the legacy graffiti resource.

All 164 application and 473 standalone checks passed. The player skill tree and installed OldRO executable matched the prior checkpoint.
