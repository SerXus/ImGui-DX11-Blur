#define IMGUI_DEFINE_MATH_OPERATORS

#include <d3d11.h>
#include <d3dcompiler.h>
#include "../imgui/imgui.h"

struct float2
{
    float x, y;
};

inline ID3D11ShaderResourceView* effectResourceView;

struct alignas(16) BlurInputBufferT {
    float2 textureSize;
    float blurDirections;
    float blurQuality;
    float blurRadius;
};

namespace shaders {
    void blurBegin(const ImDrawList* drawList, const ImDrawCmd* drawCmd);
    void blurEnd(const ImDrawList* drawList, const ImDrawCmd* drawCmd);
    void createDeviceObjects(IDXGISwapChain* swapChain);
    void drawToScreenUV(ImVec2 pos, ImVec2 size, ImColor color, float rounding, ImDrawFlags flags);
    void applyBlur(float size, float quality, float directions);
    void beginFrame(IDXGISwapChain* swapChain);
}

inline void blur(const ImVec2& pos, const ImVec2& size, ImColor color, float rounding, ImDrawFlags flags, bool background) {
    const ImVec2& scaledPos{ pos };
    const ImVec2& scaledSize{ size };
    const ImVec2& finalPos{ scaledPos - (scaledSize / 2.f) };
    shaders::drawToScreenUV(finalPos, finalPos + scaledSize, color, rounding, flags);
}
