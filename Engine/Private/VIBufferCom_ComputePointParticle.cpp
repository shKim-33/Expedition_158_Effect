#include "VIBufferCom_ComputePointParticle.h"

#include "Camera.h"
#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "GameInstance.h"
#include "TransformCom.h"

struct ComputePointParticleParams
{
    float elapsedTime{};
    float deltaTime{};
    float instanceCount{};
    uint32 drawMode{};
    uint32 spawnRequest{};
    uint32 maxActiveCount{};
    uint32 maxDrawCount{};
    uint32 killActiveParticles{};
    Vec4 center{};
    Vec4 emitterBasisRight{};                        // xyz: emitter local X axis in world space.
    Vec4 emitterBasisUp{};                           // xyz: emitter local Y axis in world space.
    Vec4 emitterBasisLook{};                         // xyz: emitter local Z axis in world space.
    Vec4 spawnRange{};                               // xyz: spawn range.
    Vec4 initialLocationMin{};                       // xyz: InitialLocation min offset, w: enabled.
    Vec4 initialLocationMax{};                       // xyz: InitialLocation max offset, w: unused.
    Vec4 sphereLocationOffsetRadius{};               // xyz: sphere center offset, w: radius.
    Vec4 sphereLocationParams{};                     // x: enabled, y: mode, z: placement mode, w: unused.
    Vec4 planeRadialLocationParams{};                // x: enabled, y: plane, z: shape, w: placement mode.
    Vec4 planeRadialLocationOffsetThickness{};       // xyz: offset, w: thickness.
    Vec4 planeRadialLocationRectRange{};             // x/y: U range, z/w: V range.
    Vec4 planeRadialLocationPolarRange{};            // x/y: radius range, z/w: angle degree range.
    XMUINT4 planeRadialLocationSeedSalts{}; // x: U/radius, y: V/angle, z/w: unused.
    Vec4 planeRadialOrientationParams{};             // x: enabled, y: target kind, z: orientation mode, w: unused.
    Vec4 sizeMin{};                                  // x/y: width/height min.
    Vec4 sizeMax{};                                  // x/y: width/height max.
    Vec4 lifeTime{};                                 // x/y: lifetime min/max.
    Vec4 lifetimeCurveParams{};                      // x: key count, y: enabled, z: sampling phase.
    Vec4 lifetimeCurveTimes{};                       // Lifetime curve key times.
    Vec4 lifetimeCurveTimesBlock1{};
    Vec4 lifetimeCurveValues{};                      // Lifetime curve key values.
    Vec4 lifetimeCurveValuesBlock1{};
    Vec4 lifetimeCurveArriveTangents{};              // Lifetime curve arrive tangents.
    Vec4 lifetimeCurveArriveTangentsBlock1{};
    Vec4 lifetimeCurveLeaveTangents{};               // Lifetime curve leave tangents.
    Vec4 lifetimeCurveLeaveTangentsBlock1{};
    Vec4 lifetimeCurveModes{};                       // Lifetime curve interpolation modes.
    Vec4 lifetimeCurveModesBlock1{};
    Vec4 startColorMin{};
    Vec4 startColorMax{};
    Vec4 endColorMin{};
    Vec4 endColorMax{};
    Vec4 colorOverLifeCurveParams{};                 // x: color key count, y: alpha key count, z: color enabled, w: alpha enabled.
    Vec4 colorOverLifeColorCurveTimes{};
    Vec4 colorOverLifeColorCurveTimesBlock1{};
    Vec4 colorOverLifeColorCurveValuesR{};
    Vec4 colorOverLifeColorCurveValuesRBlock1{};
    Vec4 colorOverLifeColorCurveValuesG{};
    Vec4 colorOverLifeColorCurveValuesGBlock1{};
    Vec4 colorOverLifeColorCurveValuesB{};
    Vec4 colorOverLifeColorCurveValuesBBlock1{};
    Vec4 colorOverLifeColorCurveArriveR{};
    Vec4 colorOverLifeColorCurveArriveRBlock1{};
    Vec4 colorOverLifeColorCurveArriveG{};
    Vec4 colorOverLifeColorCurveArriveGBlock1{};
    Vec4 colorOverLifeColorCurveArriveB{};
    Vec4 colorOverLifeColorCurveArriveBBlock1{};
    Vec4 colorOverLifeColorCurveLeaveR{};
    Vec4 colorOverLifeColorCurveLeaveRBlock1{};
    Vec4 colorOverLifeColorCurveLeaveG{};
    Vec4 colorOverLifeColorCurveLeaveGBlock1{};
    Vec4 colorOverLifeColorCurveLeaveB{};
    Vec4 colorOverLifeColorCurveLeaveBBlock1{};
    Vec4 colorOverLifeColorCurveModes{};
    Vec4 colorOverLifeColorCurveModesBlock1{};
    Vec4 colorOverLifeAlphaCurveTimes{};
    Vec4 colorOverLifeAlphaCurveTimesBlock1{};
    Vec4 colorOverLifeAlphaCurveValues{};
    Vec4 colorOverLifeAlphaCurveValuesBlock1{};
    Vec4 colorOverLifeAlphaCurveArrive{};
    Vec4 colorOverLifeAlphaCurveArriveBlock1{};
    Vec4 colorOverLifeAlphaCurveLeave{};
    Vec4 colorOverLifeAlphaCurveLeaveBlock1{};
    Vec4 colorOverLifeAlphaCurveModes{};
    Vec4 colorOverLifeAlphaCurveModesBlock1{};
    Vec4 coreColorRgbParticleLifeUniformParams{};
    Vec4 coreColorRgbParticleLifeUniformMin{};
    Vec4 coreColorRgbParticleLifeUniformMax{};
    XMUINT4 coreColorRgbParticleLifeUniformSeed{};
    Vec4 subUVParams{};                              // x/y: rows/cols, z: playback mode, w: frames per second.
    Vec4 subUVFrameParams{};                         // x/y: start/end frame, z: loop, w: enabled.
    Vec4 subUVFrameCurveTimes{};                     // SubUV frame curve key times.
    Vec4 subUVFrameCurveTimesBlock1{};
    Vec4 subUVFrameCurveValues{};                    // SubUV frame curve frame values.
    Vec4 subUVFrameCurveValuesBlock1{};
    Vec4 subUVFrameCurveParams{};                    // x: key count, y: random start phase flag, z/w: unused.
    XMUINT4 spawnSeedSalts{};                        // x: Lifetime, y: InitialLocation, z: SphereLocation, w: PlaneRadialLocation.
    XMUINT4 subUVSeedSalts{};                        // x: SubUV RandomFrame, y/z/w: unused.
    XMUINT4 appearanceSeedSalts0{};                  // x: InitialSize, y: InitialColor RGB, z: InitialColor Alpha, w: ColorOverLife RGB.
    XMUINT4 appearanceSeedSalts1{};                  // x: ColorOverLife Alpha, y/z/w: unused.
    Vec4 sizeByLifeParams{};                         // x/y: width start/end, z/w: height start/end.
    Vec4 sizeByLifeCurveTimes{};                     // SizeByLife scale curve key times.
    Vec4 sizeByLifeCurveTimesBlock1{};
    Vec4 sizeByLifeCurveValuesX{};                   // SizeByLife X multiplier key values.
    Vec4 sizeByLifeCurveValuesXBlock1{};
    Vec4 sizeByLifeCurveValuesY{};                   // SizeByLife Y multiplier key values.
    Vec4 sizeByLifeCurveValuesYBlock1{};
    Vec4 sizeByLifeCurveArriveTangentsX{};           // SizeByLife X arrive tangents.
    Vec4 sizeByLifeCurveArriveTangentsXBlock1{};
    Vec4 sizeByLifeCurveLeaveTangentsX{};            // SizeByLife X leave tangents.
    Vec4 sizeByLifeCurveLeaveTangentsXBlock1{};
    Vec4 sizeByLifeCurveArriveTangentsY{};           // SizeByLife Y arrive tangents.
    Vec4 sizeByLifeCurveArriveTangentsYBlock1{};
    Vec4 sizeByLifeCurveLeaveTangentsY{};            // SizeByLife Y leave tangents.
    Vec4 sizeByLifeCurveLeaveTangentsYBlock1{};
    Vec4 sizeByLifeCurveModes{};                     // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 sizeByLifeCurveModesBlock1{};
    Vec4 sizeByLifeCurveParams{};                    // x: key count, y: multiplyX, z: multiplyY, w: axisLock.
    Vec4 spriteTiltInitial{};                         // x/y: initial min, z/w: initial max.
    Vec4 spriteTiltOverLife{};                        // x/y: over-life min, z/w: over-life max.
    Vec4 spriteTiltCurveTimes{};                      // SpriteTiltOverLife key times.
    Vec4 spriteTiltCurveTimesBlock1{};
    Vec4 spriteTiltCurveValuesX{};                    // SpriteTiltOverLife X key values.
    Vec4 spriteTiltCurveValuesXBlock1{};
    Vec4 spriteTiltCurveValuesY{};                    // SpriteTiltOverLife Y key values.
    Vec4 spriteTiltCurveValuesYBlock1{};
    Vec4 spriteTiltCurveArriveTangentsX{};
    Vec4 spriteTiltCurveArriveTangentsXBlock1{};
    Vec4 spriteTiltCurveLeaveTangentsX{};
    Vec4 spriteTiltCurveLeaveTangentsXBlock1{};
    Vec4 spriteTiltCurveArriveTangentsY{};
    Vec4 spriteTiltCurveArriveTangentsYBlock1{};
    Vec4 spriteTiltCurveLeaveTangentsY{};
    Vec4 spriteTiltCurveLeaveTangentsYBlock1{};
    Vec4 spriteTiltCurveModes{};                      // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 spriteTiltCurveModesBlock1{};
    Vec4 spriteTiltParams{};                          // x: initial enabled, y: over-life enabled, z: key count, w: curve enabled.
    XMUINT4 spriteTiltSeedSalts{};                    // x: initial tilt, y: over-life uniform, z/w: unused.
    Vec4 motionFlags{};                              // x: motion enabled, y: vector enabled, z: vector world, w: radial enabled.
    Vec4 motionSpaceFlags{};                         // x: radial world, y: cone enabled, z: cone world, w: radial center direction mode.
    Vec4 initialVelocityMin{};                       // xyz: vector velocity min, w: unused.
    Vec4 initialVelocityMax{};                       // xyz: vector velocity max, w: unused.
    Vec4 motionRadialPivot{};                        // xyz: radial pivot, w: unused.
    Vec4 motionRadialSpeedDrag{};                    // x/y: radial speed min/max, z/w: drag min/max.
    Vec4 velocityConeAxisAngle{};                    // xyz: cone axis, w: half-angle degrees.
    Vec4 velocityConeSpeed{};                        // x/y: cone speed min/max, z/w: unused.
    Vec4 accelerationMin{};                          // xyz: acceleration min, w: unused.
    Vec4 accelerationMax{};                          // xyz: acceleration max, w: unused.
    Vec4 accelerationCurveTimes{};                   // Acceleration key times.
    Vec4 accelerationCurveTimesBlock1{};
    Vec4 accelerationCurveValuesX{};                 // Acceleration X key values.
    Vec4 accelerationCurveValuesXBlock1{};
    Vec4 accelerationCurveValuesY{};                 // Acceleration Y key values.
    Vec4 accelerationCurveValuesYBlock1{};
    Vec4 accelerationCurveValuesZ{};                 // Acceleration Z key values.
    Vec4 accelerationCurveValuesZBlock1{};
    Vec4 accelerationCurveArriveTangentsX{};         // Acceleration X arrive tangents.
    Vec4 accelerationCurveArriveTangentsXBlock1{};
    Vec4 accelerationCurveLeaveTangentsX{};          // Acceleration X leave tangents.
    Vec4 accelerationCurveLeaveTangentsXBlock1{};
    Vec4 accelerationCurveArriveTangentsY{};         // Acceleration Y arrive tangents.
    Vec4 accelerationCurveArriveTangentsYBlock1{};
    Vec4 accelerationCurveLeaveTangentsY{};          // Acceleration Y leave tangents.
    Vec4 accelerationCurveLeaveTangentsYBlock1{};
    Vec4 accelerationCurveArriveTangentsZ{};         // Acceleration Z arrive tangents.
    Vec4 accelerationCurveArriveTangentsZBlock1{};
    Vec4 accelerationCurveLeaveTangentsZ{};          // Acceleration Z leave tangents.
    Vec4 accelerationCurveLeaveTangentsZBlock1{};
    Vec4 accelerationCurveModes{ 1.f, 1.f, 0.f, 0.f }; // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 accelerationCurveModesBlock1{};
    Vec4 accelerationCurveParams{};                  // x: key count, y: enabled, z: acceleration world, w: emitter-time duration.
    Vec4 velocityScaleByLife{};                      // x/y: scale start/end, z/w: unused.
    Vec4 velocityScaleByLifeCurveTimes{};            // VelocityOverLife key times.
    Vec4 velocityScaleByLifeCurveTimesBlock1{};
    Vec4 velocityScaleByLifeCurveValues{};           // VelocityOverLife multiplier values.
    Vec4 velocityScaleByLifeCurveValuesBlock1{};
    Vec4 velocityScaleByLifeCurveArriveTangents{};   // VelocityOverLife arrive tangents.
    Vec4 velocityScaleByLifeCurveArriveTangentsBlock1{};
    Vec4 velocityScaleByLifeCurveLeaveTangents{};    // VelocityOverLife leave tangents.
    Vec4 velocityScaleByLifeCurveLeaveTangentsBlock1{};
    Vec4 velocityScaleByLifeCurveModes{};            // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 velocityScaleByLifeCurveModesBlock1{};
    Vec4 velocityScaleByLifeCurveParams{};           // x: key count, y/z/w: unused.
    Vec4 orbitParams{};                              // x: enabled, y: plane, z/w: unused.
    Vec4 orbitAngleRange{};                          // x/y: angle start/end degrees, z/w: unused.
    Vec4 orbitAngleCurveTimes{};                     // OrbitOverLife angle key times.
    Vec4 orbitAngleCurveTimesBlock1{};
    Vec4 orbitAngleCurveValues{};                    // OrbitOverLife angle degree key values.
    Vec4 orbitAngleCurveValuesBlock1{};
    Vec4 orbitAngleCurveArriveTangents{};            // OrbitOverLife angle arrive tangents.
    Vec4 orbitAngleCurveArriveTangentsBlock1{};
    Vec4 orbitAngleCurveLeaveTangents{};             // OrbitOverLife angle leave tangents.
    Vec4 orbitAngleCurveLeaveTangentsBlock1{};
    Vec4 orbitAngleCurveModes{};                     // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 orbitAngleCurveModesBlock1{};
    Vec4 orbitAngleCurveParams{};                    // x: key count, y/z/w: unused.
    Vec4 orbitRadiusRange{};                         // x/y: radius scale start/end, z/w: unused.
    Vec4 orbitRadiusCurveTimes{};                    // OrbitOverLife radius scale key times.
    Vec4 orbitRadiusCurveTimesBlock1{};
    Vec4 orbitRadiusCurveValues{};                   // OrbitOverLife radius scale key values.
    Vec4 orbitRadiusCurveValuesBlock1{};
    Vec4 orbitRadiusCurveArriveTangents{};           // OrbitOverLife radius scale arrive tangents.
    Vec4 orbitRadiusCurveArriveTangentsBlock1{};
    Vec4 orbitRadiusCurveLeaveTangents{};            // OrbitOverLife radius scale leave tangents.
    Vec4 orbitRadiusCurveLeaveTangentsBlock1{};
    Vec4 orbitRadiusCurveModes{};                    // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 orbitRadiusCurveModesBlock1{};
    Vec4 orbitRadiusCurveParams{};                   // x: key count, y/z/w: unused.
    XMUINT4 motionSeedSalts0{};                      // x: InitialVelocity, y: InitialRadialVelocity, z: VelocityCone, w: Acceleration.
    XMUINT4 motionSeedSalts1{};                      // x: Drag, y/z/w: unused.
    Vec4 rotationInitialRate{};                      // x/y: initial rotation min/max, z/w: rate min/max.
    Vec4 rotationByLife{};                           // x/y: legacy rotation start/end, z/w: rate scale start/end.
    Vec4 rotationOverLifeCurveTimes{};               // RotationOverLife key times.
    Vec4 rotationOverLifeCurveTimesBlock1{};
    Vec4 rotationOverLifeCurveValues{};              // RotationOverLife degree key values.
    Vec4 rotationOverLifeCurveValuesBlock1{};
    Vec4 rotationOverLifeCurveArriveTangents{};      // RotationOverLife arrive tangents.
    Vec4 rotationOverLifeCurveArriveTangentsBlock1{};
    Vec4 rotationOverLifeCurveLeaveTangents{};       // RotationOverLife leave tangents.
    Vec4 rotationOverLifeCurveLeaveTangentsBlock1{};
    Vec4 rotationOverLifeCurveModes{};               // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 rotationOverLifeCurveModesBlock1{};
    Vec4 rotationOverLifeCurveParams{};              // x: key count, y/z/w: unused.
    Vec4 rotationRateScaleByLifeCurveTimes{};        // RotationRateScaleByLife key times.
    Vec4 rotationRateScaleByLifeCurveTimesBlock1{};
    Vec4 rotationRateScaleByLifeCurveValues{};       // RotationRateScaleByLife multiplier values.
    Vec4 rotationRateScaleByLifeCurveValuesBlock1{};
    Vec4 rotationRateScaleByLifeCurveArriveTangents{}; // RotationRateScaleByLife arrive tangents.
    Vec4 rotationRateScaleByLifeCurveArriveTangentsBlock1{};
    Vec4 rotationRateScaleByLifeCurveLeaveTangents{};  // RotationRateScaleByLife leave tangents.
    Vec4 rotationRateScaleByLifeCurveLeaveTangentsBlock1{};
    Vec4 rotationRateScaleByLifeCurveModes{};          // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 rotationRateScaleByLifeCurveModesBlock1{};
    Vec4 rotationRateScaleByLifeCurveParams{};       // x: key count, y/z/w: unused.
    XMUINT4 rotationSeedSalts{};                     // x: InitialRotation, y: InitialRotationRate, z/w: unused.
    Vec4 planeRadialCameraFacingRight{};             // xyz: camera right axis in emitter-local sampling space.
    Vec4 planeRadialCameraFacingUp{};                // xyz: camera up axis in emitter-local sampling space.
    Vec4 planeRadialCameraFacingNormal{};            // xyz: camera look axis in emitter-local sampling space.
    XMUINT4 spawnSerialParams{};                     // x: replay-local spawn serial base, y/z/w: unused.
    Vec4 sourceMotionVelocity{};                     // xyz: emitter center delta velocity in world space, w: valid.
    Vec4 sourceMotionVelocityParams{};               // x: enabled, y: direction mode, z: spread angle degrees, w: source speed scale.
    Vec4 sourceMotionVelocitySpeed{};                // x/y: speed min/max, z/w: unused.
    XMUINT4 sourceMotionVelocitySeedSalts{};         // x: SourceMotionVelocity, y/z/w: unused.
    Vec4 cylinderLocationParams{};                   // x: enabled, y: axis, z: mode, w: placement mode.
    Vec4 cylinderLocationOffset{};                   // xyz: offset, w: unused.
    Vec4 cylinderLocationRadiusHeight{};             // x/y: radius range, z/w: height range.
    Vec4 cylinderLocationAngle{};                    // x/y: angle degree range, z/w: unused.
    XMUINT4 cylinderLocationSeedSalts{};             // x: radius, y: height, z: angle, w: unused.
    Vec4 cylinderOrientationParams{};                // x: enabled, y: target kind, z: orientation mode, w: follow orbit over life.
    Vec4 cylinderOrientationAngles{};                // x: tilt degrees, y: roll offset degrees, z/w: unused.
    Vec4 initialVelocityScaleByLifeCurveTimes{};     // InitialVelocity channel VelocityOverLife key times.
    Vec4 initialVelocityScaleByLifeCurveTimesBlock1{};
    Vec4 initialVelocityScaleByLifeCurveValues{};    // InitialVelocity channel VelocityOverLife values.
    Vec4 initialVelocityScaleByLifeCurveValuesBlock1{};
    Vec4 initialVelocityScaleByLifeCurveArriveTangents{};
    Vec4 initialVelocityScaleByLifeCurveArriveTangentsBlock1{};
    Vec4 initialVelocityScaleByLifeCurveLeaveTangents{};
    Vec4 initialVelocityScaleByLifeCurveLeaveTangentsBlock1{};
    Vec4 initialVelocityScaleByLifeCurveModes{};
    Vec4 initialVelocityScaleByLifeCurveModesBlock1{};
    Vec4 initialVelocityScaleByLifeCurveParams{};    // x: key count, y: enabled, z/w: unused.
    Vec4 initialRadialVelocityScaleByLifeCurveTimes{};
    Vec4 initialRadialVelocityScaleByLifeCurveTimesBlock1{};
    Vec4 initialRadialVelocityScaleByLifeCurveValues{};
    Vec4 initialRadialVelocityScaleByLifeCurveValuesBlock1{};
    Vec4 initialRadialVelocityScaleByLifeCurveArriveTangents{};
    Vec4 initialRadialVelocityScaleByLifeCurveArriveTangentsBlock1{};
    Vec4 initialRadialVelocityScaleByLifeCurveLeaveTangents{};
    Vec4 initialRadialVelocityScaleByLifeCurveLeaveTangentsBlock1{};
    Vec4 initialRadialVelocityScaleByLifeCurveModes{};
    Vec4 initialRadialVelocityScaleByLifeCurveModesBlock1{};
    Vec4 initialRadialVelocityScaleByLifeCurveParams{}; // x: key count, y: enabled, z/w: unused.
    Vec4 velocityConeScaleByLifeCurveTimes{};
    Vec4 velocityConeScaleByLifeCurveTimesBlock1{};
    Vec4 velocityConeScaleByLifeCurveValues{};
    Vec4 velocityConeScaleByLifeCurveValuesBlock1{};
    Vec4 velocityConeScaleByLifeCurveArriveTangents{};
    Vec4 velocityConeScaleByLifeCurveArriveTangentsBlock1{};
    Vec4 velocityConeScaleByLifeCurveLeaveTangents{};
    Vec4 velocityConeScaleByLifeCurveLeaveTangentsBlock1{};
    Vec4 velocityConeScaleByLifeCurveModes{};
    Vec4 velocityConeScaleByLifeCurveModesBlock1{};
    Vec4 velocityConeScaleByLifeCurveParams{};       // x: key count, y: enabled, z/w: unused.
    Vec4 sourceMotionVelocityScaleByLifeCurveTimes{};
    Vec4 sourceMotionVelocityScaleByLifeCurveTimesBlock1{};
    Vec4 sourceMotionVelocityScaleByLifeCurveValues{};
    Vec4 sourceMotionVelocityScaleByLifeCurveValuesBlock1{};
    Vec4 sourceMotionVelocityScaleByLifeCurveArriveTangents{};
    Vec4 sourceMotionVelocityScaleByLifeCurveArriveTangentsBlock1{};
    Vec4 sourceMotionVelocityScaleByLifeCurveLeaveTangents{};
    Vec4 sourceMotionVelocityScaleByLifeCurveLeaveTangentsBlock1{};
    Vec4 sourceMotionVelocityScaleByLifeCurveModes{};
    Vec4 sourceMotionVelocityScaleByLifeCurveModesBlock1{};
    Vec4 sourceMotionVelocityScaleByLifeCurveParams{}; // x: key count, y: enabled, z/w: unused.
    Vec4 accelerationIntegratedVelocityScaleByLifeCurveTimes{};
    Vec4 accelerationIntegratedVelocityScaleByLifeCurveTimesBlock1{};
    Vec4 accelerationIntegratedVelocityScaleByLifeCurveValues{};
    Vec4 accelerationIntegratedVelocityScaleByLifeCurveValuesBlock1{};
    Vec4 accelerationIntegratedVelocityScaleByLifeCurveArriveTangents{};
    Vec4 accelerationIntegratedVelocityScaleByLifeCurveArriveTangentsBlock1{};
    Vec4 accelerationIntegratedVelocityScaleByLifeCurveLeaveTangents{};
    Vec4 accelerationIntegratedVelocityScaleByLifeCurveLeaveTangentsBlock1{};
    Vec4 accelerationIntegratedVelocityScaleByLifeCurveModes{};
    Vec4 accelerationIntegratedVelocityScaleByLifeCurveModesBlock1{};
    Vec4 accelerationIntegratedVelocityScaleByLifeCurveParams{}; // x: key count, y: enabled, z/w: unused.
};

