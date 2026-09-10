#pragma once
#include "EffectAssetRuntimeLoader.h"
#include "EffectRuntime_Lowering.h"

namespace Engine
{
struct PointParticleCylinderLocationDesc;
struct PointParticleSpriteTiltDesc;
}

NS_BEGIN(Client)

namespace EffectAssetRuntimeLoad
{
    namespace Json
    {
        //## JSON::PrimitiveRead

        inline constexpr uint32 kMaxSubUVFrameCurveKeys{ kEffectDistributionCurveMaxKeys };

        string To_LogPath(const fs::path& path);
        string Read_String(const json& node, const char* key, const string& fallbackValue = {});
        bool Read_Bool(const json& node, const char* key, bool fallbackValue = false);
        uint32 Read_UInt(const json& node, const char* key, uint32 fallbackValue = 0u);
        int32 Read_Int(const json& node, const char* key, int32 fallbackValue = 0);
        float Read_Float(const json& node, const char* key, float fallbackValue = 0.f);
        void Read_Vec2(const json& node, Vec2& outValue);
        void Read_Vec3(const json& node, Vec3& outValue);
        void Read_Vec4(const json& node, Vec4& outValue);
        void Read_Color(const json& node, Color& outValue);

        template <typename EnumType>
        void Read_Enum(const json& node, const char* key, EnumType& outValue)
        {
            const string token = Read_String(node, key);
            if (token.empty())
                return;

            const optional<EnumType> parsed = magic_enum::enum_cast<EnumType>(token);
            if (parsed.has_value())
                outValue = parsed.value();
        }

        //## JSON::PolicyRead

        EffectTextureUVTilingMode Resolve_CompatibleUVTilingMode(const EffectMaterialUVAxisPolicy& policy);
        void Read_UVAxisPolicy(
            const json& node,
            const char* key,
            EffectTextureUVTilingMode legacyMode,
            EffectMaterialUVAxisPolicy& outPolicy,
            EffectTextureUVTilingMode& outCompatibleMode);
        EffectSortPolicy Read_SortPolicy(const json& node, EffectSortPolicy fallbackValue);
        void Apply_SortPayload(const json& data, EffectRequiredSortRuntimeDesc& sortDesc);
        PointParticleRandomSeedRuntimeDesc Read_RandomSeedDesc(const json& moduleData);
        PointParticleRandomSeedRuntimeDesc Read_DistributionRandomSeedDesc(const json& distribution, const json* legacyModuleData = nullptr);
    }

    namespace Curves
    {
        //## Curve::Evaluation

        float Evaluate_FloatCurveSegmentLinear(const json& leftKey, const json& rightKey, float x);
        float Evaluate_FloatCurveSegmentAutoClamped(const json& leftKey, const json& rightKey, float x);
        float Evaluate_FloatConstantCurve(const json& keys, float x, float fallbackValue);
        Vec2 Evaluate_Vector2CurveSegmentLinear(const json& leftKey, const json& rightKey, float x);
        Vec2 Evaluate_Vector2CurveSegmentAutoClamped(const json& leftKey, const json& rightKey, float x);
        Vec2 Evaluate_Vector2ConstantCurve(const json& keys, float x, const Vec2& fallbackValue);
        Vec3 Evaluate_Vector3ConstantCurve(const json& keys, float x, const Vec3& fallbackValue);
        Vec4 Evaluate_ColorConstantCurve(const json& keys, float x, const Vec4& fallbackValue);

        //## Distribution::Evaluation

        const json* Find_DistributionPayload(const json& distribution);
        float Evaluate_FloatDistributionMin(const json& distribution, float fallbackValue);
        float Evaluate_FloatDistributionMax(const json& distribution, float fallbackValue);
        Vec2 Evaluate_Vector2DistributionMin(const json& distribution, const Vec2& fallbackValue);
        Vec2 Evaluate_Vector2DistributionMax(const json& distribution, const Vec2& fallbackValue);
        Vec3 Evaluate_Vector3DistributionMin(const json& distribution, const Vec3& fallbackValue);
        Vec3 Evaluate_Vector3DistributionMax(const json& distribution, const Vec3& fallbackValue);
        Vec4 Clamp_Color(Vec4 value);
        Vec4 Evaluate_ColorDistributionMin(const json& distribution, const Vec4& fallbackValue);
        Vec4 Evaluate_ColorDistributionMax(const json& distribution, const Vec4& fallbackValue);
        Vec4 Evaluate_ColorOverLifeEndpointColorMin(const json& distribution, const Vec4& fallbackValue);
        Vec4 Evaluate_ColorOverLifeEndpointColorMax(const json& distribution, const Vec4& fallbackValue);
        float Evaluate_ColorOverLifeEndpointAlphaMin(const json& distribution, float fallbackValue);
        float Evaluate_ColorOverLifeEndpointAlphaMax(const json& distribution, float fallbackValue);
        float Resolve_CurveInterpolationMode(const json& key);

        //## Runtime::CurvePayloadFill

