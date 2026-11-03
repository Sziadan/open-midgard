// YUV→RGBA. MIT attribution: third_party/gr2-reader/LICENSE.
#include "Gr2IgcInternal.h"
#include <algorithm>

namespace ro::gr2 {
namespace igc {

void YuvToRgb(const int16_t* yp, const int16_t* up, const int16_t* vp, const int16_t* ap,
    int width, int height, uint8_t* rgba)
{
    const int count = width * height;
    int o = 0;
    for (int i = 0; i < count; ++i) {
        int r = int(up[i]);
        int g = int(yp[i]);
        int b = int(vp[i]);
        int a = int(ap[i]);
        g -= (r + b) / 4; // toward-zero divide
        r += g;
        b += g;
        r = std::clamp(r, 0, 255);
        g = std::clamp(g, 0, 255);
        b = std::clamp(b, 0, 255);
        a = std::clamp(a, 0, 255);
        rgba[o] = uint8_t(r);
        rgba[o + 1] = uint8_t(g);
        rgba[o + 2] = uint8_t(b);
        rgba[o + 3] = uint8_t(a);
        o += 4;
    }
}

} // namespace igc
} // namespace ro::gr2
