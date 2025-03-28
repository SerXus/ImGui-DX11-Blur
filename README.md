NOTE: Somehow it does not apply within the ImGui Window.

shaders::beginFrame(pSwapChain);


shaders::applyBlur(10.f, 3.f, 20.f);


shaders::drawToScreenUV(pos, pos + size, ImColor(255, 255, 255), 8.f, ImDrawFlags_None);
