#pragma once
#include "AnimNotify.h"
NS_BEGIN(Engine)
class GameObject;
NS_END
NS_BEGIN(Client)

class AN_SpawnLineGroundEffect :
    public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_SpawnLineGroundEffect)

public:
    string Get_TypeName() const override { return "AN_SpawnLineGroundEffect"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    wstring                     _effectName = L"";
    uint32                      _segmentIndex = 1; // 1, 2, 3
    uint32                      _segmentNum = 4; // 몇등분 할지
    wstring                     _layerTag = L"Layer_Effect";
    Vec3                        _localOffset = Vec3::Zero;
    Vec3                        _localRotation = Vec3::Zero;
    Vec3                        _scale = Vec3::One;
    float                        _yPos = 0.1f;
};


NS_END

