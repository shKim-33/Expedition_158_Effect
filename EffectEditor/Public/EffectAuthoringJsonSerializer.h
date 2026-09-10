#pragma once
#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

class EffectAuthoringJsonSerializer final
{
public:
    static json To_Json(const EffectAuthoringDocument& document);
    static bool From_Json(const json& root, EffectAuthoringDocument& outDocument);
    static uint32 Find_NextAuthoringId(const EffectAuthoringDocument& document);

private: //## Static::Serialization
    static constexpr int kEffectAuthoringJsonVersion{ 2 };

private: //## Helper::AuthoringAggregate
    static json To_Json(const AuthoringModule& module);
    static json To_Json(const AuthoringEmitter& emitter);
    static bool From_Json(const json& root, AuthoringModule& outModule);
    static bool From_Json(const json& root, AuthoringEmitter& outEmitter);
    static uint32 Find_NextAuthoringId(const vector<AuthoringEmitter>& emitters);
    static AuthoringModuleData Make_DefaultModuleDataForRestore(AuthoringModuleType type);
};

NS_END
