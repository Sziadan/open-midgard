#include "Ijl.h"
#include "core/DllMgr.h"
#include <vector>

bool CIjl::LoadJPG(const unsigned char* buffer, int size, int& width, int& height, u32*& data) {
    if (!g_dllExports.ijlInit || !g_dllExports.ijlRead || !g_dllExports.ijlFree) return false;

    // Use a large enough buffer for JPEG_CORE_PROPERTIES (approx 20KB as seen in decompiler)
    std::vector<u32> ijlProps(5018, 0); 
    u32* props = ijlProps.data();

    if (g_dllExports.ijlInit(props) != 0) return false;

    // Map common IJL properties (offsets based on Ref/Bitmap.cpp)
    props[9] = (u32)buffer; // JPGBuffer
    props[10] = (u32)size;   // JPGSize

    // Read Header
    if (g_dllExports.ijlRead(props, 5) != 0) { // IJL_JBUFF_READHEADER = 5
        g_dllExports.ijlFree(props);
        return false;
    }

    width = (int)props[11];  // JPGWidth
    height = (int)props[12]; // JPGHeight

    // Allocate memory for the decoded image (BGR format)
    data = (u32*)operator new(4 * width * height);
    
    // Set decoding parameters
    props[1] = (u32)data;     // DIBBuffer
    props[2] = (u32)width;    // DIBWidth
    props[3] = (u32)height;   // DIBHeight
    props[4] = 0;             // DIBPadBytes (already aligned to 4 bytes)
    props[5] = 3;             // DIBChannels (BGR)
    props[6] = 2;             // DIBColor (IJL_BGR)

    // Read Whole Image
    if (g_dllExports.ijlRead(props, 3) != 0) { // IJL_JBUFF_READWHOLEIMAGE = 3
        operator delete(data);
        data = nullptr;
        g_dllExports.ijlFree(props);
        return false;
    }

    g_dllExports.ijlFree(props);
    return true;
}
