"""Keep input-pumping Qt overlay work ahead of deferred world sprite submission."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = (root / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8")
start = source.index("const bool hasLegacyDevice = GetRenderDevice().GetLegacyDevice() != nullptr;")
end = source.index("const DWORD drawSceneEnd", start)
frame = source[start:end]
overlay = frame.index("queuedModernOverlayFrame = QueueModernOverlayQuad(")
sprites = frame.index("m_view->OnRender();")
draw = frame.index("g_renderer.DrawScene();")
assert overlay < sprites < draw, "Qt input processing must finish before world sprite textures are queued"
assert "Queue" not in frame[sprites:draw], "no event-pumping overlay work may follow world submission"
print("gameplay frame submits world sprites after Qt event processing")