        void Fill_ColorOverLifeCurvePayload(PointParticleColorOverLifeCurveDesc& desc, const json& colorOverLife);
        void Set_SubUVFrameCurveComponent(Vec4& values, uint32 index, float value);
        void Set_CurveComponent(Vec4& valuesBlock0, Vec4& valuesBlock1, uint32 index, float value);
        void Fill_FloatCurvePayload(PointParticleFloatCurveRuntimeDesc& desc, const json& distribution, float fallbackValue);
        void Fill_SubUVFrameCurvePayload(PointParticleSubUVFrameOverLifeDesc& desc, const json& frameIndex);
        void Fill_SizeByLifePayload(PointParticleSizeByLifeDesc& desc, const json& sizeByLife);
        void Fill_BeamEnvelopeOverLifePayload(EffectBeamEnvelopeOverLifeRuntimeDesc& desc, const json& beamEnvelopeOverLife);
        void Fill_SpriteTiltPayload(PointParticleSpriteTiltDesc& desc, const json* spriteTilt, const json* spriteTiltOverLife);
        void Fill_RotationOverLifePayload(PointParticleRotationDesc& desc, const json& rotationOverLife);
        void Fill_RotationRateScaleByLifePayload(PointParticleRotationDesc& desc, const json& rotationRateScaleByLife);
        void Fill_VelocityScaleByLifePayload(PointParticleMotionDesc& desc, const json& velocityOverLife);
        void Fill_VelocityScaleByLifeChannelPayload(PointParticleFloatCurveRuntimeDesc& desc, const json& velocityOverLife);
        void Fill_VelocityConePayload(PointParticleMotionDesc& desc, const json& velocityCone);
        void Fill_AccelerationCurvePayload(PointParticleMotionDesc& desc, const json& distribution);
        void Fill_OrbitOverLifePayload(EffectOrbitOverLifeRuntimeDesc& desc, const json& orbitOverLife);
        void Fill_PlaneRadialLocationPayload(PointParticlePlaneRadialLocationDesc& desc, const json& planeRadialLocation);
        void Fill_CylinderLocationPayload(PointParticleCylinderLocationDesc& desc, const json& cylinderLocation);
        void Fill_SphereRadialOrientationPayload(PointParticleSphereRadialOrientationDesc& desc, const json& sphereRadialOrientation);
        void Fill_PlaneRadialOrientationPayload(PointParticlePlaneRadialOrientationDesc& desc, const json& planeRadialOrientation);
        void Fill_CylinderOrientationPayload(PointParticleCylinderOrientationDesc& desc, const json& cylinderOrientation);
        void Fill_MeshVector3CurvePayload(MeshVector3CurveRuntimeDesc& desc, const json& distribution, const Vec3& fallbackValue);
    }

    namespace Material
    {
        //## Runtime::MaterialPayloadFill

        void Apply_MaterialPayload(const json& material, EffectRequiredMaterialRuntimeDesc& materialDesc);
        void Fill_MaterialScalarModulationPayload(EffectMaterialScalarModulationRuntimeDesc& desc, const json& materialScalarModulation);
        void Fill_CoreColorRgbModulationPayload(EffectMaterialCoreColorRgbModulationRuntimeDesc& desc, const json& materialScalarModulation);
        void Fill_MaterialVec2ModulationPayload(EffectMaterialVec2ModulationRuntimeDesc& desc, const json& materialScalarModulation);
    }

    namespace Emitters
    {
        //## Emitter::JSONQuery

        const json* Find_ModuleData(const json& emitter, const string& moduleType);

        //## Emitter::ModuleLowering

        string Resolve_RendererType(const json& emitter);
        void Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeSpriteEmitterDesc& desc);
        void Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeTrailEmitterDesc& desc);
        void Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeRibbonEmitterDesc& desc);
        void Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeSourceHistorySpriteTrailEmitterDesc& desc);
        void Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeBeamEmitterDesc& desc);
        void Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, MeshEmitterDesc& desc);
        void Apply_SpawnModule(const json& data, ComputeSpriteEmitterDesc& desc);
        void Apply_SpawnModule(const json& data, MeshEmitterDesc& desc);

        //## Emitter::DefinitionBuild

        bool Build_EmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition);
        bool Build_TrailEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition);
        bool Build_SourceHistoryRibbonEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition);
        bool Build_SourceHistorySpriteTrailEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition);
        bool Build_BeamEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition);
        bool Build_MeshEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition);
        EffectEmitterHistoryBudgetRuntimeDesc Read_HistoryBudgetUsage(const json& emitter);
    }

    namespace Assets
    {
        //## Asset::ResolveAndRootLoad

        bool Try_MakeResourceRelativePath(const fs::path& fullPath, const char* logContext, const string& assetKey, wstring& outRelativePath);
        bool Resolve_EffectAssetPathByGuid(const string& effectAssetGuid, wstring& outEffectAssetPath);
        string Normalize_EffectNameKey(const wstring& effectName);
        bool Is_EffectDefinitionAssetPath(const wstring& relativePath);
        bool Find_EffectAssetGuidByName(const wstring& effectName, string& outEffectAssetGuid);
        bool Load_JsonRoot(const wstring& effectAssetPath, json& outRoot);
    }
}

NS_END
