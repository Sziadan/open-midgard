#include <cstdio>
#include <vector>

#define FRAME_QUEUES(X) \
    X(m_rpFaceList) X(m_rpLMGroundList) X(m_rpLMLightList) X(m_rpRawList) \
    X(m_rpAlphaDepthList) X(m_rpAlphaList) X(m_rpAlphaNoDepthList) \
    X(m_rpEmissiveDepthList) X(m_rpEmissiveList) X(m_rpEmissiveNoDepthList) \
    X(m_rpAlphaOPSortedList) X(m_rpRawAlphaList) X(m_rpRawEmissiveList) \
    X(m_rpLmList) X(m_rpAlphaOPList) X(m_rpAlphaOPNoDepthList) X(m_vertBuffer)

struct Device {
    bool ready = true;
    bool BeginScene() { return ready; }
    void EndScene() {}
};
struct CRenderer {
    Device* m_renderDevice = nullptr;
#define QUEUE(name) std::vector<int> name;
    FRAME_QUEUES(QUEUE)
#undef QUEUE
    std::vector<int> m_rpNullFaceList{1}, m_rpQuadFaceList{1}, m_rpLmQuadFaceList{1};
    std::vector<int>::iterator m_rpNullFaceListIter = m_rpNullFaceList.end();
    std::vector<int>::iterator m_rpQuadFaceListIter = m_rpQuadFaceList.end();
    std::vector<int>::iterator m_rpLmQuadFaceListIter = m_rpLmQuadFaceList.end();
    int drawnAuras = 0;
    void ApplyFogState() {}
    void FlushRenderList() {
        drawnAuras += static_cast<int>(m_rpAlphaOPSortedList.size());
        DiscardRenderList();
    }
    void DiscardRenderList();
    bool DrawScene();
};
#include "RendererFrameBodies.inc"

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        if (!ok) { std::fprintf(stderr, "FAIL: %s\n", name); ++failures; }
    };
    Device device;
    CRenderer renderer;
    renderer.m_renderDevice = &device;
#define ENQUEUE(name) renderer.name.push_back(1);
    FRAME_QUEUES(ENQUEUE)
#undef ENQUEUE
    device.ready = false;
    check(!renderer.DrawScene(), "failed BeginScene is reported");
#define EMPTY(name) check(renderer.name.empty(), #name " does not survive a failed frame");
    FRAME_QUEUES(EMPTY)
#undef EMPTY
    check(renderer.m_rpNullFaceListIter == renderer.m_rpNullFaceList.begin()
        && renderer.m_rpQuadFaceListIter == renderer.m_rpQuadFaceList.begin()
        && renderer.m_rpLmQuadFaceListIter == renderer.m_rpLmQuadFaceList.begin(),
        "failed frame resets primitive pools");
    renderer.m_rpAlphaOPSortedList.push_back(1);
    device.ready = true;
    check(renderer.DrawScene(), "next frame draws successfully");
    check(renderer.drawnAuras == 1, "next frame draws one aura rather than a doubled additive flash");
    renderer.m_renderDevice = nullptr;
    renderer.m_rpAlphaOPSortedList.push_back(1);
    check(!renderer.DrawScene() && renderer.m_rpAlphaOPSortedList.empty(), "missing device also discards queued aura");
    return failures ? 1 : 0;
}
