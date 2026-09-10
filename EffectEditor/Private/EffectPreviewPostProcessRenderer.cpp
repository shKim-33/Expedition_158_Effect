#include "EffectPreviewPostProcessRenderer.h"

#include "EffectEditorInstance.h"
#include "RenderTarget.h"
#include "ShaderCom.h"
#include "VIBuffer_Rect.h"

NS_BEGIN(EffectEditor)

namespace
{
    Shared<RenderTarget> Create_PreviewRenderTarget(
        const wchar_t* targetTag,
        uint32 width,
        uint32 height,
        DXGI_FORMAT format,
        const Vec4& clearColor,
        bool useDepthStencil)
    {
        return RenderTarget::Create(
            targetTag,
            EDITOR->Get_Device(),
            EDITOR->Get_Context(),
            width,
            height,
            format,
            clearColor,
            useDepthStencil
        );
    }

    wstring Make_TargetTag(const wstring& prefix, const wchar_t* suffix)
    {
        return prefix + suffix;
    }
}

EffectPreviewPostProcessRenderer::EffectPreviewPostProcessRenderer(
    const wchar_t* displayTargetTag,
    const wchar_t* hdrTargetTag,
    const wchar_t* bloomTargetPrefix,
    uint32 width,
    uint32 height)
    : _displayTargetTag{ displayTargetTag != nullptr ? displayTargetTag : L"EffectPreviewDisplay" }
    , _hdrTargetTag{ hdrTargetTag != nullptr ? hdrTargetTag : L"EffectPreviewHDR" }
    , _bloomTargetPrefix{ bloomTargetPrefix != nullptr ? bloomTargetPrefix : L"EffectPreviewBloom" }
    , _width{ max(1u, width) }
    , _height{ max(1u, height) }
{
}

EffectPreviewPostProcessRenderer::~EffectPreviewPostProcessRenderer()
{
    Cleanup();
}

void EffectPreviewPostProcessRenderer::Resize(uint32 width, uint32 height)
{
    width = clamp(width, 128u, 2048u);
    height = clamp(height, 128u, 2048u);
    if (_width == width && _height == height)
        return;

    _width = width;
    _height = height;
    _displayTarget.reset();
    _hdrTarget.reset();
    _bloomHalfA.reset();
    _bloomHalfB.reset();
    _bloomQuarterA.reset();
    _bloomQuarterB.reset();
    _bloomEighthA.reset();
    _bloomEighthB.reset();
}

bool EffectPreviewPostProcessRenderer::Ready(string& outStatusMessage)
{
    if (EDITOR == nullptr)
    {
        outStatusMessage = "Preview renderer is not ready.";
        return false;
    }

    return Ready_DisplayTarget(outStatusMessage) &&
           Ready_HDRTarget(outStatusMessage) &&
           Ready_BloomTargets(outStatusMessage) &&
           Ready_PostProcessShader(outStatusMessage) &&
           Ready_PostProcessRect(outStatusMessage);
}

HRESULT EffectPreviewPostProcessRenderer::Begin_HDRTarget() const
{
    CHECK_NULL(_hdrTarget, E_FAIL);
    return _hdrTarget->Begin(true, true);
}

HRESULT EffectPreviewPostProcessRenderer::Resolve() const
{
    return Render_PostProcessResolve();
}

ShaderResourceView* EffectPreviewPostProcessRenderer::Get_SRV() const
{
    return _displayTarget != nullptr ? _displayTarget->Get_SRV() : nullptr;
}

bool EffectPreviewPostProcessRenderer::Ready_DisplayTarget(string& outStatusMessage)
{
    if (_displayTarget != nullptr)
        return true;

    _displayTarget = Create_PreviewRenderTarget(
        _displayTargetTag.c_str(),
        _width,
        _height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        Vec4{ 0.18f, 0.18f, 0.18f, 1.f },
        false
    );

    if (_displayTarget == nullptr)
    {
        outStatusMessage = "Preview render target creation failed.";
        return false;
    }

    return true;
}

bool EffectPreviewPostProcessRenderer::Ready_HDRTarget(string& outStatusMessage)
{
    if (_hdrTarget != nullptr)
        return true;

    _hdrTarget = Create_PreviewRenderTarget(
        _hdrTargetTag.c_str(),
        _width,
        _height,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        Vec4{ 0.02f, 0.025f, 0.035f, 1.f },
        true
    );

    if (_hdrTarget == nullptr)
    {
        outStatusMessage = "Preview HDR render target creation failed.";
        return false;
    }

    return true;
}

