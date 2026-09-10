#pragma once

#include "Client_Defines.h"

NS_BEGIN(Client)

// SwitchDissolve Feature constants
// flag/texture slot 값 자체는 shader feature 계약이라 특정 컴포넌트 내부로 이동은 안시키겠습니다
inline constexpr uint32 DrawFeatureFlagDissolve = (1u << 0);    // 이번 draw 에서 디졸브 기능을 켤지 여부. 지원 여부와는 별개임.
inline constexpr uint32 DrawFeatureFlagDistortion = (1u << 1);  // 이번 draw 에서 왜곡 기능을 켤지 여부. 지원 여부와는 별개임.
inline constexpr uint32 DrawFeatureTexDissolveNoise = 0u;       // dissolve 기능에서 사용할 노이즈 텍스처가 바인딩될 슬롯 인덱스. DrawFeatureCB의 textureSlotMask에 이 인덱스 비트를 켜서 실제로 바인딩된 텍스처를 알린다.

struct SwitchDissolveRuntime
{
    bool enabled = false;
    bool reveal = true;                     // true: reveal 모드, false: vanish 모드
    float progress = 0.f;                   // 0.f ~ 1.f
    float duration = 0.45f;                 // 디졸브 지속 시간
    uint32 noiseHandle = InvalidSRVHandle;  // 노이즈 텍스처 핸들
    float edgeWidth = 0.08f;                // 가장자리 너비
    float edgeSoftness = 0.02f;             // 가장자리 블러/부드러움
    Vec3 edgeColor = Vec3(0.25f, 0.25f, 0.25f); // 사라지기 직전(경계) 색

    void Start(float durationSec, uint32 noise, bool isReveal, float width, float softness, const Vec3& color)
    {
        enabled = true;
        reveal = isReveal;
        progress = 0.f;
        duration = fmaxf(durationSec, 0.0001f);
        noiseHandle = noise;
        edgeWidth = width;
        edgeSoftness = softness;
        edgeColor = color;
    }

    void Start_Reveal(float durationSec, uint32 noise, float width, float softness, const Vec3& color)
    {
        Start(durationSec, noise, true, width, softness, color);
    }

    void Start_Vanish(float durationSec, uint32 noise, float width, float softness, const Vec3& color)
    {
        Start(durationSec, noise, false, width, softness, color);
    }

    void Stop()
    {
        enabled = false;
        progress = 1.f;
    }

    void Update(float dt)
    {
        if (!enabled)
            return;

        progress += dt / std::fmaxf(duration, 0.0001f);
        if (progress >= 1.f)
        {
            progress = 1.f;
            enabled = false;
        }
    }
};

NS_END