static uint32 Resolve_SeedSalt(const PointParticleRandomSeedRuntimeDesc& seed, uint32 effectPlaybackSeed)
{
    uint32 salt = seed.manualSeedEnabled ? seed.seed : 0u;
    if (seed.useInstanceSeed)
        salt += effectPlaybackSeed;
    return salt;
}

static float Hash01(uint32 seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return static_cast<float>(seed & 0x00FFFFFFu) / 16777215.f;
}

static Vec3 Normalize_OrFallback(const Vec3& value, const Vec3& fallback)
{
    if (value.LengthSquared() <= 1e-8f)
        return fallback;

    Vec3 result = value;
    result.Normalize();
    return result;
}

static Vec4 Build_FloatCurveParams(const PointParticleFloatCurveRuntimeDesc& curve)
{
    return Vec4{
        static_cast<float>(max(1u, min(kEffectDistributionCurveMaxKeys, curve.curveKeyCount))),
        curve.enabled ? 1.f : 0.f,
        0.f,
        0.f
    };
}

VIBufferCom_ComputePointParticle::VIBufferCom_ComputePointParticle(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : VIBufferCom_Instance{ device, context }
{
}

VIBufferCom_ComputePointParticle::VIBufferCom_ComputePointParticle(const VIBufferCom_ComputePointParticle& prototype)
    : VIBufferCom_Instance{ prototype }
{
}

VIBufferCom_ComputePointParticle::~VIBufferCom_ComputePointParticle()
{
    Free();
}

HRESULT VIBufferCom_ComputePointParticle::Initialize_Prototype()
{
    _numVertexBuffers = 2;
    _numVertices = 1;
    _vertexStride = sizeof(Vec3);
    _numIndices = 0;
    _indexStride = 0;
    _primitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;

    _instanceStride = sizeof(ParticleInstanceVertex);
    _verticesPerInstance = 1;

    return Ready_VertexBuffer();
}

HRESULT VIBufferCom_ComputePointParticle::Initialize(void* arg)
{
    if (nullptr == arg)
    {
        LOG_ERROR("VIBufferCom_ComputePointParticle requires ComputePointParticleDesc at clone time.");
        return E_FAIL;
    }

    _desc = *static_cast<ComputePointParticleDesc*>(arg);

    if (_desc.instanceCount == 0)
    {
        LOG_ERROR("VIBufferCom_ComputePointParticle requires at least one instance.");
        return E_FAIL;
    }

    _instanceCount = _desc.instanceCount;

    _instanceBufferDesc = {
        .ByteWidth = _instanceStride * _instanceCount,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = 0,
        .StructureByteStride = _instanceStride
    };

    CHECK_FAILED(Ready_ResetBaseline(), E_FAIL);
    CHECK_FAILED(Ready_ComputeShader(), E_FAIL);
    CHECK_FAILED(Ready_ComputeOutput(), E_FAIL);
    CHECK_FAILED(Ready_LifecycleState(), E_FAIL);
    CHECK_FAILED(Ready_InstanceBuffer(), E_FAIL);
    CHECK_FAILED(Ready_IndexedDrawBuffer(), E_FAIL);
    CHECK_FAILED(Ready_IndirectArgsBuffers(), E_FAIL);
    CHECK_FAILED(Ready_ConstantBuffer(), E_FAIL);
    CHECK_FAILED(Update_ConstantBuffer(), E_FAIL);

    CHECK_FAILED(_computeShader->Bind_UAV(0, _computeOutput->Get_UAV()), E_FAIL);
    if (nullptr != _computeArgsOutput)
        CHECK_FAILED(_computeShader->Bind_UAV(1, _computeArgsOutput->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_UAV(2, _lifecycleState->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_ConstantBuffer(0, _computeConstantBuffer.Get()), E_FAIL);
    CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);
    CHECK_FAILED(_computeShader->Dispatch((_instanceCount + kThreadCountX - 1) / kThreadCountX, 1, 1), E_FAIL);
    CHECK_FAILED(Copy_OutputToInstanceVB(), E_FAIL);
    CHECK_FAILED(Copy_OutputToIndirectArgs(), E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Render()
{
    if (_instanceCount == 0)
        return E_FAIL;

    switch (_desc.drawMode)
    {
    case PointParticleDrawMode::Direct:
        return __super::Render();

    case PointParticleDrawMode::DrawInstancedIndirect:
        if (nullptr == _indirectArgsBuffer)
            return E_FAIL;

        _context->DrawInstancedIndirect(_indirectArgsBuffer.Get(), 0);
        return S_OK;

    case PointParticleDrawMode::DrawIndexedInstancedIndirect:
        if (nullptr == _indirectArgsBuffer || nullptr == _ib)
            return E_FAIL;

        _context->DrawIndexedInstancedIndirect(_indirectArgsBuffer.Get(), 0);
        return S_OK;

    default:
        return E_FAIL;
    }
}

void VIBufferCom_ComputePointParticle::Dispatch_Compute(
    float timeDelta,
    uint32 spawnRequest,
    uint32 spawnSerialBase,
    float lifetimeSamplePhase,
    float emitterElapsedTime,
    bool killActiveParticles)
{
    if (nullptr == _computeShader || nullptr == _computeOutput || nullptr == _lifecycleState || nullptr == _instanceVB)
        return;

    _hasActiveParticles = _hasActiveParticles || spawnRequest > 0u;
    if (killActiveParticles)
        _hasActiveParticles = false;

    _deltaTime = max(0.f, timeDelta);
    _elapsedTime = max(0.f, emitterElapsedTime);
    _spawnRequest = spawnRequest;
    _spawnSerialBase = spawnSerialBase;
    _lifetimeSamplePhase = clamp(lifetimeSamplePhase, 0.f, 1.f);
    _killActiveParticles = killActiveParticles ? 1u : 0u;
    _sourceMotionVelocity = Vec3{};
    _sourceMotionVelocityValid = false;
    if (_hasPreviousSourceMotionCenter && _deltaTime > 0.f)
    {
        const Vec3 movement = _desc.center - _previousSourceMotionCenter;
        if (movement.LengthSquared() > 0.0001f * 0.0001f)
        {
            _sourceMotionVelocity = movement / max(_deltaTime, 0.0001f);
            _sourceMotionVelocityValid = true;
        }
    }

    CHECK_FAILED(Update_ConstantBuffer());
    CHECK_FAILED(_computeShader->Bind_UAV(0, _computeOutput->Get_UAV()));
    if (nullptr != _computeArgsOutput)
        CHECK_FAILED(_computeShader->Bind_UAV(1, _computeArgsOutput->Get_UAV()));
    CHECK_FAILED(_computeShader->Bind_UAV(2, _lifecycleState->Get_UAV()));
    CHECK_FAILED(_computeShader->Bind_ConstantBuffer(0, _computeConstantBuffer.Get()));
    CHECK_FAILED(Reset_IndirectArgs());
    CHECK_FAILED(_computeShader->Dispatch((_instanceCount + kThreadCountX - 1) / kThreadCountX, 1, 1));
    CHECK_FAILED(Copy_OutputToInstanceVB());
    CHECK_FAILED(Copy_OutputToIndirectArgs());

    _previousSourceMotionCenter = _desc.center;
    _hasPreviousSourceMotionCenter = true;
}

void VIBufferCom_ComputePointParticle::Update_RuntimeDesc(const ComputePointParticleDesc& desc)
{
    if (desc.instanceCount != _instanceCount)
        return;

    _desc = desc;
}

void VIBufferCom_ComputePointParticle::Set_Center(const Vec3& center)
{
    _desc.center = center;
}

void VIBufferCom_ComputePointParticle::Set_EmitterBasis(const Vec3& right, const Vec3& up, const Vec3& look)
{
    _desc.emitterRight = right;
    _desc.emitterUp = up;
    _desc.emitterLook = look;
}

HRESULT VIBufferCom_ComputePointParticle::Reset_ForEffectReplay()
{
    if (nullptr == _computeOutput || nullptr == _lifecycleState || nullptr == _instanceVB)
        return E_FAIL;
    if (_initialLifecycleState.size() != _instanceCount || _emptyInstancePayload.size() != _instanceCount)
        return E_FAIL;

    _elapsedTime = 0.f;
    _deltaTime = 0.f;
    _lifetimeSamplePhase = 0.f;
    _spawnRequest = 0u;
    _spawnSerialBase = 0u;
    _killActiveParticles = 0u;
    _sourceMotionVelocity = Vec3{};
    _previousSourceMotionCenter = _desc.center;
    _hasPreviousSourceMotionCenter = false;
    _sourceMotionVelocityValid = false;
    _hasActiveParticles = false;

    CHECK_FAILED(Ready_ResetBaseline(), E_FAIL);
    CHECK_FAILED(_lifecycleState->Update_Data(_initialLifecycleState.data(), _instanceCount), E_FAIL);
    CHECK_FAILED(_computeOutput->Update_Data(_emptyInstancePayload.data(), _instanceCount), E_FAIL);
    CHECK_FAILED(Update_ConstantBuffer(), E_FAIL);
    CHECK_FAILED(Copy_OutputToInstanceVB(), E_FAIL);
    CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);
    CHECK_FAILED(Copy_OutputToIndirectArgs(), E_FAIL);

    return S_OK;
}

bool VIBufferCom_ComputePointParticle::Collect_FollowerSourcePoints(
    vector<EffectFollowerSourcePoint>& outPoints,
    uint32 maxPointCount) const
{
    if (maxPointCount == 0 || nullptr == _computeOutput || _instanceCount == 0)
        return false;

    vector<ParticleInstanceVertex> particles(_instanceCount);
    if (FAILED(_computeOutput->Read_Data(particles.data(), _instanceCount)))
        return false;

    const size_t initialCount = outPoints.size();
    for (uint32 index = 0; index < _instanceCount && outPoints.size() - initialCount < maxPointCount; ++index)
    {
        const ParticleInstanceVertex& particle = particles[index];
        if (particle.lifeTime.x <= 0.0001f || particle.lifeTime.y < 0.f || particle.lifeTime.y >= particle.lifeTime.x)
            continue;

        outPoints.push_back(
            EffectFollowerSourcePoint{
                .sourceIndex = index,
                .position = Vec3{ particle.translation.x, particle.translation.y, particle.translation.z }
            }
        );
    }

    return outPoints.size() > initialCount;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_VertexBuffer()
{
    const D3D11_BUFFER_DESC vertexBufferDesc = {
        .ByteWidth = _vertexStride * _numVertices,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .StructureByteStride = _vertexStride
    };

    const Vec3 vertex{ 0.f, 0.f, 0.f };

    const D3D11_SUBRESOURCE_DATA initialData = {
        .pSysMem = &vertex
    };

    CHECK_FAILED(_device->CreateBuffer(&vertexBufferDesc, &initialData, _vb.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_ComputeShader()
{
    if (_desc.computeShaderPrototypeTag.empty())
        return E_FAIL;

    _computeShader = dynamic_pointer_cast<ComputeShaderCom>(
        GAME->Clone_Prototype(
            Prototype::Comopnent,
            _desc.computeShaderLevelIndex,
            _desc.computeShaderPrototypeTag
        )
    );
    CHECK_NULL(_computeShader, E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_ComputeOutput()
{
    _computeOutput = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(ParticleInstanceVertex),
        _instanceCount,
        _emptyInstancePayload.data()
    );
    CHECK_NULL(_computeOutput, E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_LifecycleState()
{
    _lifecycleState = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(ParticleLifecycleState),
        _instanceCount,
        _initialLifecycleState.data()
    );
    CHECK_NULL(_lifecycleState, E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_InstanceBuffer()
{
    CHECK_FAILED(_device->CreateBuffer(&_instanceBufferDesc, nullptr, _instanceVB.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_IndexedDrawBuffer()
{
    if (PointParticleDrawMode::DrawIndexedInstancedIndirect != _desc.drawMode)
        return S_OK;

    _numIndices = 1;
    _indexStride = sizeof(uint16);

    const D3D11_BUFFER_DESC indexBufferDesc = {
        .ByteWidth = _indexStride * _numIndices,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_INDEX_BUFFER,
        .StructureByteStride = _indexStride
    };

    const uint16 index = 0;
    const D3D11_SUBRESOURCE_DATA initialData = {
        .pSysMem = &index
    };

    CHECK_FAILED(_device->CreateBuffer(&indexBufferDesc, &initialData, _ib.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_IndirectArgsBuffers()
{
    _computeArgsOutput = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(ComputeIndirectArgs),
        1
    );
    CHECK_NULL(_computeArgsOutput, E_FAIL);

    const D3D11_BUFFER_DESC argsBufferDesc = {
        .ByteWidth = sizeof(ComputeIndirectArgs),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = 0,
        .CPUAccessFlags = 0,
        .MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS
    };

    CHECK_FAILED(_device->CreateBuffer(&argsBufferDesc, nullptr, _indirectArgsBuffer.GetAddressOf()), E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_ConstantBuffer()
{
    const D3D11_BUFFER_DESC bufferDesc = {
        .ByteWidth = sizeof(ComputePointParticleParams),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER
    };

    CHECK_FAILED(_device->CreateBuffer(&bufferDesc, nullptr, _computeConstantBuffer.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Update_ConstantBuffer()
{
    if (nullptr == _computeConstantBuffer)
        return E_FAIL;

    Vec3 cameraFacingRight{ 1.f, 0.f, 0.f };
    Vec3 cameraFacingUp{ 0.f, 1.f, 0.f };
    Vec3 cameraFacingNormal{ 0.f, 0.f, 1.f };
    if (const Shared<Camera> activeCamera = GAME->Get_ActiveCamera())
    {
        if (const Shared<TransformCom> cameraTransform = activeCamera->Get_Transform())
        {
            auto to_emitter_sampling_axis = [this](const Vec3& worldAxis, const Vec3& fallback)
            {
                const Vec3 localAxis{
                    worldAxis.Dot(_desc.emitterRight),
                    worldAxis.Dot(_desc.emitterUp),
                    worldAxis.Dot(_desc.emitterLook)
                };
                return Normalize_OrFallback(localAxis, fallback);
            };

            cameraFacingRight = to_emitter_sampling_axis(cameraTransform->Get_WorldRight(), cameraFacingRight);
            cameraFacingUp = to_emitter_sampling_axis(cameraTransform->Get_WorldUp(), cameraFacingUp);
            cameraFacingNormal = to_emitter_sampling_axis(cameraTransform->Get_WorldForward(), cameraFacingNormal);
        }
    }

    const ComputePointParticleParams params = {
        .elapsedTime = _elapsedTime,
        .deltaTime = _deltaTime,
        .instanceCount = static_cast<float>(_instanceCount),
        .drawMode = ETOI(_desc.drawMode),
        .spawnRequest = _spawnRequest,
        .maxActiveCount = min(_instanceCount, max(1u, _desc.spawn.maxActiveCount)),
        .maxDrawCount = _desc.useMaxDrawCount ? min(_instanceCount, max(1u, _desc.maxDrawCount)) : _instanceCount,
        .killActiveParticles = _killActiveParticles,
        .center = Vec4{ _desc.center.x, _desc.center.y, _desc.center.z, 1.f },
        .emitterBasisRight = Vec4{ _desc.emitterRight.x, _desc.emitterRight.y, _desc.emitterRight.z, 0.f },
        .emitterBasisUp = Vec4{ _desc.emitterUp.x, _desc.emitterUp.y, _desc.emitterUp.z, 0.f },
        .emitterBasisLook = Vec4{ _desc.emitterLook.x, _desc.emitterLook.y, _desc.emitterLook.z, 0.f },
        .spawnRange = Vec4{ _desc.range.x, _desc.range.y, _desc.range.z, 0.f },
        .initialLocationMin = Vec4{
            _desc.initialLocation.minOffset.x,
            _desc.initialLocation.minOffset.y,
            _desc.initialLocation.minOffset.z,
            _desc.initialLocation.enabled ? 1.f : 0.f
        },
        .initialLocationMax = Vec4{
            _desc.initialLocation.maxOffset.x,
            _desc.initialLocation.maxOffset.y,
            _desc.initialLocation.maxOffset.z,
            0.f
        },
        .sphereLocationOffsetRadius = Vec4{
            _desc.sphereLocation.offset.x,
            _desc.sphereLocation.offset.y,
            _desc.sphereLocation.offset.z,
            max(0.f, _desc.sphereLocation.radius)
        },
        .sphereLocationParams = Vec4{
            _desc.sphereLocation.enabled ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.sphereLocation.mode)),
            static_cast<float>(ETOI(_desc.sphereLocation.placementMode)),
            0.f
        },
        .planeRadialLocationParams = Vec4{
            _desc.planeRadialLocation.enabled ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.planeRadialLocation.plane)),
            static_cast<float>(ETOI(_desc.planeRadialLocation.shape)),
            static_cast<float>(ETOI(_desc.planeRadialLocation.placementMode))
        },
        .planeRadialLocationOffsetThickness = Vec4{
            _desc.planeRadialLocation.offset.x,
            _desc.planeRadialLocation.offset.y,
            _desc.planeRadialLocation.offset.z,
            max(0.f, _desc.planeRadialLocation.thickness)
        },
        .planeRadialLocationRectRange = Vec4{
            _desc.planeRadialLocation.uRange.x,
            _desc.planeRadialLocation.uRange.y,
            _desc.planeRadialLocation.vRange.x,
            _desc.planeRadialLocation.vRange.y
        },
        .planeRadialLocationPolarRange = Vec4{
            _desc.planeRadialLocation.radiusRange.x,
            _desc.planeRadialLocation.radiusRange.y,
            _desc.planeRadialLocation.angleDegreesRange.x,
            _desc.planeRadialLocation.angleDegreesRange.y
        },
        .planeRadialLocationSeedSalts = XMUINT4{
            _desc.planeRadialLocation.shape == PointParticlePlaneRadialLocationShape::Rectangle
            ? Resolve_SeedSalt(_desc.planeRadialLocation.uSeed, _desc.effectPlaybackSeed)
            : Resolve_SeedSalt(_desc.planeRadialLocation.radiusSeed, _desc.effectPlaybackSeed),
            _desc.planeRadialLocation.shape == PointParticlePlaneRadialLocationShape::Rectangle
            ? Resolve_SeedSalt(_desc.planeRadialLocation.vSeed, _desc.effectPlaybackSeed)
            : Resolve_SeedSalt(_desc.planeRadialLocation.angleDegreesSeed, _desc.effectPlaybackSeed),
            0u,
            0u
        },
        .planeRadialOrientationParams = Vec4{
            _desc.planeRadialOrientation.enabled ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.planeRadialOrientation.targetKind)),
            static_cast<float>(ETOI(_desc.planeRadialOrientation.orientationMode)),
            0.f
        },
        .sizeMin = Vec4{ _desc.sizeMin.x, _desc.sizeMin.y, 0.f, 0.f },
        .sizeMax = Vec4{ _desc.sizeMax.x, _desc.sizeMax.y, 0.f, 0.f },
        .lifeTime = Vec4{ _desc.lifeTime.x, _desc.lifeTime.y, 0.f, 0.f },
        .lifetimeCurveParams = Vec4{
            static_cast<float>(max(1u, min(kEffectDistributionCurveMaxKeys, _desc.lifetimeCurve.curveKeyCount))),
            _desc.lifetimeCurve.enabled ? 1.f : 0.f,
            clamp(_lifetimeSamplePhase, 0.f, 1.f),
            0.f
        },
        .lifetimeCurveTimes = _desc.lifetimeCurve.curveKeyTimes,
        .lifetimeCurveTimesBlock1 = _desc.lifetimeCurve.curveKeyTimesBlock1,
        .lifetimeCurveValues = _desc.lifetimeCurve.curveKeyValues,
        .lifetimeCurveValuesBlock1 = _desc.lifetimeCurve.curveKeyValuesBlock1,
        .lifetimeCurveArriveTangents = _desc.lifetimeCurve.curveKeyArriveTangents,
        .lifetimeCurveArriveTangentsBlock1 = _desc.lifetimeCurve.curveKeyArriveTangentsBlock1,
        .lifetimeCurveLeaveTangents = _desc.lifetimeCurve.curveKeyLeaveTangents,
        .lifetimeCurveLeaveTangentsBlock1 = _desc.lifetimeCurve.curveKeyLeaveTangentsBlock1,
        .lifetimeCurveModes = _desc.lifetimeCurve.curveKeyModes,
        .lifetimeCurveModesBlock1 = _desc.lifetimeCurve.curveKeyModesBlock1,
        .startColorMin = _desc.startColorMin,
        .startColorMax = _desc.startColorMax,
        .endColorMin = _desc.endColorMin,
        .endColorMax = _desc.endColorMax,
        .colorOverLifeCurveParams = Vec4{
            static_cast<float>(max(1u, min(kEffectDistributionCurveMaxKeys, _desc.colorOverLifeCurve.colorCurveKeyCount))),
            static_cast<float>(max(1u, min(kEffectDistributionCurveMaxKeys, _desc.colorOverLifeCurve.alphaCurveKeyCount))),
            _desc.colorOverLifeCurve.colorCurveEnabled ? 1.f : 0.f,
            _desc.colorOverLifeCurve.alphaCurveEnabled ? 1.f : 0.f
        },
        .colorOverLifeColorCurveTimes = _desc.colorOverLifeCurve.colorCurveTimes,
        .colorOverLifeColorCurveTimesBlock1 = _desc.colorOverLifeCurve.colorCurveTimesBlock1,
        .colorOverLifeColorCurveValuesR = _desc.colorOverLifeCurve.colorCurveValuesR,
        .colorOverLifeColorCurveValuesRBlock1 = _desc.colorOverLifeCurve.colorCurveValuesRBlock1,
        .colorOverLifeColorCurveValuesG = _desc.colorOverLifeCurve.colorCurveValuesG,
        .colorOverLifeColorCurveValuesGBlock1 = _desc.colorOverLifeCurve.colorCurveValuesGBlock1,
        .colorOverLifeColorCurveValuesB = _desc.colorOverLifeCurve.colorCurveValuesB,
        .colorOverLifeColorCurveValuesBBlock1 = _desc.colorOverLifeCurve.colorCurveValuesBBlock1,
        .colorOverLifeColorCurveArriveR = _desc.colorOverLifeCurve.colorCurveArriveR,
        .colorOverLifeColorCurveArriveRBlock1 = _desc.colorOverLifeCurve.colorCurveArriveRBlock1,
        .colorOverLifeColorCurveArriveG = _desc.colorOverLifeCurve.colorCurveArriveG,
        .colorOverLifeColorCurveArriveGBlock1 = _desc.colorOverLifeCurve.colorCurveArriveGBlock1,
        .colorOverLifeColorCurveArriveB = _desc.colorOverLifeCurve.colorCurveArriveB,
        .colorOverLifeColorCurveArriveBBlock1 = _desc.colorOverLifeCurve.colorCurveArriveBBlock1,
        .colorOverLifeColorCurveLeaveR = _desc.colorOverLifeCurve.colorCurveLeaveR,
        .colorOverLifeColorCurveLeaveRBlock1 = _desc.colorOverLifeCurve.colorCurveLeaveRBlock1,
        .colorOverLifeColorCurveLeaveG = _desc.colorOverLifeCurve.colorCurveLeaveG,
        .colorOverLifeColorCurveLeaveGBlock1 = _desc.colorOverLifeCurve.colorCurveLeaveGBlock1,
        .colorOverLifeColorCurveLeaveB = _desc.colorOverLifeCurve.colorCurveLeaveB,
        .colorOverLifeColorCurveLeaveBBlock1 = _desc.colorOverLifeCurve.colorCurveLeaveBBlock1,
        .colorOverLifeColorCurveModes = _desc.colorOverLifeCurve.colorCurveModes,
        .colorOverLifeColorCurveModesBlock1 = _desc.colorOverLifeCurve.colorCurveModesBlock1,
        .colorOverLifeAlphaCurveTimes = _desc.colorOverLifeCurve.alphaCurveTimes,
        .colorOverLifeAlphaCurveTimesBlock1 = _desc.colorOverLifeCurve.alphaCurveTimesBlock1,
        .colorOverLifeAlphaCurveValues = _desc.colorOverLifeCurve.alphaCurveValues,
        .colorOverLifeAlphaCurveValuesBlock1 = _desc.colorOverLifeCurve.alphaCurveValuesBlock1,
        .colorOverLifeAlphaCurveArrive = _desc.colorOverLifeCurve.alphaCurveArrive,
        .colorOverLifeAlphaCurveArriveBlock1 = _desc.colorOverLifeCurve.alphaCurveArriveBlock1,
        .colorOverLifeAlphaCurveLeave = _desc.colorOverLifeCurve.alphaCurveLeave,
        .colorOverLifeAlphaCurveLeaveBlock1 = _desc.colorOverLifeCurve.alphaCurveLeaveBlock1,
        .colorOverLifeAlphaCurveModes = _desc.colorOverLifeCurve.alphaCurveModes,
        .colorOverLifeAlphaCurveModesBlock1 = _desc.colorOverLifeCurve.alphaCurveModesBlock1,
        .coreColorRgbParticleLifeUniformParams = _desc.coreColorRgbParticleLifeUniformParams,
        .coreColorRgbParticleLifeUniformMin = _desc.coreColorRgbParticleLifeUniformMin,
        .coreColorRgbParticleLifeUniformMax = _desc.coreColorRgbParticleLifeUniformMax,
        .coreColorRgbParticleLifeUniformSeed = _desc.coreColorRgbParticleLifeUniformSeed,
        .subUVParams = Vec4{
            static_cast<float>(max(1u, _desc.subUVRows)),
            static_cast<float>(max(1u, _desc.subUVCols)),
            static_cast<float>(ETOI(_desc.subUvFrameOverLife.playbackMode)),
            max(0.f, _desc.subUvFrameOverLife.framesPerSecond)
        },
        .subUVFrameParams = Vec4{
            static_cast<float>(_desc.subUvFrameOverLife.startFrame),
            static_cast<float>(_desc.subUvFrameOverLife.endFrame),
            _desc.subUvFrameOverLife.loop ? 1.f : 0.f,
            _desc.subUvFrameOverLife.enabled ? 1.f : 0.f
        },
        .subUVFrameCurveTimes = _desc.subUvFrameOverLife.frameCurveTimes,
        .subUVFrameCurveTimesBlock1 = _desc.subUvFrameOverLife.frameCurveTimesBlock1,
        .subUVFrameCurveValues = _desc.subUvFrameOverLife.frameCurveValues,
        .subUVFrameCurveValuesBlock1 = _desc.subUvFrameOverLife.frameCurveValuesBlock1,
        .subUVFrameCurveParams = Vec4{
            static_cast<float>(max(1u, min(kEffectDistributionCurveMaxKeys, _desc.subUvFrameOverLife.frameCurveKeyCount))),
            _desc.subUvFrameOverLife.randomStartPhase ? 1.f : 0.f,
            0.f,
            0.f
        },
        .spawnSeedSalts = XMUINT4{
            Resolve_SeedSalt(_desc.lifetimeSeed, _desc.effectPlaybackSeed),
            Resolve_SeedSalt(_desc.initialLocationSeed, _desc.effectPlaybackSeed),
            Resolve_SeedSalt(_desc.sphereLocationSeed, _desc.effectPlaybackSeed),
            0u
        },
        .subUVSeedSalts = XMUINT4{
            Resolve_SeedSalt(_desc.subUVRandomFrameSeed, _desc.effectPlaybackSeed),
            0u,
            0u,
            0u
        },
        .appearanceSeedSalts0 = XMUINT4{
            Resolve_SeedSalt(_desc.initialSizeSeed, _desc.effectPlaybackSeed),
            Resolve_SeedSalt(_desc.initialColorSeed, _desc.effectPlaybackSeed),
            Resolve_SeedSalt(_desc.initialAlphaSeed, _desc.effectPlaybackSeed),
            Resolve_SeedSalt(_desc.colorOverLifeSeed, _desc.effectPlaybackSeed)
        },
        .appearanceSeedSalts1 = XMUINT4{
            Resolve_SeedSalt(_desc.alphaOverLifeSeed, _desc.effectPlaybackSeed),
            0u,
            0u,
            0u
        },
        .sizeByLifeParams = Vec4{
            _desc.sizeByLife.multiplyXStart,
            _desc.sizeByLife.multiplyXEnd,
            _desc.sizeByLife.multiplyYStart,
            _desc.sizeByLife.multiplyYEnd
        },
        .sizeByLifeCurveTimes = _desc.sizeByLife.curveKeyTimes,
        .sizeByLifeCurveTimesBlock1 = _desc.sizeByLife.curveKeyTimesBlock1,
        .sizeByLifeCurveValuesX = _desc.sizeByLife.curveKeyValuesX,
        .sizeByLifeCurveValuesXBlock1 = _desc.sizeByLife.curveKeyValuesXBlock1,
        .sizeByLifeCurveValuesY = _desc.sizeByLife.curveKeyValuesY,
        .sizeByLifeCurveValuesYBlock1 = _desc.sizeByLife.curveKeyValuesYBlock1,
        .sizeByLifeCurveArriveTangentsX = _desc.sizeByLife.curveKeyArriveTangentsX,
        .sizeByLifeCurveArriveTangentsXBlock1 = _desc.sizeByLife.curveKeyArriveTangentsXBlock1,
        .sizeByLifeCurveLeaveTangentsX = _desc.sizeByLife.curveKeyLeaveTangentsX,
        .sizeByLifeCurveLeaveTangentsXBlock1 = _desc.sizeByLife.curveKeyLeaveTangentsXBlock1,
        .sizeByLifeCurveArriveTangentsY = _desc.sizeByLife.curveKeyArriveTangentsY,
        .sizeByLifeCurveArriveTangentsYBlock1 = _desc.sizeByLife.curveKeyArriveTangentsYBlock1,
        .sizeByLifeCurveLeaveTangentsY = _desc.sizeByLife.curveKeyLeaveTangentsY,
        .sizeByLifeCurveLeaveTangentsYBlock1 = _desc.sizeByLife.curveKeyLeaveTangentsYBlock1,
        .sizeByLifeCurveModes = _desc.sizeByLife.curveKeyModes,
        .sizeByLifeCurveModesBlock1 = _desc.sizeByLife.curveKeyModesBlock1,
        .sizeByLifeCurveParams = Vec4{
            static_cast<float>(max(1u, min(kEffectDistributionCurveMaxKeys, _desc.sizeByLife.curveKeyCount))),
            _desc.sizeByLife.multiplyX ? 1.f : 0.f,
            _desc.sizeByLife.multiplyY ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.sizeByLife.axisLock))
        },
        .spriteTiltInitial = Vec4{
            _desc.spriteTilt.tiltDegreesMin.x,
            _desc.spriteTilt.tiltDegreesMin.y,
            _desc.spriteTilt.tiltDegreesMax.x,
            _desc.spriteTilt.tiltDegreesMax.y
        },
        .spriteTiltOverLife = Vec4{
            _desc.spriteTilt.tiltOverLifeMin.x,
            _desc.spriteTilt.tiltOverLifeMin.y,
            _desc.spriteTilt.tiltOverLifeMax.x,
            _desc.spriteTilt.tiltOverLifeMax.y
        },
        .spriteTiltCurveTimes = _desc.spriteTilt.tiltOverLifeCurveTimes,
        .spriteTiltCurveTimesBlock1 = _desc.spriteTilt.tiltOverLifeCurveTimesBlock1,
        .spriteTiltCurveValuesX = _desc.spriteTilt.tiltOverLifeCurveValuesX,
        .spriteTiltCurveValuesXBlock1 = _desc.spriteTilt.tiltOverLifeCurveValuesXBlock1,
        .spriteTiltCurveValuesY = _desc.spriteTilt.tiltOverLifeCurveValuesY,
        .spriteTiltCurveValuesYBlock1 = _desc.spriteTilt.tiltOverLifeCurveValuesYBlock1,
        .spriteTiltCurveArriveTangentsX = _desc.spriteTilt.tiltOverLifeCurveArriveTangentsX,
        .spriteTiltCurveArriveTangentsXBlock1 = _desc.spriteTilt.tiltOverLifeCurveArriveTangentsXBlock1,
        .spriteTiltCurveLeaveTangentsX = _desc.spriteTilt.tiltOverLifeCurveLeaveTangentsX,
        .spriteTiltCurveLeaveTangentsXBlock1 = _desc.spriteTilt.tiltOverLifeCurveLeaveTangentsXBlock1,
        .spriteTiltCurveArriveTangentsY = _desc.spriteTilt.tiltOverLifeCurveArriveTangentsY,
        .spriteTiltCurveArriveTangentsYBlock1 = _desc.spriteTilt.tiltOverLifeCurveArriveTangentsYBlock1,
        .spriteTiltCurveLeaveTangentsY = _desc.spriteTilt.tiltOverLifeCurveLeaveTangentsY,
        .spriteTiltCurveLeaveTangentsYBlock1 = _desc.spriteTilt.tiltOverLifeCurveLeaveTangentsYBlock1,
        .spriteTiltCurveModes = _desc.spriteTilt.tiltOverLifeCurveModes,
        .spriteTiltCurveModesBlock1 = _desc.spriteTilt.tiltOverLifeCurveModesBlock1,
        .spriteTiltParams = Vec4{
            _desc.spriteTilt.initialTiltEnabled ? 1.f : 0.f,
            _desc.spriteTilt.tiltOverLifeEnabled ? 1.f : 0.f,
            static_cast<float>(max(1u, min(kEffectDistributionCurveMaxKeys, _desc.spriteTilt.tiltOverLifeCurveKeyCount))),
            _desc.spriteTilt.tiltOverLifeCurveEnabled ? 1.f : 0.f
        },
        .spriteTiltSeedSalts = XMUINT4{
            Resolve_SeedSalt(_desc.spriteTilt.tiltSeed, _desc.effectPlaybackSeed),
            Resolve_SeedSalt(_desc.spriteTilt.tiltOverLifeSeed, _desc.effectPlaybackSeed),
            0u,
            0u
        },
        .motionFlags = Vec4{
            _desc.motion.enabled ? 1.f : 0.f,
            _desc.motion.initialVelocityEnabled ? 1.f : 0.f,
            _desc.motion.initialVelocityInWorldSpace ? 1.f : 0.f,
            _desc.motion.initialRadialVelocityEnabled ? 1.f : 0.f
        },
        .motionSpaceFlags = Vec4{
            _desc.motion.initialRadialVelocityInWorldSpace ? 1.f : 0.f,
            _desc.motion.velocityConeEnabled ? 1.f : 0.f,
            _desc.motion.velocityConeInWorldSpace ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.motion.initialRadialVelocityCenterDirectionMode))
        },
        .initialVelocityMin = Vec4{
            _desc.motion.initialVelocityMin.x,
            _desc.motion.initialVelocityMin.y,
            _desc.motion.initialVelocityMin.z,
            0.f
        },
        .initialVelocityMax = Vec4{
            _desc.motion.initialVelocityMax.x,
            _desc.motion.initialVelocityMax.y,
            _desc.motion.initialVelocityMax.z,
            0.f
        },
        .motionRadialPivot = Vec4{
            _desc.motion.radialPivot.x,
            _desc.motion.radialPivot.y,
            _desc.motion.radialPivot.z,
            0.f
        },
        .motionRadialSpeedDrag = Vec4{
            _desc.motion.radialSpeed.x,
            _desc.motion.radialSpeed.y,
            _desc.motion.drag.x,
            _desc.motion.drag.y
        },
        .velocityConeAxisAngle = Vec4{
            _desc.motion.velocityConeAxis.x,
            _desc.motion.velocityConeAxis.y,
            _desc.motion.velocityConeAxis.z,
            clamp(_desc.motion.velocityConeAngleDegrees, 0.f, 180.f)
        },
        .velocityConeSpeed = Vec4{
            _desc.motion.velocityConeSpeed.x,
            _desc.motion.velocityConeSpeed.y,
            0.f,
            0.f
        },
        .accelerationMin = Vec4{
            _desc.motion.accelerationMin.x,
            _desc.motion.accelerationMin.y,
            _desc.motion.accelerationMin.z,
            0.f
        },
        .accelerationMax = Vec4{
            _desc.motion.accelerationMax.x,
            _desc.motion.accelerationMax.y,
            _desc.motion.accelerationMax.z,
            0.f
        },
        .accelerationCurveTimes = _desc.motion.accelerationCurveTimes,
        .accelerationCurveTimesBlock1 = _desc.motion.accelerationCurveTimesBlock1,
        .accelerationCurveValuesX = _desc.motion.accelerationCurveValuesX,
        .accelerationCurveValuesXBlock1 = _desc.motion.accelerationCurveValuesXBlock1,
        .accelerationCurveValuesY = _desc.motion.accelerationCurveValuesY,
        .accelerationCurveValuesYBlock1 = _desc.motion.accelerationCurveValuesYBlock1,
        .accelerationCurveValuesZ = _desc.motion.accelerationCurveValuesZ,
        .accelerationCurveValuesZBlock1 = _desc.motion.accelerationCurveValuesZBlock1,
        .accelerationCurveArriveTangentsX = _desc.motion.accelerationCurveArriveTangentsX,
        .accelerationCurveArriveTangentsXBlock1 = _desc.motion.accelerationCurveArriveTangentsXBlock1,
        .accelerationCurveLeaveTangentsX = _desc.motion.accelerationCurveLeaveTangentsX,
        .accelerationCurveLeaveTangentsXBlock1 = _desc.motion.accelerationCurveLeaveTangentsXBlock1,
        .accelerationCurveArriveTangentsY = _desc.motion.accelerationCurveArriveTangentsY,
        .accelerationCurveArriveTangentsYBlock1 = _desc.motion.accelerationCurveArriveTangentsYBlock1,
        .accelerationCurveLeaveTangentsY = _desc.motion.accelerationCurveLeaveTangentsY,
        .accelerationCurveLeaveTangentsYBlock1 = _desc.motion.accelerationCurveLeaveTangentsYBlock1,
        .accelerationCurveArriveTangentsZ = _desc.motion.accelerationCurveArriveTangentsZ,
        .accelerationCurveArriveTangentsZBlock1 = _desc.motion.accelerationCurveArriveTangentsZBlock1,
        .accelerationCurveLeaveTangentsZ = _desc.motion.accelerationCurveLeaveTangentsZ,
        .accelerationCurveLeaveTangentsZBlock1 = _desc.motion.accelerationCurveLeaveTangentsZBlock1,
        .accelerationCurveModes = _desc.motion.accelerationCurveModes,
        .accelerationCurveModesBlock1 = _desc.motion.accelerationCurveModesBlock1,
        .accelerationCurveParams = Vec4{
            static_cast<float>(max(1u, min(kEffectDistributionCurveMaxKeys, _desc.motion.accelerationCurveKeyCount))),
            _desc.motion.accelerationCurveEnabled ? 1.f : 0.f,
            _desc.motion.accelerationInWorldSpace ? 1.f : 0.f,
            _desc.motion.accelerationTimeBasis == PointParticleAccelerationTimeBasis::EmitterNormalizedTime
            ? max(0.0001f, _desc.playbackDuration)
            : 0.f
        },
        .velocityScaleByLife = Vec4{ _desc.motion.velocityScaleByLife.x, _desc.motion.velocityScaleByLife.y, 0.f, 0.f },
        .velocityScaleByLifeCurveTimes = _desc.motion.velocityScaleByLifeCurveTimes,
        .velocityScaleByLifeCurveTimesBlock1 = _desc.motion.velocityScaleByLifeCurveTimesBlock1,
        .velocityScaleByLifeCurveValues = _desc.motion.velocityScaleByLifeCurveValues,
        .velocityScaleByLifeCurveValuesBlock1 = _desc.motion.velocityScaleByLifeCurveValuesBlock1,
        .velocityScaleByLifeCurveArriveTangents = _desc.motion.velocityScaleByLifeCurveArriveTangents,
        .velocityScaleByLifeCurveArriveTangentsBlock1 = _desc.motion.velocityScaleByLifeCurveArriveTangentsBlock1,
        .velocityScaleByLifeCurveLeaveTangents = _desc.motion.velocityScaleByLifeCurveLeaveTangents,
        .velocityScaleByLifeCurveLeaveTangentsBlock1 = _desc.motion.velocityScaleByLifeCurveLeaveTangentsBlock1,
        .velocityScaleByLifeCurveModes = _desc.motion.velocityScaleByLifeCurveModes,
        .velocityScaleByLifeCurveModesBlock1 = _desc.motion.velocityScaleByLifeCurveModesBlock1,
        .velocityScaleByLifeCurveParams = Vec4{
            static_cast<float>(max(1u, min(kEffectDistributionCurveMaxKeys, _desc.motion.velocityScaleByLifeCurveKeyCount))),
            0.f,
            0.f,
            0.f
        },
        .orbitParams = Vec4{
            _desc.orbitOverLife.enabled ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.orbitOverLife.plane)),
            0.f,
            0.f
        },
        .orbitAngleRange = Vec4{ _desc.orbitOverLife.angleDegreesOverLife.x, _desc.orbitOverLife.angleDegreesOverLife.y, 0.f, 0.f },
        .orbitAngleCurveTimes = _desc.orbitOverLife.angleCurveTimes,
        .orbitAngleCurveTimesBlock1 = _desc.orbitOverLife.angleCurveTimesBlock1,
        .orbitAngleCurveValues = _desc.orbitOverLife.angleCurveValues,
        .orbitAngleCurveValuesBlock1 = _desc.orbitOverLife.angleCurveValuesBlock1,
        .orbitAngleCurveArriveTangents = _desc.orbitOverLife.angleCurveArriveTangents,
        .orbitAngleCurveArriveTangentsBlock1 = _desc.orbitOverLife.angleCurveArriveTangentsBlock1,
        .orbitAngleCurveLeaveTangents = _desc.orbitOverLife.angleCurveLeaveTangents,
        .orbitAngleCurveLeaveTangentsBlock1 = _desc.orbitOverLife.angleCurveLeaveTangentsBlock1,
        .orbitAngleCurveModes = _desc.orbitOverLife.angleCurveModes,
        .orbitAngleCurveModesBlock1 = _desc.orbitOverLife.angleCurveModesBlock1,
        .orbitAngleCurveParams = Vec4{
            static_cast<float>(max(1u, min(kEffectDistributionCurveMaxKeys, _desc.orbitOverLife.angleCurveKeyCount))),
            0.f,
            0.f,
            0.f
        },
        .orbitRadiusRange = Vec4{ _desc.orbitOverLife.radiusScaleOverLife.x, _desc.orbitOverLife.radiusScaleOverLife.y, 0.f, 0.f },
        .orbitRadiusCurveTimes = _desc.orbitOverLife.radiusScaleCurveTimes,
        .orbitRadiusCurveTimesBlock1 = _desc.orbitOverLife.radiusScaleCurveTimesBlock1,
        .orbitRadiusCurveValues = _desc.orbitOverLife.radiusScaleCurveValues,
        .orbitRadiusCurveValuesBlock1 = _desc.orbitOverLife.radiusScaleCurveValuesBlock1,
        .orbitRadiusCurveArriveTangents = _desc.orbitOverLife.radiusScaleCurveArriveTangents,
        .orbitRadiusCurveArriveTangentsBlock1 = _desc.orbitOverLife.radiusScaleCurveArriveTangentsBlock1,
        .orbitRadiusCurveLeaveTangents = _desc.orbitOverLife.radiusScaleCurveLeaveTangents,
        .orbitRadiusCurveLeaveTangentsBlock1 = _desc.orbitOverLife.radiusScaleCurveLeaveTangentsBlock1,
        .orbitRadiusCurveModes = _desc.orbitOverLife.radiusScaleCurveModes,
        .orbitRadiusCurveModesBlock1 = _desc.orbitOverLife.radiusScaleCurveModesBlock1,
        .orbitRadiusCurveParams = Vec4{
            static_cast<float>(max(1u, min(kEffectDistributionCurveMaxKeys, _desc.orbitOverLife.radiusScaleCurveKeyCount))),
            0.f,
            0.f,
            0.f
        },
        .motionSeedSalts0 = XMUINT4{
            Resolve_SeedSalt(_desc.motion.initialVelocitySeed, _desc.effectPlaybackSeed),
            Resolve_SeedSalt(_desc.motion.initialRadialVelocitySeed, _desc.effectPlaybackSeed),
            Resolve_SeedSalt(_desc.motion.velocityConeSeed, _desc.effectPlaybackSeed),
            Resolve_SeedSalt(_desc.motion.accelerationSeed, _desc.effectPlaybackSeed)
        },
        .motionSeedSalts1 = XMUINT4{
            Resolve_SeedSalt(_desc.motion.dragSeed, _desc.effectPlaybackSeed),
            0u,
            0u,
            0u
        },
        .rotationInitialRate = Vec4{
            _desc.rotation.initialRotationDegrees.x,
            _desc.rotation.initialRotationDegrees.y,
            _desc.rotation.initialRotationRateDegrees.x,
            _desc.rotation.initialRotationRateDegrees.y
        },
        .rotationByLife = Vec4{
            _desc.rotation.rotationOverLifeDegrees.x,
            _desc.rotation.rotationOverLifeDegrees.y,
            _desc.rotation.rotationRateScaleByLife.x,
            _desc.rotation.rotationRateScaleByLife.y
        },
        .rotationOverLifeCurveTimes = _desc.rotation.rotationOverLifeCurveTimes,
        .rotationOverLifeCurveTimesBlock1 = _desc.rotation.rotationOverLifeCurveTimesBlock1,
        .rotationOverLifeCurveValues = _desc.rotation.rotationOverLifeCurveValues,
        .rotationOverLifeCurveValuesBlock1 = _desc.rotation.rotationOverLifeCurveValuesBlock1,
        .rotationOverLifeCurveArriveTangents = _desc.rotation.rotationOverLifeCurveArriveTangents,
        .rotationOverLifeCurveArriveTangentsBlock1 = _desc.rotation.rotationOverLifeCurveArriveTangentsBlock1,
        .rotationOverLifeCurveLeaveTangents = _desc.rotation.rotationOverLifeCurveLeaveTangents,
        .rotationOverLifeCurveLeaveTangentsBlock1 = _desc.rotation.rotationOverLifeCurveLeaveTangentsBlock1,
        .rotationOverLifeCurveModes = _desc.rotation.rotationOverLifeCurveModes,
        .rotationOverLifeCurveModesBlock1 = _desc.rotation.rotationOverLifeCurveModesBlock1,
        .rotationOverLifeCurveParams = Vec4{
            static_cast<float>(max(1u, min(kEffectDistributionCurveMaxKeys, _desc.rotation.rotationOverLifeCurveKeyCount))),
            0.f,
            0.f,
            0.f
        },
        .rotationRateScaleByLifeCurveTimes = _desc.rotation.rotationRateScaleByLifeCurveTimes,
        .rotationRateScaleByLifeCurveTimesBlock1 = _desc.rotation.rotationRateScaleByLifeCurveTimesBlock1,
        .rotationRateScaleByLifeCurveValues = _desc.rotation.rotationRateScaleByLifeCurveValues,
        .rotationRateScaleByLifeCurveValuesBlock1 = _desc.rotation.rotationRateScaleByLifeCurveValuesBlock1,
        .rotationRateScaleByLifeCurveArriveTangents = _desc.rotation.rotationRateScaleByLifeCurveArriveTangents,
        .rotationRateScaleByLifeCurveArriveTangentsBlock1 = _desc.rotation.rotationRateScaleByLifeCurveArriveTangentsBlock1,
        .rotationRateScaleByLifeCurveLeaveTangents = _desc.rotation.rotationRateScaleByLifeCurveLeaveTangents,
        .rotationRateScaleByLifeCurveLeaveTangentsBlock1 = _desc.rotation.rotationRateScaleByLifeCurveLeaveTangentsBlock1,
        .rotationRateScaleByLifeCurveModes = _desc.rotation.rotationRateScaleByLifeCurveModes,
        .rotationRateScaleByLifeCurveModesBlock1 = _desc.rotation.rotationRateScaleByLifeCurveModesBlock1,
        .rotationRateScaleByLifeCurveParams = Vec4{
            static_cast<float>(max(1u, min(kEffectDistributionCurveMaxKeys, _desc.rotation.rotationRateScaleByLifeCurveKeyCount))),
            0.f,
            0.f,
            0.f
        },
        .rotationSeedSalts = XMUINT4{
            Resolve_SeedSalt(_desc.rotation.initialRotationSeed, _desc.effectPlaybackSeed),
            Resolve_SeedSalt(_desc.rotation.initialRotationRateSeed, _desc.effectPlaybackSeed),
            0u,
            0u
        },
        .planeRadialCameraFacingRight = Vec4{ cameraFacingRight.x, cameraFacingRight.y, cameraFacingRight.z, 0.f },
        .planeRadialCameraFacingUp = Vec4{ cameraFacingUp.x, cameraFacingUp.y, cameraFacingUp.z, 0.f },
        .planeRadialCameraFacingNormal = Vec4{ cameraFacingNormal.x, cameraFacingNormal.y, cameraFacingNormal.z, 0.f },
        .spawnSerialParams = XMUINT4{ _spawnSerialBase, 0u, 0u, 0u },
        .sourceMotionVelocity = Vec4{
            _sourceMotionVelocity.x,
            _sourceMotionVelocity.y,
            _sourceMotionVelocity.z,
            _sourceMotionVelocityValid ? 1.f : 0.f
        },
        .sourceMotionVelocityParams = Vec4{
            _desc.motion.sourceMotionVelocityEnabled ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.motion.sourceMotionVelocityDirectionMode)),
            clamp(_desc.motion.sourceMotionVelocitySpreadAngleDegrees, 0.f, 180.f),
            max(0.f, _desc.motion.sourceMotionVelocitySourceSpeedScale)
        },
        .sourceMotionVelocitySpeed = Vec4{
            _desc.motion.sourceMotionVelocitySpeed.x,
            _desc.motion.sourceMotionVelocitySpeed.y,
            0.f,
            0.f
        },
        .sourceMotionVelocitySeedSalts = XMUINT4{
            Resolve_SeedSalt(_desc.motion.sourceMotionVelocitySeed, _desc.effectPlaybackSeed),
            0u,
            0u,
            0u
        },
        .cylinderLocationParams = Vec4{
            _desc.cylinderLocation.enabled ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.cylinderLocation.axis)),
            static_cast<float>(ETOI(_desc.cylinderLocation.mode)),
            static_cast<float>(ETOI(_desc.cylinderLocation.placementMode))
        },
        .cylinderLocationOffset = Vec4{
            _desc.cylinderLocation.offset.x,
            _desc.cylinderLocation.offset.y,
            _desc.cylinderLocation.offset.z,
            0.f
        },
        .cylinderLocationRadiusHeight = Vec4{
            _desc.cylinderLocation.radiusRange.x,
            _desc.cylinderLocation.radiusRange.y,
            _desc.cylinderLocation.heightRange.x,
            _desc.cylinderLocation.heightRange.y
        },
        .cylinderLocationAngle = Vec4{
            _desc.cylinderLocation.angleDegreesRange.x,
            _desc.cylinderLocation.angleDegreesRange.y,
            0.f,
            0.f
        },
        .cylinderLocationSeedSalts = XMUINT4{
            Resolve_SeedSalt(_desc.cylinderLocation.radiusSeed, _desc.effectPlaybackSeed),
            Resolve_SeedSalt(_desc.cylinderLocation.heightSeed, _desc.effectPlaybackSeed),
            Resolve_SeedSalt(_desc.cylinderLocation.angleDegreesSeed, _desc.effectPlaybackSeed),
            0u
        },
        .cylinderOrientationParams = Vec4{
            _desc.cylinderOrientation.enabled ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.cylinderOrientation.targetKind)),
            static_cast<float>(ETOI(_desc.cylinderOrientation.orientationMode)),
            _desc.cylinderOrientation.followOrbitOverLife ? 1.f : 0.f
        },
        .cylinderOrientationAngles = Vec4{
            _desc.cylinderOrientation.tiltDegrees,
            _desc.cylinderOrientation.rollOffsetDegrees,
            0.f,
            0.f
        },
        .initialVelocityScaleByLifeCurveTimes = _desc.motion.initialVelocityScaleByLife.curveKeyTimes,
        .initialVelocityScaleByLifeCurveTimesBlock1 = _desc.motion.initialVelocityScaleByLife.curveKeyTimesBlock1,
        .initialVelocityScaleByLifeCurveValues = _desc.motion.initialVelocityScaleByLife.curveKeyValues,
        .initialVelocityScaleByLifeCurveValuesBlock1 = _desc.motion.initialVelocityScaleByLife.curveKeyValuesBlock1,
        .initialVelocityScaleByLifeCurveArriveTangents = _desc.motion.initialVelocityScaleByLife.curveKeyArriveTangents,
        .initialVelocityScaleByLifeCurveArriveTangentsBlock1 = _desc.motion.initialVelocityScaleByLife.curveKeyArriveTangentsBlock1,
        .initialVelocityScaleByLifeCurveLeaveTangents = _desc.motion.initialVelocityScaleByLife.curveKeyLeaveTangents,
        .initialVelocityScaleByLifeCurveLeaveTangentsBlock1 = _desc.motion.initialVelocityScaleByLife.curveKeyLeaveTangentsBlock1,
        .initialVelocityScaleByLifeCurveModes = _desc.motion.initialVelocityScaleByLife.curveKeyModes,
        .initialVelocityScaleByLifeCurveModesBlock1 = _desc.motion.initialVelocityScaleByLife.curveKeyModesBlock1,
        .initialVelocityScaleByLifeCurveParams = Build_FloatCurveParams(_desc.motion.initialVelocityScaleByLife),
        .initialRadialVelocityScaleByLifeCurveTimes = _desc.motion.initialRadialVelocityScaleByLife.curveKeyTimes,
        .initialRadialVelocityScaleByLifeCurveTimesBlock1 = _desc.motion.initialRadialVelocityScaleByLife.curveKeyTimesBlock1,
        .initialRadialVelocityScaleByLifeCurveValues = _desc.motion.initialRadialVelocityScaleByLife.curveKeyValues,
        .initialRadialVelocityScaleByLifeCurveValuesBlock1 = _desc.motion.initialRadialVelocityScaleByLife.curveKeyValuesBlock1,
        .initialRadialVelocityScaleByLifeCurveArriveTangents = _desc.motion.initialRadialVelocityScaleByLife.curveKeyArriveTangents,
        .initialRadialVelocityScaleByLifeCurveArriveTangentsBlock1 = _desc.motion.initialRadialVelocityScaleByLife.curveKeyArriveTangentsBlock1,
        .initialRadialVelocityScaleByLifeCurveLeaveTangents = _desc.motion.initialRadialVelocityScaleByLife.curveKeyLeaveTangents,
        .initialRadialVelocityScaleByLifeCurveLeaveTangentsBlock1 = _desc.motion.initialRadialVelocityScaleByLife.curveKeyLeaveTangentsBlock1,
        .initialRadialVelocityScaleByLifeCurveModes = _desc.motion.initialRadialVelocityScaleByLife.curveKeyModes,
        .initialRadialVelocityScaleByLifeCurveModesBlock1 = _desc.motion.initialRadialVelocityScaleByLife.curveKeyModesBlock1,
        .initialRadialVelocityScaleByLifeCurveParams = Build_FloatCurveParams(_desc.motion.initialRadialVelocityScaleByLife),
        .velocityConeScaleByLifeCurveTimes = _desc.motion.velocityConeScaleByLife.curveKeyTimes,
        .velocityConeScaleByLifeCurveTimesBlock1 = _desc.motion.velocityConeScaleByLife.curveKeyTimesBlock1,
        .velocityConeScaleByLifeCurveValues = _desc.motion.velocityConeScaleByLife.curveKeyValues,
        .velocityConeScaleByLifeCurveValuesBlock1 = _desc.motion.velocityConeScaleByLife.curveKeyValuesBlock1,
        .velocityConeScaleByLifeCurveArriveTangents = _desc.motion.velocityConeScaleByLife.curveKeyArriveTangents,
        .velocityConeScaleByLifeCurveArriveTangentsBlock1 = _desc.motion.velocityConeScaleByLife.curveKeyArriveTangentsBlock1,
        .velocityConeScaleByLifeCurveLeaveTangents = _desc.motion.velocityConeScaleByLife.curveKeyLeaveTangents,
        .velocityConeScaleByLifeCurveLeaveTangentsBlock1 = _desc.motion.velocityConeScaleByLife.curveKeyLeaveTangentsBlock1,
        .velocityConeScaleByLifeCurveModes = _desc.motion.velocityConeScaleByLife.curveKeyModes,
        .velocityConeScaleByLifeCurveModesBlock1 = _desc.motion.velocityConeScaleByLife.curveKeyModesBlock1,
        .velocityConeScaleByLifeCurveParams = Build_FloatCurveParams(_desc.motion.velocityConeScaleByLife),
        .sourceMotionVelocityScaleByLifeCurveTimes = _desc.motion.sourceMotionVelocityScaleByLife.curveKeyTimes,
        .sourceMotionVelocityScaleByLifeCurveTimesBlock1 = _desc.motion.sourceMotionVelocityScaleByLife.curveKeyTimesBlock1,
        .sourceMotionVelocityScaleByLifeCurveValues = _desc.motion.sourceMotionVelocityScaleByLife.curveKeyValues,
        .sourceMotionVelocityScaleByLifeCurveValuesBlock1 = _desc.motion.sourceMotionVelocityScaleByLife.curveKeyValuesBlock1,
        .sourceMotionVelocityScaleByLifeCurveArriveTangents = _desc.motion.sourceMotionVelocityScaleByLife.curveKeyArriveTangents,
        .sourceMotionVelocityScaleByLifeCurveArriveTangentsBlock1 = _desc.motion.sourceMotionVelocityScaleByLife.curveKeyArriveTangentsBlock1,
        .sourceMotionVelocityScaleByLifeCurveLeaveTangents = _desc.motion.sourceMotionVelocityScaleByLife.curveKeyLeaveTangents,
        .sourceMotionVelocityScaleByLifeCurveLeaveTangentsBlock1 = _desc.motion.sourceMotionVelocityScaleByLife.curveKeyLeaveTangentsBlock1,
        .sourceMotionVelocityScaleByLifeCurveModes = _desc.motion.sourceMotionVelocityScaleByLife.curveKeyModes,
        .sourceMotionVelocityScaleByLifeCurveModesBlock1 = _desc.motion.sourceMotionVelocityScaleByLife.curveKeyModesBlock1,
        .sourceMotionVelocityScaleByLifeCurveParams = Build_FloatCurveParams(_desc.motion.sourceMotionVelocityScaleByLife),
        .accelerationIntegratedVelocityScaleByLifeCurveTimes = _desc.motion.accelerationIntegratedVelocityScaleByLife.curveKeyTimes,
        .accelerationIntegratedVelocityScaleByLifeCurveTimesBlock1 = _desc.motion.accelerationIntegratedVelocityScaleByLife.curveKeyTimesBlock1,
        .accelerationIntegratedVelocityScaleByLifeCurveValues = _desc.motion.accelerationIntegratedVelocityScaleByLife.curveKeyValues,
        .accelerationIntegratedVelocityScaleByLifeCurveValuesBlock1 = _desc.motion.accelerationIntegratedVelocityScaleByLife.curveKeyValuesBlock1,
        .accelerationIntegratedVelocityScaleByLifeCurveArriveTangents = _desc.motion.accelerationIntegratedVelocityScaleByLife.curveKeyArriveTangents,
        .accelerationIntegratedVelocityScaleByLifeCurveArriveTangentsBlock1 = _desc.motion.accelerationIntegratedVelocityScaleByLife.curveKeyArriveTangentsBlock1,
        .accelerationIntegratedVelocityScaleByLifeCurveLeaveTangents = _desc.motion.accelerationIntegratedVelocityScaleByLife.curveKeyLeaveTangents,
        .accelerationIntegratedVelocityScaleByLifeCurveLeaveTangentsBlock1 = _desc.motion.accelerationIntegratedVelocityScaleByLife.curveKeyLeaveTangentsBlock1,
        .accelerationIntegratedVelocityScaleByLifeCurveModes = _desc.motion.accelerationIntegratedVelocityScaleByLife.curveKeyModes,
        .accelerationIntegratedVelocityScaleByLifeCurveModesBlock1 = _desc.motion.accelerationIntegratedVelocityScaleByLife.curveKeyModesBlock1,
        .accelerationIntegratedVelocityScaleByLifeCurveParams = Build_FloatCurveParams(_desc.motion.accelerationIntegratedVelocityScaleByLife)
    };

    _context->UpdateSubresource(_computeConstantBuffer.Get(), 0, nullptr, &params, 0, 0);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Reset_IndirectArgs()
{
    if (nullptr == _computeArgsOutput)
        return E_FAIL;

    const ComputeIndirectArgs args{
        .value0 = 1u,
        .instanceCount = 0u,
        .value2 = 0u,
        .value3 = 0,
        .startInstanceLocation = 0u
    };

    return _computeArgsOutput->Update_Data(&args, 1);
}

HRESULT VIBufferCom_ComputePointParticle::Copy_OutputToInstanceVB()
{
    if (nullptr == _computeOutput || nullptr == _instanceVB)
        return E_FAIL;

    _context->CopyResource(_instanceVB.Get(), _computeOutput->Get_Buffer());
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Copy_OutputToIndirectArgs()
{
    if (PointParticleDrawMode::Direct == _desc.drawMode)
        return S_OK;

    if (nullptr == _computeArgsOutput || nullptr == _indirectArgsBuffer)
        return E_FAIL;

    _context->CopyResource(_indirectArgsBuffer.Get(), _computeArgsOutput->Get_Buffer());

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_ResetBaseline()
{
    _initialLifecycleState.clear();
    _emptyInstancePayload.clear();

    _initialLifecycleState.reserve(_instanceCount);
    for (uint32 index = 0; index < _instanceCount; ++index)
        _initialLifecycleState.push_back(Make_InitialLifecycleState(index));

    _emptyInstancePayload.resize(_instanceCount);
    return S_OK;
}

VIBufferCom_ComputePointParticle::ParticleLifecycleState VIBufferCom_ComputePointParticle::Make_InitialLifecycleState(uint32 index) const
{
    const float ratio = (static_cast<float>(index) + 0.5f) / max(1.f, static_cast<float>(_instanceCount));
    const float lifeMin = max(0.0001f, min(_desc.lifeTime.x, _desc.lifeTime.y));
    const float lifeMax = max(lifeMin, max(_desc.lifeTime.x, _desc.lifeTime.y));
    const uint32 respawnSeed = index * 9781u + 1u;
    const uint32 lifetimeSeedSalt = Resolve_SeedSalt(_desc.lifetimeSeed, _desc.effectPlaybackSeed);
    const bool useRandomLifetimeSample = _desc.lifetimeSeed.manualSeedEnabled || _desc.lifetimeSeed.useInstanceSeed;
    const float lifeSample = useRandomLifetimeSample
                             ? Hash01(respawnSeed + lifetimeSeedSalt + 43u)
                             : ratio;
    const float sampledLifeMax = lerp(lifeMin, lifeMax, lifeSample);

    return ParticleLifecycleState{
        .lifeAge = 0.f,
        .lifeMax = sampledLifeMax,
        .respawnSeed = respawnSeed,
        .active = 0u,
        .placementIndex = 0u,
        .placementCount = 1u,
        .reserved0 = 0u,
        .reserved1 = 0u
    };
}

Shared<VIBufferCom_ComputePointParticle> VIBufferCom_ComputePointParticle::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<VIBufferCom_ComputePointParticle>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : VIBufferCom_ComputePointParticle");
        return nullptr;
    }

    return instance;
}

Shared<Component> VIBufferCom_ComputePointParticle::Clone(void* arg)
{
    auto instance = make_shared<VIBufferCom_ComputePointParticle>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : VIBufferCom_ComputePointParticle");
        MSG_BOX("Failed to Clone : VIBufferCom_ComputePointParticle");
        return nullptr;
    }

    return instance;
}

void VIBufferCom_ComputePointParticle::Free()
{
    _indirectArgsBuffer.Reset();
    _computeConstantBuffer.Reset();
    _lifecycleState.reset();
    _computeArgsOutput.reset();
    _computeOutput.reset();
    _computeShader.reset();
    _emptyInstancePayload.clear();
    _initialLifecycleState.clear();

    __super::Free();
}