bool EffectPreviewPostProcessRenderer::Ready_BloomTargets(string& outStatusMessage)
{
    const uint32 halfWidth = max(1u, _width / 2u);
    const uint32 halfHeight = max(1u, _height / 2u);
    const uint32 quarterWidth = max(1u, halfWidth / 2u);
    const uint32 quarterHeight = max(1u, halfHeight / 2u);
    const uint32 eighthWidth = max(1u, quarterWidth / 2u);
    const uint32 eighthHeight = max(1u, quarterHeight / 2u);
    constexpr DXGI_FORMAT bloomFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    const Vec4 clearColor{ 0.f, 0.f, 0.f, 1.f };

    auto ready_target = [&](Shared<RenderTarget>& target, const wchar_t* suffix, uint32 width, uint32 height)
    {
        if (target != nullptr)
            return true;

        const wstring tag = Make_TargetTag(_bloomTargetPrefix, suffix);
        target = Create_PreviewRenderTarget(tag.c_str(), width, height, bloomFormat, clearColor, false);
        return target != nullptr;
    };

    if (!ready_target(_bloomHalfA, L"HalfA", halfWidth, halfHeight) ||
        !ready_target(_bloomHalfB, L"HalfB", halfWidth, halfHeight) ||
        !ready_target(_bloomQuarterA, L"QuarterA", quarterWidth, quarterHeight) ||
        !ready_target(_bloomQuarterB, L"QuarterB", quarterWidth, quarterHeight) ||
        !ready_target(_bloomEighthA, L"EighthA", eighthWidth, eighthHeight) ||
        !ready_target(_bloomEighthB, L"EighthB", eighthWidth, eighthHeight))
    {
        outStatusMessage = "Preview postprocess render target creation failed.";
        return false;
    }

    return true;
}

bool EffectPreviewPostProcessRenderer::Ready_PostProcessShader(string& outStatusMessage)
{
    if (_postProcessShader != nullptr)
        return true;

    _postProcessShader = ShaderCom::Create(
        EDITOR->Get_Device(),
        EDITOR->Get_Context(),
        kPostProcessShaderPath,
        VTXTEX::Elements,
        VTXTEX::numElements,
        ShaderType::PostProcess,
        false
    );

    if (_postProcessShader == nullptr)
    {
        outStatusMessage = "Shader_PostProcess creation failed.";
        return false;
    }

    return true;
}

bool EffectPreviewPostProcessRenderer::Ready_PostProcessRect(string& outStatusMessage)
{
    if (_postProcessRect != nullptr)
        return true;

    _postProcessRect = VIBuffer_Rect::Create(EDITOR->Get_Device(), EDITOR->Get_Context());
    if (_postProcessRect == nullptr)
    {
        outStatusMessage = "Preview postprocess rect creation failed.";
        return false;
    }

    return true;
}

HRESULT EffectPreviewPostProcessRenderer::Render_PostProcessResolve() const
{
    CHECK_NULL(_hdrTarget, E_FAIL);
    CHECK_NULL(_displayTarget, E_FAIL);
    CHECK_NULL(_postProcessShader, E_FAIL);
    CHECK_NULL(_postProcessRect, E_FAIL);

    const EffectEditorPostProcessOptions options = EDITOR->Get_PostProcessOptions();
    const bool useHDRPostProcess =
        options.features != PostProcessFeature::None &&
        Has_Flag(options.features, PostProcessFeature::HDR);
    const bool useBloom =
        useHDRPostProcess &&
        Has_Flag(options.features, PostProcessFeature::Bloom);

    if (!useHDRPostProcess)
        return Render_PostProcessCopy();

    if (useBloom)
    {
        CHECK_FAILED(Render_BloomPass(_bloomHalfA, _hdrTarget, PostProcessPass::BloomExtractHalf), E_FAIL);
        CHECK_FAILED(Render_BloomPass(_bloomQuarterA, _bloomHalfA, PostProcessPass::BloomDownsample), E_FAIL);
        CHECK_FAILED(Render_BloomPass(_bloomEighthA, _bloomQuarterA, PostProcessPass::BloomDownsample), E_FAIL);

        CHECK_FAILED(Render_BloomPass(_bloomEighthB, _bloomEighthA, PostProcessPass::BloomBlurH), E_FAIL);
        CHECK_FAILED(Render_BloomPass(_bloomEighthA, _bloomEighthB, PostProcessPass::BloomBlurV), E_FAIL);

        CHECK_FAILED(Render_BloomPass(_bloomQuarterB, _bloomQuarterA, PostProcessPass::BloomBlurH), E_FAIL);
        CHECK_FAILED(Render_BloomPass(_bloomQuarterA, _bloomQuarterB, PostProcessPass::BloomBlurV), E_FAIL);

        CHECK_FAILED(Render_BloomPass(_bloomHalfB, _bloomHalfA, PostProcessPass::BloomBlurH), E_FAIL);
        CHECK_FAILED(Render_BloomPass(_bloomHalfA, _bloomHalfB, PostProcessPass::BloomBlurV), E_FAIL);

        CHECK_FAILED(Render_BloomUpsamplePass(_bloomQuarterB, _bloomEighthA, _bloomQuarterA), E_FAIL);
        CHECK_FAILED(Render_BloomUpsamplePass(_bloomHalfB, _bloomQuarterB, _bloomHalfA), E_FAIL);
    }

    return Render_PostProcessToneMapping(useBloom ? _bloomHalfB : nullptr);
}

