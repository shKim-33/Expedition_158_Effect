#pragma once
#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

struct AuthoringModuleMetadata
{
    string key{};
    wstring displayName{};
    wstring summary{};
    bool movable{ true };
    bool removable{ true };
    bool supportsSprite{ true };
    bool supportsTrail{ false };
    bool supportsBeam{ false };
};

const AuthoringModuleMetadata& Get_AuthoringModuleMetadata(AuthoringModuleType type);
void Apply_AuthoringModuleMetadata(AuthoringModule& module);

NS_END
