"""Keep input-pumping Qt overlay work ahead of deferred world sprite submission."""
from pathlib import Path


root = Path(__file__).resolve().parents[2]
mode_source = (root / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8")
effect_source = (root / "src/world/MsgEffect.cpp").read_text(encoding="utf-8")


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


start = mode_source.index("const bool hasLegacyDevice = GetRenderDevice().GetLegacyDevice() != nullptr;")
end = mode_source.index("const DWORD drawSceneEnd", start)
frame = mode_source[start:end]
overlay = frame.index("queuedModernOverlayFrame = QueueModernOverlayQuad(")
world = frame.index("m_view->OnRender();")
submit = frame.index("QueueMsgEffectsOverlayQuad();")
draw = frame.index("g_renderer.DrawScene();", world)
assert overlay < world < submit < draw, "world effects must be produced before their same-frame queue flush"
assert frame.count("QueueMsgEffectsOverlayQuad();") == 1

# Follow the wrapper into the queue consumer and the digit/sprite submission
# helpers. These functions must remain renderer-only: no input/event pumping.
wrapper = function(mode_source, "bool QueueMsgEffectsOverlayQuad()\n{")
assert "return QueueQueuedMsgEffectsQuads();" in wrapper
flush = function(effect_source, "bool QueueQueuedMsgEffectsQuads()")
digit = function(effect_source, "bool QueueDigitSpriteQuad(")
sprite = function(effect_source, "bool QueueMsgSpriteQuad(")
assert "QueueDigitSpriteQuad(draw)" in flush and "QueueMsgSpriteQuad(draw)" in flush
pump_tokens = ("processEvents", "ProcessEvents", "PeekMessage", "DispatchMessage", "QCoreApplication", "QApplication")
for name, body in [("number queue flush", flush), ("digit quad submission", digit), ("message quad submission", sprite)]:
    assert not any(token in body for token in pump_tokens), f"{name} must not pump input/events after world submission"

# Qt's modern overlay queues before world rendering; after world rendering the
# only queue operation is the deferred number/sprite flush above.
after_world_before_draw = frame[world:draw]
assert "QueueModernOverlayQuad(" not in after_world_before_draw
assert "processEvents" not in after_world_before_draw and "PeekMessage" not in after_world_before_draw
print("gameplay frame submits world sprites after Qt event processing; deferred effect flush does not pump events")
