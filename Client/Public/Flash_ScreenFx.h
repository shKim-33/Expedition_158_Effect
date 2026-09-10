#pragma once
#include "ScreenFx_Instance.h"
#include "ScreenFxCueTypes.h"

NS_BEGIN(Client)

class Flash_ScreenFx final : public ScreenFx_Instance
{
    GENERATED_SCREEN_FX(Flash_ScreenFx)

public:
    struct FlashDesc : public ScreenFxDesc
    {
        FlashDesc()
        {
            duration = 0.2f;
        }

        Vec4 color{ 1.f, 1.f, 1.f, 1.f };
        float intensity{ 1.f };
        float fadeInTime{ 0.f };
        float fadeOutTime{ 0.1f };
        ScreenFxShapeMode shapeMode{ ScreenFxShapeMode::FullScreen };
        Vec2 center{ 0.5f, 0.5f };
        float startRadius{ 0.f };
        float endRadius{ 1.f };
        float softness{ 0.05f };
        bool invertShape{ false };
        ScreenFxExclusionMode exclusionMode{ ScreenFxExclusionMode::None };
    };

public:
    Flash_ScreenFx() = default;
    ~Flash_ScreenFx() override = default;

public:
    HRESULT Initialize(ScreenFxDesc* desc) override;
    void Update_ScreenFx(float timeDelta) override;
    HRESULT Bind_FxConstantBuffer(ShaderCom* shader) override;
    void Evaluate_At(float elapsedTime) override;

private:
    FlashDesc _originalDesc{};
    float _currentAmount{ 0.f };
    float _currentRadius{ 1.f };

private:
    float Compute_Envelope(float elapsedTime) const;

public:
    static Shared<Flash_ScreenFx> Create(FlashDesc* desc);
};

NS_END