HRESULT EffectPreviewPostProcessRenderer::Render_PostProcessToneMapping(const Shared<RenderTarget>& bloomTexture) const
{
    CHECK_NULL(_hdrTarget, E_FAIL);
    CHECK_NULL(_displayTarget, E_FAIL);
    CHECK_NULL(_postProcessShader, E_FAIL);
    CHECK_NULL(_postProcessRect, E_FAIL);

    ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    EDITOR->Get_Context()->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

    CHECK_FAILED(_displayTarget->Begin(true, false), E_FAIL);
    CHECK_FAILED(Bind_PostProcessCommon(_width, _height), E_FAIL);

    PostProcessCB params = EDITOR->Get_PostProcessOptions().params;
    params.featureMask = To_Underlying(EDITOR->Get_PostProcessOptions().features);
    if (bloomTexture == nullptr)
        params.featureMask &= ~To_Underlying(PostProcessFeature::Bloom);

    CHECK_FAILED(_postProcessShader->Bind_CBufferData(params), E_FAIL);
    CHECK_FAILED(_hdrTarget->Bind_ShaderResource(_postProcessShader.get(), "g_HDRTexture"), E_FAIL);

    if (bloomTexture != nullptr)
        CHECK_FAILED(bloomTexture->Bind_ShaderResource(_postProcessShader.get(), "g_BloomTexture"), E_FAIL);
    else
        CHECK_FAILED(_postProcessShader->Bind_SRV("g_BloomTexture", nullptr), E_FAIL);

    CHECK_FAILED(_postProcessShader->Bind_SRV("g_BackgroundMaskTexture", nullptr), E_FAIL);
    CHECK_FAILED(_postProcessShader->Begin(ETOI(PostProcessPass::ToneMapping)), E_FAIL);
    CHECK_FAILED(_postProcessRect->Render(), E_FAIL);

    return S_OK;
}

HRESULT EffectPreviewPostProcessRenderer::Render_PostProcessCopy() const
{
    CHECK_NULL(_hdrTarget, E_FAIL);
    CHECK_NULL(_displayTarget, E_FAIL);
    CHECK_NULL(_postProcessShader, E_FAIL);
    CHECK_NULL(_postProcessRect, E_FAIL);

    ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    EDITOR->Get_Context()->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

    CHECK_FAILED(_displayTarget->Begin(true, false), E_FAIL);
    CHECK_FAILED(Bind_PostProcessCommon(_width, _height), E_FAIL);

    PostProcessCB params{};
    params.featureMask = 0u;
    CHECK_FAILED(_postProcessShader->Bind_CBufferData(params), E_FAIL);
    CHECK_FAILED(_hdrTarget->Bind_ShaderResource(_postProcessShader.get(), "g_HDRTexture"), E_FAIL);
    CHECK_FAILED(_postProcessShader->Bind_SRV("g_BloomTexture", nullptr), E_FAIL);
    CHECK_FAILED(_postProcessShader->Bind_SRV("g_BackgroundMaskTexture", nullptr), E_FAIL);
    CHECK_FAILED(_postProcessShader->Begin(ETOI(PostProcessPass::ToneMapping)), E_FAIL);
    CHECK_FAILED(_postProcessRect->Render(), E_FAIL);

    return S_OK;
}

