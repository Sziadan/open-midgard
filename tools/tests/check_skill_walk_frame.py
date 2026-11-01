"""Keep bootstrap placement out of active gameplay after a skill stops walking."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = (root / 'src/gamemode/GameMode.cpp').read_text(encoding='utf-8')
start = source.index('int  CGameMode::OnRun()')
end = source.index('\nvoid CGameMode::OnUpdate()', start)
assert 'SyncBootstrapSelfActorWorldPos(' not in source[start:end], (
    'Gameplay frames must not replace a stopped/casting pose with the session walk destination')
start = source.index('void AdvanceMapLoading(CGameMode& mode)\n{')
end = source.index('case CGameMode::MapLoading_CreateView:', start)
assert 'SyncBootstrapSelfActorWorldPos(mode);' in source[start:end]
print('Initial placement remains in map loading; gameplay retains the actor position after skill interruption')
