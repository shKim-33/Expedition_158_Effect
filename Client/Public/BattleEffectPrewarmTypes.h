#pragma once

#include "Client_Defines.h"

NS_BEGIN(Client)

struct EffectPrewarmRequest
{
    wstring effectName{};
    wstring layerTag{ L"Layer_Effect" };
    uint32 targetCount{ 1 };
    uint32 targetLevelIndex{ UINT32_MAX };
};

struct BattleEffectPrewarmContext
{
    uint32 currentHeroCount{};
    uint32 currentEnemyCount{};
};

NS_END
