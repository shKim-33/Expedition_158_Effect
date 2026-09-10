#pragma once

#include "DetailPropertyContext.h"
#include "EffectAuthoring_Types.h"
#include "EffectMaterialPreviewRenderer.h"

NS_BEGIN(EffectEditor)

class RequiredModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        RequiredModuleData& data,
        DetailPropertyContext& detailContext,
        float timeDelta);

private:
    Unique<EffectMaterialPreviewRenderer> _materialSlotPreview{};
};

class SpawnModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        SpawnModuleData& data,
        DetailPropertyContext& detailContext);
};

class LifetimeModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        const AuthoringModule& module,
        LifetimeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialSizeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialSizeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialMeshSizeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialMeshSizeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialLocationModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialLocationModuleData& data,
        DetailPropertyContext& detailContext);
};

class RibbonOrientationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        RibbonOrientationModuleData& data,
        DetailPropertyContext& detailContext);
};

class SphereLocationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        SphereLocationModuleData& data,
        DetailPropertyContext& detailContext);
};

class PlaneRadialLocationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        PlaneRadialLocationModuleData& data,
        DetailPropertyContext& detailContext);
};

class CylinderLocationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        CylinderLocationModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialVelocityModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialVelocityModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialRadialVelocityModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialRadialVelocityModuleData& data,
        DetailPropertyContext& detailContext);
};

class VelocityConeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        VelocityConeModuleData& data,
        DetailPropertyContext& detailContext);
};

class SourceMotionVelocityModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        SourceMotionVelocityModuleData& data,
        DetailPropertyContext& detailContext);
};

class AccelerationModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        AccelerationModuleData& data,
        DetailPropertyContext& detailContext);
};

class DragModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        DragModuleData& data,
        DetailPropertyContext& detailContext);
};

class VelocityOverLifeModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        const AuthoringModule& module,
        VelocityOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialRotationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialRotationModuleData& data,
        DetailPropertyContext& detailContext);
};

class OrbitOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        OrbitOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class PlaneRadialOrientationModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        PlaneRadialOrientationModuleData& data,
        DetailPropertyContext& detailContext);
};

class CylinderOrientationModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        CylinderOrientationModuleData& data,
        DetailPropertyContext& detailContext);
};

class SphereRadialOrientationModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        SphereRadialOrientationModuleData& data,
        DetailPropertyContext& detailContext);
};

class RotationOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        RotationOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class SpriteTiltModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SpriteTiltModuleData& data,
        DetailPropertyContext& detailContext);
};

class SpriteTiltOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SpriteTiltOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialRotationRateModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialRotationRateModuleData& data,
        DetailPropertyContext& detailContext);
};

class RotationRateScaleByLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        RotationRateScaleByLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialMeshRotationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialMeshRotationModuleData& data,
        DetailPropertyContext& detailContext);
};

class MeshRotationOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        MeshRotationOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class MeshDirectionAlignOverLifeModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        MeshDirectionAlignOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialMeshRotationRateModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialMeshRotationRateModuleData& data,
        DetailPropertyContext& detailContext);
};

class MeshRotationRateScaleByLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        MeshRotationRateScaleByLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialColorModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        InitialColorModuleData& data,
        DetailPropertyContext& detailContext);
};

class ColorOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        ColorOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class SubUVFrameOverLifeModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        SubUVFrameOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class SizeByLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SizeByLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class BeamEnvelopeOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        BeamEnvelopeOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class MeshSizeByLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        MeshSizeByLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class SpawnPerUnitModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SpawnPerUnitModuleData& data,
        DetailPropertyContext& detailContext);
};

class SourceHistorySpriteTrailPathFollowModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SourceHistorySpriteTrailPathFollowModuleData& data,
        DetailPropertyContext& detailContext);
};

class SourceHistorySpriteTrailPathReplayModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SourceHistorySpriteTrailPathReplayModuleData& data,
        DetailPropertyContext& detailContext);
};

class MaterialScalarModulationModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        MaterialScalarModulationModuleData& data,
        DetailPropertyContext& detailContext);
};

NS_END
