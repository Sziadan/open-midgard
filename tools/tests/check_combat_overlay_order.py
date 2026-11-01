"""Prevent dropping world-produced healing/damage/Miss draws each frame."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = (root / 'src/gamemode/GameMode.cpp').read_text(encoding='utf-8')
start = source.index('int  CGameMode::OnRun()')
end = source.index('\nvoid CGameMode::OnUpdate()', start)
frame = source[start:end]
clear = frame.index('ClearQueuedMsgEffects();')
produce = frame.index('m_view->OnRender();')
submit = frame.index('QueueMsgEffectsOverlayQuad();')
draw = frame.index('g_renderer.DrawScene();', produce)
assert clear < produce < submit < draw
assert frame.count('QueueMsgEffectsOverlayQuad();') == 1
assert 'ClearQueuedMsgEffects();' not in frame[produce:draw]
# Verify that both numbers and sprite effects enter the consumed queue.
messages = (root / 'src/world/MsgEffect.cpp').read_text(encoding='utf-8')
queue_start = messages.index('bool QueueQueuedMsgEffectsQuads()')
queue_end = messages.index('bool GetQueuedMsgEffectsBounds(', queue_start)
flush = messages[queue_start:queue_end]
assert 'QueueDigitSpriteQuad(draw)' in flush and 'QueueMsgSpriteQuad(draw)' in flush
print('Combat overlay production/submission ordering checks passed')
