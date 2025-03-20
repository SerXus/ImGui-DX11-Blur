#include "shader.hpp"

static ID3D11Device* d3d11Device;
static ID3D11DeviceContext* d3d11Context;
static ID3D11RenderTargetView* d3d11RenderTarget;
static ID3D11Texture2D* backBufferTexture;
static ID3D11Texture2D* effectTexture;
static ID3D11SamplerState* effectTextureSampler;
static ID3D11ShaderResourceView* backBufferResourceView;
static ID3D11PixelShader* blurPixelShader;
static ID3D11Buffer* blurShaderCBuffer;
static ID3D11RenderTargetView* oldRenderTargetView;

// Blur
static BlurInputBufferT blurParams;

static void shaders::blurBegin(const ImDrawList* drawList, const ImDrawCmd* drawCmd) {
    d3d11Context->OMGetRenderTargets(1, &oldRenderTargetView, nullptr);
    d3d11Context->OMSetRenderTargets(1, &d3d11RenderTarget, nullptr);

    D3D11_MAPPED_SUBRESOURCE blurShaderCBufferSubres;
    d3d11Context->Map(blurShaderCBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &blurShaderCBufferSubres);

    const auto blurInput = reinterpret_cast<BlurInputBufferT*>(blurShaderCBufferSubres.pData);
    blurInput->textureSize = { ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y };
    blurInput->blurRadius = blurParams.blurRadius;
    blurInput->blurQuality = blurParams.blurQuality;
    blurInput->blurDirections = blurParams.blurDirections;

    d3d11Context->Unmap(blurShaderCBuffer, 0);
    d3d11Context->PSSetConstantBuffers(0, 1, &blurShaderCBuffer);
    d3d11Context->PSSetSamplers(0, 1, &effectTextureSampler);
    d3d11Context->PSSetShader(blurPixelShader, nullptr, 0);
}

void shaders::blurEnd(const ImDrawList* drawList, const ImDrawCmd* drawCmd) {
    d3d11Context->OMSetRenderTargets(1, &oldRenderTargetView, nullptr);
}

void shaders::beginFrame(IDXGISwapChain* swapChain) {
    if (!effectResourceView) {
        createDeviceObjects(swapChain);
    }

    d3d11Context->CopyResource(effectTexture, backBufferTexture);
}

void shaders::createDeviceObjects(IDXGISwapChain* swapChain) {
    swapChain->GetBuffer(0, IID_PPV_ARGS(&backBufferTexture));
    swapChain->GetDevice(IID_PPV_ARGS(&d3d11Device));

    d3d11Device->GetImmediateContext(&d3d11Context);

    D3D11_TEXTURE2D_DESC textureDesc = {};
    backBufferTexture->GetDesc(&textureDesc);
    d3d11Device->CreateTexture2D(&textureDesc, nullptr, &effectTexture);
    d3d11Device->CreateShaderResourceView(effectTexture, nullptr, &effectResourceView);
    d3d11Device->CreateShaderResourceView(backBufferTexture, nullptr, &backBufferResourceView);
    d3d11Device->CreateRenderTargetView(effectTexture, nullptr, &d3d11RenderTarget);

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = 1.0f;

    d3d11Device->CreateSamplerState(&samplerDesc, &effectTextureSampler);

    // @todo: Compile this ahead of time and embed a binary blob
    constexpr static const char blurPixelShaderSource[] = R"(
        struct PsInput
        {
            float4 pos : SV_POSITION;
            float4 color : COLOR0;
            float2 uv : TEXCOORD0;
        };

        cbuffer BlurInputBuffer : register(b0)
        {
            float2 textureSize;
            float blurDirections;
            float blurQuality;
            float blurRadius;
        };

        Texture2D backTexture : register(t0);
        sampler textureSampler : register(s0);

        float4 main(PsInput input) : SV_Target
        {
            const float pi2 = 6.28318530718f;

            float2 radius = blurRadius / textureSize;
            float4 color = backTexture.Sample(textureSampler, input.uv);

            float increment = 1.0f / blurQuality;
            for (float d = 0.0f; d < pi2; d += pi2 / blurDirections)
            {
                for (float i = increment; i < 1.00001f; i += increment)
                {
                    float2 blurUv = input.uv + float2(cos(d), sin(d)) * radius * i;
                    color += backTexture.Sample(textureSampler, blurUv);
                }
            }

            return color / (blurQuality * blurDirections + 1.0f);
        }
    )";

    ID3DBlob* blurPixelShaderBlob;

    if (D3DCompile(
        blurPixelShaderSource, sizeof(blurPixelShaderSource), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0,
        &blurPixelShaderBlob, nullptr
    ) != S_OK)
    {
        return;
    }

    d3d11Device->CreatePixelShader(
        blurPixelShaderBlob->GetBufferPointer(), blurPixelShaderBlob->GetBufferSize(), nullptr, &blurPixelShader
    );

    blurPixelShaderBlob->Release();

    D3D11_BUFFER_DESC blurCBufferDesc = {};

    blurCBufferDesc.ByteWidth = sizeof(BlurInputBufferT);
    blurCBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    blurCBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    blurCBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    d3d11Device->CreateBuffer(&blurCBufferDesc, nullptr, &blurShaderCBuffer);
}

void destroyDeviceObjects() {
    // @todo: Do this lol
}

void shaders::drawToScreenUV(ImVec2 pos, ImVec2 size, ImColor color, float rounding, ImDrawFlags flags) {
    ImVec2 startUv = pos / ImGui::GetIO().DisplaySize;
    ImVec2 endUv = size / ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddImageRounded(
        reinterpret_cast<ImTextureID>(effectResourceView), pos, size,
        startUv, endUv, color, rounding, flags
    );
}

void shaders::applyBlur(float size, float quality, float directions) {
    blurParams.blurRadius = size;
    blurParams.blurQuality = quality;
    blurParams.blurDirections = directions;

    ImGui::GetBackgroundDrawList()->AddCallback(blurBegin, nullptr);
    ImGui::GetBackgroundDrawList()->AddImage(
        reinterpret_cast<ImTextureID>(backBufferResourceView),
        { 0.0f, 0.0f }, ImGui::GetIO().DisplaySize, { 0.0f, 0.0f }, { 1.0f, 1.0f }, ImColor(255, 255, 255)
    );
    ImGui::GetBackgroundDrawList()->AddCallback(blurEnd, nullptr);
    ImGui::GetBackgroundDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}