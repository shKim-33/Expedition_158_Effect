#pragma once

// 이펙트 랜덤 시드 계산 helper를 모은다.
// Hash01은 HLSL Hash01(Shader_Compute_PointParticle.hlsl)과 Engine VIBufferCom_ComputePointParticle의 Hash01과 상수가 같아야 CPU/GPU 결과가 맞는다.

#include "Client_Defines.h"
#include "EffectRuntime_Types.h"

namespace Client::EffectSeed
{
// 시드 설정과 effect 재생 시드로 최종 salt를 만든다. 수동 시드가 꺼져 있으면 0에서 시작하고, 인스턴스 시드를 쓰면 재생 시드를 더한다.
inline uint32 Resolve_SeedSalt(const PointParticleRandomSeedRuntimeDesc& seed, uint32 effectPlaybackSeed)
{
    uint32 salt = seed.manualSeedEnabled ? seed.seed : 0u;
    if (seed.useInstanceSeed)
        salt += effectPlaybackSeed;
    return salt;
}

// uint32 시드를 0~1 사이 pseudo-random float으로 바꾼다.
inline float Hash01(uint32 seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return static_cast<float>(seed & 0x00FFFFFFu) / 16777215.f;
}
}
