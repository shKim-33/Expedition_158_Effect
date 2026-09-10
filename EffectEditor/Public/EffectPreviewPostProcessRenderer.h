#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class RenderTarget;
class ShaderCom;
class VIBuffer_Rect;
NS_END

NS_BEGIN(EffectEditor)

class EffectPreviewPostProcessRenderer final : public Base
{
public:
    EffectPreviewPostProcessRenderer(
        const wchar_t* displayTargetTag,
        const wchar_t* hdrTargetTag,
        const wchar_t* bloomTargetPrefix,
        uint32 width = 512,
        uint32 height = 512);
    ~EffectPreviewPostProcessRenderer() override;

public:
    void Resize(uint32 width, uint32 height);
    bool Ready(string& outStatusMessage);
    HRESULT Begin_HDRTarget() const;
    HRESULT Resolve() const;
    ShaderResourceView* Get_SRV() const;

private:
    bool Ready_DisplayTarget(string& outStatusMessage);
    bool Ready_HDRTarget(string& outStatusMessage);
    bool Ready_BloomTargets(string& outStatusMessage);
    bool Ready_PostProcessShader(string& outStatusMessage);
    bool Ready_PostProcessRect(string& outStatusMessage);
    HRESULT Render_PostProcessResolve() const;
    HRESULT Render_PostProcessToneMapping(const Shared<RenderTarget>& bloomTexture) const;
    HRESULT Render_PostProcessCopy() const;
    HRESULT Render_BloomPass(const Shared<RenderTarget>& dstTarget, const Shared<RenderTarget>& srcTarget, PostProcessPass pass) const;
    HRESULT Render_BloomUpsamplePass(
        const Shared<RenderTarget>& dstTarget,
        const Shared<RenderTarget>& smallTarget,
        const Shared<RenderTarget>& baseTarget) const;
    HRESULT Bind_PostProcessCommon(uint32 width, uint32 height) const;
    void Cleanup();

private:
    static constexpr auto kPostProcessShaderPath{ L"../../../Client/Bin/ShaderFiles/Shader_PostProcess.hlsl" };

    wstring _displayTargetTag{};
    wstring _hdrTargetTag{};
    wstring _bloomTargetPrefix{};
    uint32 _width{ 512 };
    uint32 _height{ 512 };
    Shared<RenderTarget> _displayTarget{};
    Shared<RenderTarget> _hdrTarget{};
    Shared<RenderTarget> _bloomHalfA{};
    Shared<RenderTarget> _bloomHalfB{};
    Shared<RenderTarget> _bloomQuarterA{};
    Shared<RenderTarget> _bloomQuarterB{};
    Shared<RenderTarget> _bloomEighthA{};
    Shared<RenderTarget> _bloomEighthB{};
    Shared<ShaderCom> _postProcessShader{};
    Shared<VIBuffer_Rect> _postProcessRect{};
};

NS_END
