# Legacy map property lifetime

The selected client calls `CWorld::CWorld` at `0x5696D0` from `CGameMode::OnInit` (`Ref/GameMode.cpp:7859–7907`). The constructor at `0x4FA8E0` clears PK, siege, battlefield and event PvP (`Ref/GameWorld.cpp:342–348`). The executable clears EDI at `0x4FA90F` and stores it to the four fields at `0x4FA9D2`, `0x4FA9D5`, `0x4FA9D8`, and `0x4FA9EF`. The selected `0091` equal-map branch at `Ref/GameModePacket.cpp:23155–23161` rebuilds actors and initializes the current mode. The different-map branch switches modes.

The port binds the singleton `g_world` in `CGameMode::OnInit`, so the constructor only ran once. The new explicit reset gives every real game-mode entry the selected client's fresh world flags, including a same-name mode replacement. Loaded same-map reuse keeps its world. The map type, map property table and saved effect preference retain their session ownership. The `0199` and `01D6` handlers restore the effective minimum-effect setting before applying the received rule.

Production loopback replay sends legacy event PvP, battleground and siege frames to activate the four flags. One path then takes the real loaded `0091` reuse branch and checks that the rules survive its load acknowledgement. The other path enters a replacement mode before binding the cached GAT and checks that the four world flags are clear. It also checks that a subsequent neutral property packet restores the saved effect setting while the session map type persists.

The focused lifetime replay and all 164 application and 473 standalone checks passed with deployment disabled. The player skill tree and installed OldRO executable matched the prior checkpoint. Live transitions and remaining PK19 presentation and action checks are open.
