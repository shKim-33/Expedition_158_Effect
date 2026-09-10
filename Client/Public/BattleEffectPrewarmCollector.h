#pragma once

#include "BattleEffectPrewarmTypes.h"
#include "BattleMonsterSpawnDesc.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class BattleEffectPrewarmCollector final
{
public:
    static vector<EffectPrewarmRequest> Collect_ForBattleSetup(const vector<HeroId>& heroIds, const vector<BattleMonsterSpawnDesc>& monsterSpawnDescs);
};

NS_END