HRESULT EffectPreviewPostProcessRenderer::Render_BloomPass(
    const Shared<RenderTarget>& dstTarget,
    const Shared<RenderTarget>& srcTarget,
    PostProcessPass pass) const
{
    CHECK_NULL(dstTarget, E_FAIL);
    CHECK_NULL(srcTarget, E_FAIL);
    CHECK_NULL(_postProcessShader, E_FAIL);
    CHECK_NULL(_postProcessRect, E_FAIL);

    ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    EDITOR->Get_Context()->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

    CHECK_FAILED(dstTarget->Begin(true, false), E_FAIL);
    CHECK_FAILED(Bind_PostProcessCommon(srcTarget->Get_Width(), srcTarget->Get_Height()), E_FAIL);
    CHECK_FAILED(_postProcessShader->Bind_CBufferData(EDITOR->Get_PostProcessOptions().params), E_FAIL);

    const char* sourceName = pass == PostProcessPass::BloomExtractHalf ? "g_HDRTexture" : "g_BloomTexture";
    CHECK_FAILED(srcTarget->Bind_ShaderResource(_postProcessShader.get(), sourceName), E_FAIL);
    CHECK_FAILED(_postProcessShader->Begin(ETOI(pass)), E_FAIL);
    CHECK_FAILED(_postProcessRect->Render(), E_FAIL);

    return S_OK;
}

HRESULT EffectPreviewPostProcessRenderer::Render_BloomUpsamplePass(
    const Shared<RenderTarget>& dstTarget,
    const Shared<RenderTarget>& smallTarget,
    const Shared<RenderTarget>& baseTarget) const
{
    CHECK_NULL(dstTarget, E_FAIL);
    CHECK_NULL(smallTarget, E_FAIL);
    CHECK_NULL(baseTarget, E_FAIL);
    CHECK_NULL(_postProcessShader, E_FAIL);
    CHECK_NULL(_postProcessRect, E_FAIL);

    ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    EDITOR->Get_Context()->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

    CHECK_FAILED(dstTarget->Begin(true, false), E_FAIL);
    CHECK_FAILED(Bind_PostProcessCommon(dstTarget->Get_Width(), dstTarget->Get_Height()), E_FAIL);
    CHECK_FAILED(_postProcessShader->Bind_CBufferData(EDITOR->Get_PostProcessOptions().params), E_FAIL);
    CHECK_FAILED(smallTarget->Bind_ShaderResource(_postProcessShader.get(), "g_BloomTexture"), E_FAIL);
    CHECK_FAILED(baseTarget->Bind_ShaderResource(_postProcessShader.get(), "g_BloomBaseTexture"), E_FAIL);
    CHECK_FAILED(_postProcessShader->Begin(ETOI(PostProcessPass::BloomUpsampleAdd)), E_FAIL);
    CHECK_FAILED(_postProcessRect->Render(), E_FAIL);

    return S_OK;
}

HRESULT EffectPreviewPostProcessRenderer::Bind_PostProcessCommon(uint32 width, uint32 height) const
{
    CHECK_NULL(_postProcessShader, E_FAIL);
    CHECK_NULL(_postProcessRect, E_FAIL);

    const uint32 resolvedWidth = max(1u, width);
    const uint32 resolvedHeight = max(1u, height);
    const Matrix worldMatrix = Matrix::CreateScale(
        static_cast<float>(resolvedWidth),
        static_cast<float>(resolvedHeight),
        1.f
    );
    ScreenSpaceCB screenSpaceCB{};
    screenSpaceCB.screenViewMatrix = Matrix::Identity;
    screenSpaceCB.screenProjMatrix = XMMatrixOrthographicLH(
        static_cast<float>(resolvedWidth),
        static_cast<float>(resolvedHeight),
        0.f,
        1.f
    );
    screenSpaceCB.viewInverseMatrix = Matrix::Identity;
    screenSpaceCB.projInverseMatrix = Matrix::Identity;
    screenSpaceCB.cameraPosition = Vec4(0.f, 0.f, 0.f, 1.f);
    screenSpaceCB.farPlane = 100.f;
    screenSpaceCB.texelSize = Vec2(
        1.f / static_cast<float>(resolvedWidth),
        1.f / static_cast<float>(resolvedHeight)
    );
    screenSpaceCB.screenSize = Vec2(
        static_cast<float>(resolvedWidth),
        static_cast<float>(resolvedHeight)
    );

    CHECK_FAILED(_postProcessShader->Bind_ObjectCB(worldMatrix), E_FAIL);
    CHECK_FAILED(_postProcessShader->Bind_CBufferData(screenSpaceCB), E_FAIL);
    CHECK_FAILED(_postProcessRect->Bind_Resources(), E_FAIL);

    return S_OK;
}

void EffectPreviewPostProcessRenderer::Cleanup()
{
    _displayTarget.reset();
    _hdrTarget.reset();
    _bloomHalfA.reset();
    _bloomHalfB.reset();
    _bloomQuarterA.reset();
    _bloomQuarterB.reset();
    _bloomEighthA.reset();
    _bloomEighthB.reset();
    _postProcessShader.reset();
    _postProcessRect.reset();
}

NS_END
