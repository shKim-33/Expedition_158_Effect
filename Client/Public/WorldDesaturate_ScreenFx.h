#pragma once
#include "ScreenFx_Instance.h"
#include "ScreenFxCueTypes.h"

NS_BEGIN(Client)

class WorldDesaturate_ScreenFx final : public ScreenFx_Instance
{
    GENERATED_SCREEN_FX(WorldDesaturate_ScreenFx)

public:
    struct DesaturateDesc : public ScreenFxDesc
    {
        DesaturateDesc()
        {
            duration = 0.35f;
        }

        float amount{ 1.f };
        float fadeInTime{ 0.05f };
        float fadeOutTime{ 0.15f };
        ScreenFxShapeMode shapeMode{ ScreenFxShapeMode::FullScreen };
        Vec2 center{ 0.5f, 0.5f };
        float startRadius{ 0.f };
        float endRadius{ 1.f };
        float softness{ 0.05f };
        bool invertShape{ false };
        ScreenFxExclusionMode exclusionMode{ ScreenFxExclusionMode::None };
    };

public:
    WorldDesaturate_ScreenFx() = default;
    ~WorldDesaturate_ScreenFx() override = default;

public:
    HRESULT Initialize(ScreenFxDesc* desc) override;
    void Update_ScreenFx(float timeDelta) override;
    HRESULT Bind_FxConstantBuffer(ShaderCom* shader) override;
    void Evaluate_At(float elapsedTime) override;

private:
    DesaturateDesc _originalDesc{};
    float _currentAmount{ 0.f };
    float _currentRadius{ 1.f };

private:
    float Compute_Envelope(float elapsedTime) const;

public:
    static Shared<WorldDesaturate_ScreenFx> Create(DesaturateDesc* desc);
};

NS_END
