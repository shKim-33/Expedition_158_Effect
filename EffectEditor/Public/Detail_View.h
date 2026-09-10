#pragma once
#include "Editor_Window.h"

#include "DetailPropertyContext.h"
#include "EffectAuthoring_Types.h"
#include "Emitter_View.h"
#include "ModuleDetail_Drawers.h"

NS_BEGIN(EffectEditor)

class Detail_View final : public Editor_Window
{
public:
    Detail_View();
    ~Detail_View() override;

public:
    void Update(float timeDelta) override;
    void Render() override;

private: //## Data::ModuleDetail
    DetailPropertyContext _detailPropertyContext{};
    float _detailTimeDelta{};

    RequiredModuleDetail _requiredModuleDetail{};
    SpawnModuleDetail _spawnModuleDetail{};
    LifetimeModuleDetail _lifetimeModuleDetail{};
    InitialLocationModuleDetail _initialLocationModuleDetail{};
    RibbonOrientationModuleDetail _ribbonOrientationModuleDetail{};
    SphereLocationModuleDetail _sphereLocationModuleDetail{};
    PlaneRadialLocationModuleDetail _planeRadialLocationModuleDetail{};
    CylinderLocationModuleDetail _cylinderLocationModuleDetail{};
    InitialSizeModuleDetail _initialSizeModuleDetail{};
    InitialMeshSizeModuleDetail _initialMeshSizeModuleDetail{};
    InitialVelocityModuleDetail _initialVelocityModuleDetail{};
    InitialRadialVelocityModuleDetail _initialRadialVelocityModuleDetail{};
    VelocityConeModuleDetail _velocityConeModuleDetail{};
    SourceMotionVelocityModuleDetail _sourceMotionVelocityModuleDetail{};
    AccelerationModuleDetail _accelerationModuleDetail{};
    DragModuleDetail _dragModuleDetail{};
    VelocityOverLifeModuleDetail _velocityOverLifeModuleDetail{};
    OrbitOverLifeModuleDetail _orbitOverLifeModuleDetail{};
    InitialRotationModuleDetail _initialRotationModuleDetail{};
    SphereRadialOrientationModuleDetail _sphereRadialOrientationModuleDetail{};
    PlaneRadialOrientationModuleDetail _planeRadialOrientationModuleDetail{};
    CylinderOrientationModuleDetail _cylinderOrientationModuleDetail{};
    RotationOverLifeModuleDetail _rotationOverLifeModuleDetail{};
    SpriteTiltModuleDetail _spriteTiltModuleDetail{};
    SpriteTiltOverLifeModuleDetail _spriteTiltOverLifeModuleDetail{};
    InitialRotationRateModuleDetail _initialRotationRateModuleDetail{};
    RotationRateScaleByLifeModuleDetail _rotationRateScaleByLifeModuleDetail{};
    InitialMeshRotationModuleDetail _initialMeshRotationModuleDetail{};
    MeshRotationOverLifeModuleDetail _meshRotationOverLifeModuleDetail{};
    MeshDirectionAlignOverLifeModuleDetail _meshDirectionAlignOverLifeModuleDetail{};
    InitialMeshRotationRateModuleDetail _initialMeshRotationRateModuleDetail{};
    MeshRotationRateScaleByLifeModuleDetail _meshRotationRateScaleByLifeModuleDetail{};
    InitialColorModuleDetail _initialColorModuleDetail{};
    ColorOverLifeModuleDetail _colorOverLifeModuleDetail{};
    SubUVFrameOverLifeModuleDetail _subUvFrameOverLifeModuleDetail{};
    SizeByLifeModuleDetail _sizeByLifeModuleDetail{};
    BeamEnvelopeOverLifeModuleDetail _beamEnvelopeOverLifeModuleDetail{};
    MeshSizeByLifeModuleDetail _meshSizeByLifeModuleDetail{};
    SpawnPerUnitModuleDetail _spawnPerUnitModuleDetail{};
    SourceHistorySpriteTrailPathFollowModuleDetail _sourceHistorySpriteTrailPathFollowModuleDetail{};
    SourceHistorySpriteTrailPathReplayModuleDetail _sourceHistorySpriteTrailPathReplayModuleDetail{};
    MaterialScalarModulationModuleDetail _materialScalarModulationModuleDetail{};

private: //## Data::AuthoringHistory
    bool _hasPendingAuthoringEdit{ false };
    EffectAuthoringSelection _pendingAuthoringSelection{};
    Emitter_View::AuthoringSnapshot _pendingAuthoringSnapshot{};
    string _pendingAuthoringDescription{};

private: //## Helper::EffectAuthoring
    void Render_EffectAuthoringDetail();
    void Render_ParticleSystemDetail(
        const Shared<Emitter_View>& emitterView,
        const EffectAuthoringSelection& selection,
        ParticleSystemAuthoringData& data);
    void Render_TrailPreviewDetail();
    void Render_PostProcessShaderDetail();
    void Render_EmitterDetail(
        const Shared<Emitter_View>& emitterView,
        const EffectAuthoringSelection& selection,
        AuthoringEmitter& emitter);
    void Render_TypeDataDetail(
        const Shared<Emitter_View>& emitterView,
        const EffectAuthoringSelection& selection,
        AuthoringEmitter& emitter);
    void Render_ModuleDetail(
        const Shared<Emitter_View>& emitterView,
        const EffectAuthoringSelection& selection,
        AuthoringEmitter& emitter,
        AuthoringModule& module);

    void Mark_Changed(const Shared<Emitter_View>& emitterView, AuthoringEmitter& emitter, bool changed);
    void Commit_PendingAuthoringEditOnTargetChange(const Shared<Emitter_View>& emitterView, const EffectAuthoringSelection& selection);
    void Begin_PendingAuthoringEdit(
        const Shared<Emitter_View>& emitterView,
        const EffectAuthoringSelection& selection,
        const Emitter_View::AuthoringSnapshot& beforeSnapshot,
        const string& description);
    void Commit_PendingAuthoringEditIfIdle(const Shared<Emitter_View>& emitterView);
    void Commit_PendingAuthoringEdit(const Shared<Emitter_View>& emitterView);
    static bool Is_SameSelection(const EffectAuthoringSelection& lhs, const EffectAuthoringSelection& rhs);

public:
    static Shared<Detail_View> Create();
    void Free() override;
};

NS_END
