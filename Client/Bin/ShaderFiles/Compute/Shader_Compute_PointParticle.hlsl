// Debug smoke shader for compute-driven instancing.
// C++ binds OutputData as UAV(u0), then copies it into a VERTEX_BUFFER.
// LifecycleState(u2)는 particle별 age/lifetime을 frame 사이에 유지한다.

struct ParticleInstanceVertex
{
    float4 right;
    float4 up;
    float4 look;
    float4 translation;
    float2 lifeTime;
    float4 startColor;
    float4 endColor;
    float4 subUVRect;
    float2 spriteTiltDegrees;
    float4 coreColorRgb;
};

struct ParticleLifecycleState
{
    float lifeAge;
    float lifeMax;
    uint respawnSeed;
    uint active;
    uint placementIndex;
    uint placementCount;
    uint reserved0;
    uint reserved1;
};

cbuffer ComputePointParticleParams : register(b0)
{
    float g_ElapsedTime;
    float g_DeltaTime;
    float g_InstanceCount;
    uint g_DrawMode;
    uint g_SpawnRequest;
    uint g_MaxActiveCount;
    uint g_MaxDrawCount;
    uint g_KillActiveParticles;
    float4 g_Center;
    float4 g_EmitterBasisRight;                  // xyz: emitter local X axis in world space
    float4 g_EmitterBasisUp;                     // xyz: emitter local Y axis in world space
    float4 g_EmitterBasisLook;                   // xyz: emitter local Z axis in world space
    float4 g_SpawnRange;
    float4 g_InitialLocationMin;                 // xyz: InitialLocation min offset, w: enabled
    float4 g_InitialLocationMax;                 // xyz: InitialLocation max offset
    float4 g_SphereLocationOffsetRadius;         // xyz: sphere center offset, w: radius
    float4 g_SphereLocationParams;               // x: enabled, y: mode(0 volume, 1 surface), z: placement mode(0 random, 1 even index)
    float4 g_PlaneRadialLocationParams;          // x: enabled, y: plane, z: shape, w: placement mode
    float4 g_PlaneRadialLocationOffsetThickness; // xyz: offset, w: thickness
    float4 g_PlaneRadialLocationRectRange;       // x/y: U range, z/w: V range
    float4 g_PlaneRadialLocationPolarRange;      // x/y: radius range, z/w: angle degree range
    uint4 g_PlaneRadialLocationSeedSalts;        // x: U/radius, y: V/angle, z/w: unused
    float4 g_PlaneRadialOrientationParams;       // x: enabled, y: target kind, z: orientation mode
    float4 g_SizeMin;                            // x/y: width/height min
    float4 g_SizeMax;                            // x/y: width/height max
    float4 g_LifeTimeParams;                     // x/y: lifetime min/max
    float4 g_LifetimeCurveParams;                // x: key count, y: enabled, z: sampling phase
    float4 g_LifetimeCurveTimes;
    float4 g_LifetimeCurveTimesBlock1;
    float4 g_LifetimeCurveValues;
    float4 g_LifetimeCurveValuesBlock1;
    float4 g_LifetimeCurveArriveTangents;
    float4 g_LifetimeCurveArriveTangentsBlock1;
    float4 g_LifetimeCurveLeaveTangents;
    float4 g_LifetimeCurveLeaveTangentsBlock1;
    float4 g_LifetimeCurveModes;
    float4 g_LifetimeCurveModesBlock1;
    float4 g_StartColorMin;
    float4 g_StartColorMax;
    float4 g_EndColorMin;
    float4 g_EndColorMax;
    float4 g_ColorOverLifeCurveParams;           // x: color key count, y: alpha key count, z: color enabled, w: alpha enabled
    float4 g_ColorOverLifeColorCurveTimes;
    float4 g_ColorOverLifeColorCurveTimesBlock1; // 커브 Key를 4개 -> 8개로 확장하면서 Block1들 추가됨
    float4 g_ColorOverLifeColorCurveValuesR;
    float4 g_ColorOverLifeColorCurveValuesRBlock1;
    float4 g_ColorOverLifeColorCurveValuesG;
    float4 g_ColorOverLifeColorCurveValuesGBlock1;
    float4 g_ColorOverLifeColorCurveValuesB;
    float4 g_ColorOverLifeColorCurveValuesBBlock1;
    float4 g_ColorOverLifeColorCurveArriveR;
    float4 g_ColorOverLifeColorCurveArriveRBlock1;
    float4 g_ColorOverLifeColorCurveArriveG;
    float4 g_ColorOverLifeColorCurveArriveGBlock1;
    float4 g_ColorOverLifeColorCurveArriveB;
    float4 g_ColorOverLifeColorCurveArriveBBlock1;
    float4 g_ColorOverLifeColorCurveLeaveR;
    float4 g_ColorOverLifeColorCurveLeaveRBlock1;
    float4 g_ColorOverLifeColorCurveLeaveG;
    float4 g_ColorOverLifeColorCurveLeaveGBlock1;
    float4 g_ColorOverLifeColorCurveLeaveB;
    float4 g_ColorOverLifeColorCurveLeaveBBlock1;
    float4 g_ColorOverLifeColorCurveModes;
    float4 g_ColorOverLifeColorCurveModesBlock1;
    float4 g_ColorOverLifeAlphaCurveTimes;
    float4 g_ColorOverLifeAlphaCurveTimesBlock1;
    float4 g_ColorOverLifeAlphaCurveValues;
    float4 g_ColorOverLifeAlphaCurveValuesBlock1;
    float4 g_ColorOverLifeAlphaCurveArrive;
    float4 g_ColorOverLifeAlphaCurveArriveBlock1;
    float4 g_ColorOverLifeAlphaCurveLeave;
    float4 g_ColorOverLifeAlphaCurveLeaveBlock1;
    float4 g_ColorOverLifeAlphaCurveModes;
    float4 g_ColorOverLifeAlphaCurveModesBlock1;
    float4 g_CoreColorRgbParticleLifeUniformParams; // x: enabled
    float4 g_CoreColorRgbParticleLifeUniformMin;
    float4 g_CoreColorRgbParticleLifeUniformMax;
    uint4 g_CoreColorRgbParticleLifeUniformSeed;    // x: seed salt after playback-seed resolution
    float4 g_SubUVParams;                        // x/y: rows/cols, z: playback mode, w: frames per second
    float4 g_SubUVFrameParams;                   // x/y: start/end frame, z: loop, w: enabled
    float4 g_SubUVFrameCurveTimes;               // SubUV frame curve key times
    float4 g_SubUVFrameCurveTimesBlock1;
    float4 g_SubUVFrameCurveValues;              // SubUV frame curve frame values
    float4 g_SubUVFrameCurveValuesBlock1;
    float4 g_SubUVFrameCurveParams;              // x: key count, y: random start phase flag
    uint4 g_SpawnSeedSalts;                      // x: Lifetime, y: InitialLocation, z: SphereLocation, w: unused
    uint4 g_SubUVSeedSalts;                      // x: SubUV RandomFrame
    uint4 g_AppearanceSeedSalts0;                // x: InitialSize, y: InitialColor RGB, z: InitialColor Alpha, w: ColorOverLife RGB
    uint4 g_AppearanceSeedSalts1;                // x: ColorOverLife Alpha
    float4 g_SizeByLifeParams;                   // x/y: width start/end, z/w: height start/end
    float4 g_SizeByLifeCurveTimes;               // scaleOverLife key times
    float4 g_SizeByLifeCurveTimesBlock1;
    float4 g_SizeByLifeCurveValuesX;             // scaleOverLife X multipliers
    float4 g_SizeByLifeCurveValuesXBlock1;
    float4 g_SizeByLifeCurveValuesY;             // scaleOverLife Y multipliers
    float4 g_SizeByLifeCurveValuesYBlock1;
    float4 g_SizeByLifeCurveArriveTangentsX;
    float4 g_SizeByLifeCurveArriveTangentsXBlock1;
    float4 g_SizeByLifeCurveLeaveTangentsX;
    float4 g_SizeByLifeCurveLeaveTangentsXBlock1;
    float4 g_SizeByLifeCurveArriveTangentsY;
    float4 g_SizeByLifeCurveArriveTangentsYBlock1;
    float4 g_SizeByLifeCurveLeaveTangentsY;
    float4 g_SizeByLifeCurveLeaveTangentsYBlock1;
    float4 g_SizeByLifeCurveModes;
    float4 g_SizeByLifeCurveModesBlock1;
    float4 g_SizeByLifeCurveParams;              // x: key count, y: multiplyX, z: multiplyY, w: axisLock
    float4 g_SpriteTiltInitial;                  // x/y: initial min, z/w: initial max
    float4 g_SpriteTiltOverLife;                 // x/y: over-life min, z/w: over-life max
    float4 g_SpriteTiltCurveTimes;
    float4 g_SpriteTiltCurveTimesBlock1;
    float4 g_SpriteTiltCurveValuesX;
    float4 g_SpriteTiltCurveValuesXBlock1;
    float4 g_SpriteTiltCurveValuesY;
    float4 g_SpriteTiltCurveValuesYBlock1;
    float4 g_SpriteTiltCurveArriveTangentsX;
    float4 g_SpriteTiltCurveArriveTangentsXBlock1;
    float4 g_SpriteTiltCurveLeaveTangentsX;
    float4 g_SpriteTiltCurveLeaveTangentsXBlock1;
    float4 g_SpriteTiltCurveArriveTangentsY;
    float4 g_SpriteTiltCurveArriveTangentsYBlock1;
    float4 g_SpriteTiltCurveLeaveTangentsY;
    float4 g_SpriteTiltCurveLeaveTangentsYBlock1;
    float4 g_SpriteTiltCurveModes;
    float4 g_SpriteTiltCurveModesBlock1;
    float4 g_SpriteTiltParams;                   // x: initial enabled, y: over-life enabled, z: key count, w: curve enabled
    uint4 g_SpriteTiltSeedSalts;                 // x: initial tilt, y: over-life uniform
    float4 g_MotionFlags;                        // x: motion enabled, y: vector enabled, z: vector world, w: radial enabled
    float4 g_MotionSpaceFlags;                   // x: radial world, y: cone enabled, z: cone world, w: radial center direction mode
    float4 g_InitialVelocityMin;                 // xyz: vector velocity min
    float4 g_InitialVelocityMax;                 // xyz: vector velocity max
    float4 g_MotionRadialPivot;                  // xyz: radial pivot
    float4 g_MotionRadialSpeedDrag;              // x/y: radial speed min/max, z/w: drag min/max
    float4 g_VelocityConeAxisAngle;              // xyz: cone axis, w: half-angle degrees
    float4 g_VelocityConeSpeed;                  // x/y: speed min/max
    float4 g_AccelerationMin;
    float4 g_AccelerationMax;
    float4 g_AccelerationCurveTimes;             // Acceleration key times
    float4 g_AccelerationCurveTimesBlock1;
    float4 g_AccelerationCurveValuesX;           // Acceleration X key values
    float4 g_AccelerationCurveValuesXBlock1;
    float4 g_AccelerationCurveValuesY;           // Acceleration Y key values
    float4 g_AccelerationCurveValuesYBlock1;
    float4 g_AccelerationCurveValuesZ;           // Acceleration Z key values
    float4 g_AccelerationCurveValuesZBlock1;
    float4 g_AccelerationCurveArriveTangentsX;
    float4 g_AccelerationCurveArriveTangentsXBlock1;
    float4 g_AccelerationCurveLeaveTangentsX;
    float4 g_AccelerationCurveLeaveTangentsXBlock1;
    float4 g_AccelerationCurveArriveTangentsY;
    float4 g_AccelerationCurveArriveTangentsYBlock1;
    float4 g_AccelerationCurveLeaveTangentsY;
    float4 g_AccelerationCurveLeaveTangentsYBlock1;
    float4 g_AccelerationCurveArriveTangentsZ;
    float4 g_AccelerationCurveArriveTangentsZBlock1;
    float4 g_AccelerationCurveLeaveTangentsZ;
    float4 g_AccelerationCurveLeaveTangentsZBlock1;
    float4 g_AccelerationCurveModes;
    float4 g_AccelerationCurveModesBlock1;
    float4 g_AccelerationCurveParams;            // x: key count, y: enabled, z: acceleration world, w: emitter-time duration
    float4 g_VelocityScaleByLife;
    float4 g_VelocityScaleByLifeCurveTimes;      // VelocityOverLife key times
    float4 g_VelocityScaleByLifeCurveTimesBlock1;
    float4 g_VelocityScaleByLifeCurveValues;     // VelocityOverLife multiplier values
    float4 g_VelocityScaleByLifeCurveValuesBlock1;
    float4 g_VelocityScaleByLifeCurveArriveTangents;
    float4 g_VelocityScaleByLifeCurveArriveTangentsBlock1;
    float4 g_VelocityScaleByLifeCurveLeaveTangents;
    float4 g_VelocityScaleByLifeCurveLeaveTangentsBlock1;
    float4 g_VelocityScaleByLifeCurveModes;
    float4 g_VelocityScaleByLifeCurveModesBlock1;
    float4 g_VelocityScaleByLifeCurveParams;     // x: key count
    float4 g_OrbitParams;                        // x: enabled, y: plane
    float4 g_OrbitAngleRange;                    // x/y: angle start/end degrees
    float4 g_OrbitAngleCurveTimes;               // OrbitOverLife angle key times
    float4 g_OrbitAngleCurveTimesBlock1;
    float4 g_OrbitAngleCurveValues;              // OrbitOverLife angle degree values
    float4 g_OrbitAngleCurveValuesBlock1;
    float4 g_OrbitAngleCurveArriveTangents;
    float4 g_OrbitAngleCurveArriveTangentsBlock1;
    float4 g_OrbitAngleCurveLeaveTangents;
    float4 g_OrbitAngleCurveLeaveTangentsBlock1;
    float4 g_OrbitAngleCurveModes;
    float4 g_OrbitAngleCurveModesBlock1;
    float4 g_OrbitAngleCurveParams;              // x: key count
    float4 g_OrbitRadiusRange;                   // x/y: radius scale start/end
    float4 g_OrbitRadiusCurveTimes;              // OrbitOverLife radius scale key times
    float4 g_OrbitRadiusCurveTimesBlock1;
    float4 g_OrbitRadiusCurveValues;             // OrbitOverLife radius scale values
    float4 g_OrbitRadiusCurveValuesBlock1;
    float4 g_OrbitRadiusCurveArriveTangents;
    float4 g_OrbitRadiusCurveArriveTangentsBlock1;
    float4 g_OrbitRadiusCurveLeaveTangents;
    float4 g_OrbitRadiusCurveLeaveTangentsBlock1;
    float4 g_OrbitRadiusCurveModes;
    float4 g_OrbitRadiusCurveModesBlock1;
    float4 g_OrbitRadiusCurveParams;             // x: key count
    uint4 g_MotionSeedSalts0;                    // x: InitialVelocity, y: InitialRadialVelocity, z: VelocityCone, w: Acceleration
    uint4 g_MotionSeedSalts1;                    // x: Drag
    float4 g_RotationInitialRate;                // x/y: initial rotation min/max, z/w: rate min/max
    float4 g_RotationByLife;                     // (Legacy) x/y: rotation start/end, z/w: rate scale start/end
    float4 g_RotationOverLifeCurveTimes;         // rotationOverLife key times
    float4 g_RotationOverLifeCurveTimesBlock1;
    float4 g_RotationOverLifeCurveValues;        // rotationOverLife degree values
    float4 g_RotationOverLifeCurveValuesBlock1;
    float4 g_RotationOverLifeCurveArriveTangents;
    float4 g_RotationOverLifeCurveArriveTangentsBlock1;
    float4 g_RotationOverLifeCurveLeaveTangents;
    float4 g_RotationOverLifeCurveLeaveTangentsBlock1;
    float4 g_RotationOverLifeCurveModes;
    float4 g_RotationOverLifeCurveModesBlock1;
    float4 g_RotationOverLifeCurveParams;        // x: key count
    float4 g_RotationRateScaleByLifeCurveTimes;  // rotationRateScaleByLife key times
    float4 g_RotationRateScaleByLifeCurveTimesBlock1;
    float4 g_RotationRateScaleByLifeCurveValues; // rotationRateScaleByLife multiplier values
    float4 g_RotationRateScaleByLifeCurveValuesBlock1;
    float4 g_RotationRateScaleByLifeCurveArriveTangents;
    float4 g_RotationRateScaleByLifeCurveArriveTangentsBlock1;
    float4 g_RotationRateScaleByLifeCurveLeaveTangents;
    float4 g_RotationRateScaleByLifeCurveLeaveTangentsBlock1;
    float4 g_RotationRateScaleByLifeCurveModes;
    float4 g_RotationRateScaleByLifeCurveModesBlock1;
    float4 g_RotationRateScaleByLifeCurveParams; // x: key count
    uint4 g_RotationSeedSalts;                   // x: InitialRotation, y: InitialRotationRate
    float4 g_PlaneRadialCameraFacingRight;       // xyz: camera right axis in emitter-local sampling space
    float4 g_PlaneRadialCameraFacingUp;          // xyz: camera up axis in emitter-local sampling space
    float4 g_PlaneRadialCameraFacingNormal;      // xyz: camera look axis in emitter-local sampling space
    uint4 g_SpawnSerialParams;                   // x: replay-local spawn serial base
    float4 g_SourceMotionVelocity;               // xyz: emitter center delta velocity, w: valid
    float4 g_SourceMotionVelocityParams;         // x: enabled, y: direction mode, z: spread angle degrees, w: source speed scale
    float4 g_SourceMotionVelocitySpeed;          // x/y: speed min/max
    uint4 g_SourceMotionVelocitySeedSalts;       // x: SourceMotionVelocity
    float4 g_CylinderLocationParams;             // x: enabled, y: axis, z: mode, w: placement mode
    float4 g_CylinderLocationOffset;             // xyz: offset
    float4 g_CylinderLocationRadiusHeight;       // x/y: radius range, z/w: height range
    float4 g_CylinderLocationAngle;              // x/y: angle degree range
    uint4 g_CylinderLocationSeedSalts;           // x: radius, y: height, z: angle
    float4 g_CylinderOrientationParams;          // x: enabled, y: target kind, z: orientation mode, w: follow orbit over life
    float4 g_CylinderOrientationAngles;          // x: tilt degrees, y: roll offset degrees
    float4 g_InitialVelocityScaleByLifeCurveTimes;
    float4 g_InitialVelocityScaleByLifeCurveTimesBlock1;
    float4 g_InitialVelocityScaleByLifeCurveValues;
    float4 g_InitialVelocityScaleByLifeCurveValuesBlock1;
    float4 g_InitialVelocityScaleByLifeCurveArriveTangents;
    float4 g_InitialVelocityScaleByLifeCurveArriveTangentsBlock1;
    float4 g_InitialVelocityScaleByLifeCurveLeaveTangents;
    float4 g_InitialVelocityScaleByLifeCurveLeaveTangentsBlock1;
    float4 g_InitialVelocityScaleByLifeCurveModes;
    float4 g_InitialVelocityScaleByLifeCurveModesBlock1;
    float4 g_InitialVelocityScaleByLifeCurveParams; // x: key count, y: enabled
    float4 g_InitialRadialVelocityScaleByLifeCurveTimes;
    float4 g_InitialRadialVelocityScaleByLifeCurveTimesBlock1;
    float4 g_InitialRadialVelocityScaleByLifeCurveValues;
    float4 g_InitialRadialVelocityScaleByLifeCurveValuesBlock1;
    float4 g_InitialRadialVelocityScaleByLifeCurveArriveTangents;
    float4 g_InitialRadialVelocityScaleByLifeCurveArriveTangentsBlock1;
    float4 g_InitialRadialVelocityScaleByLifeCurveLeaveTangents;
    float4 g_InitialRadialVelocityScaleByLifeCurveLeaveTangentsBlock1;
    float4 g_InitialRadialVelocityScaleByLifeCurveModes;
    float4 g_InitialRadialVelocityScaleByLifeCurveModesBlock1;
    float4 g_InitialRadialVelocityScaleByLifeCurveParams; // x: key count, y: enabled
    float4 g_VelocityConeScaleByLifeCurveTimes;
    float4 g_VelocityConeScaleByLifeCurveTimesBlock1;
    float4 g_VelocityConeScaleByLifeCurveValues;
    float4 g_VelocityConeScaleByLifeCurveValuesBlock1;
    float4 g_VelocityConeScaleByLifeCurveArriveTangents;
    float4 g_VelocityConeScaleByLifeCurveArriveTangentsBlock1;
    float4 g_VelocityConeScaleByLifeCurveLeaveTangents;
    float4 g_VelocityConeScaleByLifeCurveLeaveTangentsBlock1;
    float4 g_VelocityConeScaleByLifeCurveModes;
    float4 g_VelocityConeScaleByLifeCurveModesBlock1;
    float4 g_VelocityConeScaleByLifeCurveParams; // x: key count, y: enabled
    float4 g_SourceMotionVelocityScaleByLifeCurveTimes;
    float4 g_SourceMotionVelocityScaleByLifeCurveTimesBlock1;
    float4 g_SourceMotionVelocityScaleByLifeCurveValues;
    float4 g_SourceMotionVelocityScaleByLifeCurveValuesBlock1;
    float4 g_SourceMotionVelocityScaleByLifeCurveArriveTangents;
    float4 g_SourceMotionVelocityScaleByLifeCurveArriveTangentsBlock1;
    float4 g_SourceMotionVelocityScaleByLifeCurveLeaveTangents;
    float4 g_SourceMotionVelocityScaleByLifeCurveLeaveTangentsBlock1;
    float4 g_SourceMotionVelocityScaleByLifeCurveModes;
    float4 g_SourceMotionVelocityScaleByLifeCurveModesBlock1;
    float4 g_SourceMotionVelocityScaleByLifeCurveParams; // x: key count, y: enabled
    float4 g_AccelerationIntegratedVelocityScaleByLifeCurveTimes;
    float4 g_AccelerationIntegratedVelocityScaleByLifeCurveTimesBlock1;
    float4 g_AccelerationIntegratedVelocityScaleByLifeCurveValues;
    float4 g_AccelerationIntegratedVelocityScaleByLifeCurveValuesBlock1;
    float4 g_AccelerationIntegratedVelocityScaleByLifeCurveArriveTangents;
    float4 g_AccelerationIntegratedVelocityScaleByLifeCurveArriveTangentsBlock1;
    float4 g_AccelerationIntegratedVelocityScaleByLifeCurveLeaveTangents;
    float4 g_AccelerationIntegratedVelocityScaleByLifeCurveLeaveTangentsBlock1;
    float4 g_AccelerationIntegratedVelocityScaleByLifeCurveModes;
    float4 g_AccelerationIntegratedVelocityScaleByLifeCurveModesBlock1;
    float4 g_AccelerationIntegratedVelocityScaleByLifeCurveParams; // x: key count, y: enabled
};

RWStructuredBuffer<ParticleInstanceVertex> OutputData : register(u0);
RWStructuredBuffer<ParticleLifecycleState> LifecycleState : register(u2);

static const float kPlaneRadialAngleEpsilon = 1e-4f;
static const uint kEffectDistributionCurveMaxKeys = 8u;

float ReadVec4Component(float4 values, uint index)
{
    float result = values.x;
    if (index == 1u)
        result = values.y;
    else if (index == 2u)
        result = values.z;
    else if (index == 3u)
        result = values.w;

    return result;
}

float ReadVec4Component(float4 valuesBlock0, float4 valuesBlock1, uint index)
{
    return index < 4u
           ? ReadVec4Component(valuesBlock0, index)
           : ReadVec4Component(valuesBlock1, index - 4u);
}

void AccumulateAccelerationSegment(
    float leftTime,
    float rightTime,
    float leftValue,
    float rightValue,
    float lifeProgress,
    inout float integral,
    inout float weightedIntegral)
{
    const float segmentEnd = min(lifeProgress, rightTime);
    if (segmentEnd <= leftTime)
        return;

    const float width = max(0.0001f, rightTime - leftTime);
    const float slope = (rightValue - leftValue) / width;
    const float dt = segmentEnd - leftTime;
    const float start2 = leftTime * leftTime;
    const float end2 = segmentEnd * segmentEnd;
    const float start3 = start2 * leftTime;
    const float end3 = end2 * segmentEnd;

    integral += leftValue * dt + 0.5f * slope * dt * dt;
    weightedIntegral +=
        0.5f * leftValue * (end2 - start2) +
        slope * ((end3 - start3) / 3.0f - 0.5f * leftTime * (end2 - start2));
}

void AccumulateAccelerationConstantSegment(
    float leftTime,
    float rightTime,
    float value,
    float lifeProgress,
    inout float integral,
    inout float weightedIntegral)
{
    AccumulateAccelerationSegment(leftTime, rightTime, value, value, lifeProgress, integral, weightedIntegral);
}

void AccumulateAccelerationHermiteSegment(
    float leftTime,
    float rightTime,
    float leftValue,
    float rightValue,
    float leftLeaveTangent,
    float rightArriveTangent,
    float lifeProgress,
    inout float integral,
    inout float weightedIntegral)
{
    const float segmentEnd = min(lifeProgress, rightTime);
    if (segmentEnd <= leftTime)
        return;

    const float width = max(0.0001f, rightTime - leftTime);
    const float s = saturate((segmentEnd - leftTime) / width);
    const float s2 = s * s;
    const float s3 = s2 * s;
    const float s4 = s3 * s;
    const float s5 = s4 * s;
    const float m0 = leftLeaveTangent * width;
    const float m1 = rightArriveTangent * width;

    const float h00 = 0.5f * s4 - s3 + s;
    const float h10 = 0.25f * s4 - 2.0f / 3.0f * s3 + 0.5f * s2;
    const float h01 = -0.5f * s4 + s3;
    const float h11 = 0.25f * s4 - 1.0f / 3.0f * s3;
    const float segmentIntegral = width * (leftValue * h00 + m0 * h10 + rightValue * h01 + m1 * h11);

    const float wh00 = 2.0f / 5.0f * s5 - 0.75f * s4 + 0.5f * s2;
    const float wh10 = 0.2f * s5 - 0.5f * s4 + 1.0f / 3.0f * s3;
    const float wh01 = -(2.0f / 5.0f) * s5 + 0.75f * s4;
    const float wh11 = 0.2f * s5 - 0.25f * s4;
    const float localWeighted =
        leftValue * wh00 +
        m0 * wh10 +
        rightValue * wh01 +
        m1 * wh11;

    integral += segmentIntegral;
    weightedIntegral += leftTime * segmentIntegral + width * width * localWeighted;
}

void AccumulateAccelerationCurveSegment(
    float leftTime,
    float rightTime,
    float leftValue,
    float rightValue,
    float leftLeaveTangent,
    float rightArriveTangent,
    float mode,
    float lifeProgress,
    inout float integral,
    inout float weightedIntegral)
{
    if (mode < 0.5f)
        AccumulateAccelerationConstantSegment(leftTime, rightTime, leftValue, lifeProgress, integral, weightedIntegral);
    else if (mode >= 1.5f)
    {
        AccumulateAccelerationHermiteSegment(
            leftTime,
            rightTime,
            leftValue,
            rightValue,
            leftLeaveTangent,
            rightArriveTangent,
            lifeProgress,
            integral,
            weightedIntegral
        );
    }
    else
        AccumulateAccelerationSegment(leftTime, rightTime, leftValue, rightValue, lifeProgress, integral, weightedIntegral);
}

void IntegrateAccelerationAxis(
    float lifeProgress,
    uint keyCount,
    float4 times,
    float4 timesBlock1,
    float4 values,
    float4 valuesBlock1,
    float4 arriveTangents,
    float4 arriveTangentsBlock1,
    float4 leaveTangents,
    float4 leaveTangentsBlock1,
    float4 modes,
    float4 modesBlock1,
    out float velocityProgress,
    out float displacementProgress)
{
    keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, keyCount));
    lifeProgress = saturate(lifeProgress);

    float integral = 0.0f;
    float weightedIntegral = 0.0f;
    const float firstTime = saturate(ReadVec4Component(times, timesBlock1, 0u));
    const float firstValue = ReadVec4Component(values, valuesBlock1, 0u);

    AccumulateAccelerationSegment(0.0f, firstTime, firstValue, firstValue, lifeProgress, integral, weightedIntegral);

    [loop]
    for (uint keyIndex = 1u; keyIndex < keyCount; ++keyIndex)
    {
        const float leftTime = saturate(ReadVec4Component(times, timesBlock1, keyIndex - 1u));
        const float rightTime = saturate(ReadVec4Component(times, timesBlock1, keyIndex));
        const float leftValue = ReadVec4Component(values, valuesBlock1, keyIndex - 1u);
        const float rightValue = ReadVec4Component(values, valuesBlock1, keyIndex);
        const float leftLeave = ReadVec4Component(leaveTangents, leaveTangentsBlock1, keyIndex - 1u);
        const float rightArrive = ReadVec4Component(arriveTangents, arriveTangentsBlock1, keyIndex);
        const float mode = ReadVec4Component(modes, modesBlock1, keyIndex - 1u);
        AccumulateAccelerationCurveSegment(
            leftTime,
            rightTime,
            leftValue,
            rightValue,
            leftLeave,
            rightArrive,
            mode,
            lifeProgress,
            integral,
            weightedIntegral
        );
    }

    const float lastTime = saturate(ReadVec4Component(times, timesBlock1, keyCount - 1u));
    const float lastValue = ReadVec4Component(values, valuesBlock1, keyCount - 1u);
    AccumulateAccelerationSegment(lastTime, lifeProgress, lastValue, lastValue, lifeProgress, integral, weightedIntegral);

    velocityProgress = integral;
    displacementProgress = lifeProgress * integral - weightedIntegral;
}

void IntegrateAccelerationCurve(
    float lifeProgress,
    out float3 velocityProgress,
    out float3 displacementProgress)
{
    const uint keyCount = (uint)g_AccelerationCurveParams.x;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float velocityZ = 0.0f;
    float displacementX = 0.0f;
    float displacementY = 0.0f;
    float displacementZ = 0.0f;
    IntegrateAccelerationAxis(
        lifeProgress,
        keyCount,
        g_AccelerationCurveTimes,
        g_AccelerationCurveTimesBlock1,
        g_AccelerationCurveValuesX,
        g_AccelerationCurveValuesXBlock1,
        g_AccelerationCurveArriveTangentsX,
        g_AccelerationCurveArriveTangentsXBlock1,
        g_AccelerationCurveLeaveTangentsX,
        g_AccelerationCurveLeaveTangentsXBlock1,
        g_AccelerationCurveModes,
        g_AccelerationCurveModesBlock1,
        velocityX,
        displacementX
    );
    IntegrateAccelerationAxis(
        lifeProgress,
        keyCount,
        g_AccelerationCurveTimes,
        g_AccelerationCurveTimesBlock1,
        g_AccelerationCurveValuesY,
        g_AccelerationCurveValuesYBlock1,
        g_AccelerationCurveArriveTangentsY,
        g_AccelerationCurveArriveTangentsYBlock1,
        g_AccelerationCurveLeaveTangentsY,
        g_AccelerationCurveLeaveTangentsYBlock1,
        g_AccelerationCurveModes,
        g_AccelerationCurveModesBlock1,
        velocityY,
        displacementY
    );
    IntegrateAccelerationAxis(
        lifeProgress,
        keyCount,
        g_AccelerationCurveTimes,
        g_AccelerationCurveTimesBlock1,
        g_AccelerationCurveValuesZ,
        g_AccelerationCurveValuesZBlock1,
        g_AccelerationCurveArriveTangentsZ,
        g_AccelerationCurveArriveTangentsZBlock1,
        g_AccelerationCurveLeaveTangentsZ,
        g_AccelerationCurveLeaveTangentsZBlock1,
        g_AccelerationCurveModes,
        g_AccelerationCurveModesBlock1,
        velocityZ,
        displacementZ
    );
    velocityProgress = float3(velocityX, velocityY, velocityZ);
    displacementProgress = float3(displacementX, displacementY, displacementZ);
}

void IntegrateAccelerationCurveRange(
    float startProgress,
    float endProgress,
    out float3 velocityProgress,
    out float3 displacementProgress)
{
    startProgress = saturate(startProgress);
    endProgress = saturate(endProgress);
    if (endProgress <= startProgress)
    {
        velocityProgress = float3(0.0f, 0.0f, 0.0f);
        displacementProgress = float3(0.0f, 0.0f, 0.0f);
        return;
    }

    float3 startVelocityProgress = float3(0.0f, 0.0f, 0.0f);
    float3 startDisplacementProgress = float3(0.0f, 0.0f, 0.0f);
    float3 endVelocityProgress = float3(0.0f, 0.0f, 0.0f);
    float3 endDisplacementProgress = float3(0.0f, 0.0f, 0.0f);
    IntegrateAccelerationCurve(startProgress, startVelocityProgress, startDisplacementProgress);
    IntegrateAccelerationCurve(endProgress, endVelocityProgress, endDisplacementProgress);

    velocityProgress = endVelocityProgress - startVelocityProgress;
    displacementProgress =
        endDisplacementProgress -
        startDisplacementProgress -
        (endProgress - startProgress) * startVelocityProgress;
}

float EvaluateCompactCurve(
    float lifeProgress,
    uint keyCount,
    float4 times,
    float4 timesBlock1,
    float4 values,
    float4 valuesBlock1,
    float4 arriveTangents,
    float4 arriveTangentsBlock1,
    float4 leaveTangents,
    float4 leaveTangentsBlock1,
    float4 modes,
    float4 modesBlock1,
    float fallbackValue)
{
    keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, keyCount));

    bool found = false;
    float result = ReadVec4Component(values, valuesBlock1, 0u);
    if (keyCount > 1u && lifeProgress > ReadVec4Component(times, timesBlock1, 0u))
    {
        result = fallbackValue;
        [loop]
        for (uint keyIndex = 1u; keyIndex < kEffectDistributionCurveMaxKeys; ++keyIndex)
        {
            if (keyIndex >= keyCount || found)
                break;

            const float rightTime = ReadVec4Component(times, timesBlock1, keyIndex);
            if (lifeProgress > rightTime)
                continue;

            const float leftTime = ReadVec4Component(times, timesBlock1, keyIndex - 1u);
            const float leftValue = ReadVec4Component(values, valuesBlock1, keyIndex - 1u);
            const float rightValue = ReadVec4Component(values, valuesBlock1, keyIndex);
            const float mode = ReadVec4Component(modes, modesBlock1, keyIndex - 1u);
            const float width = max(0.0001f, rightTime - leftTime);
            const float ratio = saturate((lifeProgress - leftTime) / width);

            if (mode < 0.5f)
                result = leftValue;
            else if (mode >= 1.5f)
            {
                const float t2 = ratio * ratio;
                const float t3 = t2 * ratio;
                const float leftLeave = ReadVec4Component(leaveTangents, leaveTangentsBlock1, keyIndex - 1u) * width;
                const float rightArrive = ReadVec4Component(arriveTangents, arriveTangentsBlock1, keyIndex) * width;
                result =
                    (2.0f * t3 - 3.0f * t2 + 1.0f) * leftValue +
                    (t3 - 2.0f * t2 + ratio) * leftLeave +
                    (-2.0f * t3 + 3.0f * t2) * rightValue +
                    (t3 - t2) * rightArrive;
            }
            else
                result = lerp(leftValue, rightValue, ratio);

            found = true;
            break;
        }

        const float lastTime = ReadVec4Component(times, timesBlock1, keyCount - 1u);
        if (!found && lifeProgress > lastTime)
            result = ReadVec4Component(values, valuesBlock1, keyCount - 1u);
    }

    return result;
}

float4 EvaluateColorOverLife(float lifeProgress, float4 fallbackColor)
{
    float4 result = fallbackColor;

    if (g_ColorOverLifeCurveParams.z != 0.0f)
    {
        const uint colorKeyCount = (uint)g_ColorOverLifeCurveParams.x;
        result.rgb = saturate(
            float3(
                EvaluateCompactCurve(
                    lifeProgress,
                    colorKeyCount,
                    g_ColorOverLifeColorCurveTimes,
                    g_ColorOverLifeColorCurveTimesBlock1,
                    g_ColorOverLifeColorCurveValuesR,
                    g_ColorOverLifeColorCurveValuesRBlock1,
                    g_ColorOverLifeColorCurveArriveR,
                    g_ColorOverLifeColorCurveArriveRBlock1,
                    g_ColorOverLifeColorCurveLeaveR,
                    g_ColorOverLifeColorCurveLeaveRBlock1,
                    g_ColorOverLifeColorCurveModes,
                    g_ColorOverLifeColorCurveModesBlock1,
                    fallbackColor.r
                ),
                EvaluateCompactCurve(
                    lifeProgress,
                    colorKeyCount,
                    g_ColorOverLifeColorCurveTimes,
                    g_ColorOverLifeColorCurveTimesBlock1,
                    g_ColorOverLifeColorCurveValuesG,
                    g_ColorOverLifeColorCurveValuesGBlock1,
                    g_ColorOverLifeColorCurveArriveG,
                    g_ColorOverLifeColorCurveArriveGBlock1,
                    g_ColorOverLifeColorCurveLeaveG,
                    g_ColorOverLifeColorCurveLeaveGBlock1,
                    g_ColorOverLifeColorCurveModes,
                    g_ColorOverLifeColorCurveModesBlock1,
                    fallbackColor.g
                ),
                EvaluateCompactCurve(
                    lifeProgress,
                    colorKeyCount,
                    g_ColorOverLifeColorCurveTimes,
                    g_ColorOverLifeColorCurveTimesBlock1,
                    g_ColorOverLifeColorCurveValuesB,
                    g_ColorOverLifeColorCurveValuesBBlock1,
                    g_ColorOverLifeColorCurveArriveB,
                    g_ColorOverLifeColorCurveArriveBBlock1,
                    g_ColorOverLifeColorCurveLeaveB,
                    g_ColorOverLifeColorCurveLeaveBBlock1,
                    g_ColorOverLifeColorCurveModes,
                    g_ColorOverLifeColorCurveModesBlock1,
                    fallbackColor.b
                )
            )
        );
    }

    if (g_ColorOverLifeCurveParams.w != 0.0f)
    {
        const uint alphaKeyCount = (uint)g_ColorOverLifeCurveParams.y;
        result.a = saturate(
            EvaluateCompactCurve(
                lifeProgress,
                alphaKeyCount,
                g_ColorOverLifeAlphaCurveTimes,
                g_ColorOverLifeAlphaCurveTimesBlock1,
                g_ColorOverLifeAlphaCurveValues,
                g_ColorOverLifeAlphaCurveValuesBlock1,
                g_ColorOverLifeAlphaCurveArrive,
                g_ColorOverLifeAlphaCurveArriveBlock1,
                g_ColorOverLifeAlphaCurveLeave,
                g_ColorOverLifeAlphaCurveLeaveBlock1,
                g_ColorOverLifeAlphaCurveModes,
                g_ColorOverLifeAlphaCurveModesBlock1,
                fallbackColor.a
            )
        );
    }

    return result;
}

float2 EvaluateSizeByLifeMultiplier(float lifeProgress)
{
    const uint keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, (uint)g_SizeByLifeCurveParams.x));
    float2 multiplier = float2(
        EvaluateCompactCurve(
            lifeProgress,
            keyCount,
            g_SizeByLifeCurveTimes,
            g_SizeByLifeCurveTimesBlock1,
            g_SizeByLifeCurveValuesX,
            g_SizeByLifeCurveValuesXBlock1,
            g_SizeByLifeCurveArriveTangentsX,
            g_SizeByLifeCurveArriveTangentsXBlock1,
            g_SizeByLifeCurveLeaveTangentsX,
            g_SizeByLifeCurveLeaveTangentsXBlock1,
            g_SizeByLifeCurveModes,
            g_SizeByLifeCurveModesBlock1,
            g_SizeByLifeParams.x
        ),
        EvaluateCompactCurve(
            lifeProgress,
            keyCount,
            g_SizeByLifeCurveTimes,
            g_SizeByLifeCurveTimesBlock1,
            g_SizeByLifeCurveValuesY,
            g_SizeByLifeCurveValuesYBlock1,
            g_SizeByLifeCurveArriveTangentsY,
            g_SizeByLifeCurveArriveTangentsYBlock1,
            g_SizeByLifeCurveLeaveTangentsY,
            g_SizeByLifeCurveLeaveTangentsYBlock1,
            g_SizeByLifeCurveModes,
            g_SizeByLifeCurveModesBlock1,
            g_SizeByLifeParams.z
        )
    );

    if ((uint)g_SizeByLifeCurveParams.w == 1u)
        multiplier.y = multiplier.x;
    else if ((uint)g_SizeByLifeCurveParams.w == 2u)
        multiplier.x = multiplier.y;

    if (g_SizeByLifeCurveParams.y == 0.0f)
        multiplier.x = 1.0f;
    if (g_SizeByLifeCurveParams.z == 0.0f)
        multiplier.y = 1.0f;

    return multiplier;
}

// RotationOverLife curve payload를 평가해 life-progress 시점의 추가 회전각을 반환한다.
float EvaluateRotationOverLifeDegrees(float lifeProgress)
{
    const uint keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, (uint)g_RotationOverLifeCurveParams.x));
    return EvaluateCompactCurve(
        lifeProgress,
        keyCount,
        g_RotationOverLifeCurveTimes,
        g_RotationOverLifeCurveTimesBlock1,
        g_RotationOverLifeCurveValues,
        g_RotationOverLifeCurveValuesBlock1,
        g_RotationOverLifeCurveArriveTangents,
        g_RotationOverLifeCurveArriveTangentsBlock1,
        g_RotationOverLifeCurveLeaveTangents,
        g_RotationOverLifeCurveLeaveTangentsBlock1,
        g_RotationOverLifeCurveModes,
        g_RotationOverLifeCurveModesBlock1,
        lerp(g_RotationByLife.x, g_RotationByLife.y, lifeProgress)
    );
}

float2 EvaluateSpriteTiltOverLifeDegrees(float lifeProgress, float2 sampledTiltOverLife)
{
    float2 result = float2(0.0f, 0.0f);
    if (g_SpriteTiltParams.y != 0.0f)
    {
        if (g_SpriteTiltParams.w == 0.0f)
            result = sampledTiltOverLife;
        else
        {
            const uint keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, (uint)g_SpriteTiltParams.z));
            const float2 fallbackTilt = lerp(g_SpriteTiltOverLife.xy, g_SpriteTiltOverLife.zw, lifeProgress);
            result.x = EvaluateCompactCurve(
                lifeProgress,
                keyCount,
                g_SpriteTiltCurveTimes,
                g_SpriteTiltCurveTimesBlock1,
                g_SpriteTiltCurveValuesX,
                g_SpriteTiltCurveValuesXBlock1,
                g_SpriteTiltCurveArriveTangentsX,
                g_SpriteTiltCurveArriveTangentsXBlock1,
                g_SpriteTiltCurveLeaveTangentsX,
                g_SpriteTiltCurveLeaveTangentsXBlock1,
                g_SpriteTiltCurveModes,
                g_SpriteTiltCurveModesBlock1,
                fallbackTilt.x
            );
            result.y = EvaluateCompactCurve(
                lifeProgress,
                keyCount,
                g_SpriteTiltCurveTimes,
                g_SpriteTiltCurveTimesBlock1,
                g_SpriteTiltCurveValuesY,
                g_SpriteTiltCurveValuesYBlock1,
                g_SpriteTiltCurveArriveTangentsY,
                g_SpriteTiltCurveArriveTangentsYBlock1,
                g_SpriteTiltCurveLeaveTangentsY,
                g_SpriteTiltCurveLeaveTangentsYBlock1,
                g_SpriteTiltCurveModes,
                g_SpriteTiltCurveModesBlock1,
                fallbackTilt.y
            );
        }
    }
    return result;
}

float IntegrateHermiteBasis0(float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float t4 = t3 * t;
    return 0.5f * t4 - t3 + t;
}

float IntegrateHermiteBasis1(float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float t4 = t3 * t;
    return 0.25f * t4 - 2.0f / 3.0f * t3 + 0.5f * t2;
}

float IntegrateHermiteBasis2(float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float t4 = t3 * t;
    return -0.5f * t4 + t3;
}

float IntegrateHermiteBasis3(float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float t4 = t3 * t;
    return 0.25f * t4 - 1.0f / 3.0f * t3;
}

float IntegrateCurveSegment(
    float leftTime,
    float leftValue,
    float rightTime,
    float rightValue,
    float leftLeaveTangent,
    float rightArriveTangent,
    float mode,
    float startTime,
    float endTime)
{
    const float width = max(0.0001f, rightTime - leftTime);
    const float t0 = saturate((startTime - leftTime) / width);
    const float t1 = saturate((endTime - leftTime) / width);
    float result = 0.0f;

    if (mode < 0.5f)
        result = leftValue * max(0.0f, endTime - startTime);
    else if (mode >= 1.5f)
    {
        const float leftLeave = leftLeaveTangent * width;
        const float rightArrive = rightArriveTangent * width;
        result = width * (
                     leftValue * (IntegrateHermiteBasis0(t1) - IntegrateHermiteBasis0(t0)) +
                     leftLeave * (IntegrateHermiteBasis1(t1) - IntegrateHermiteBasis1(t0)) +
                     rightValue * (IntegrateHermiteBasis2(t1) - IntegrateHermiteBasis2(t0)) +
                     rightArrive * (IntegrateHermiteBasis3(t1) - IntegrateHermiteBasis3(t0))
                 );
    }
    else
    {
        const float valueDelta = rightValue - leftValue;
        result = width * (leftValue * (t1 - t0) + 0.5f * valueDelta * (t1 * t1 - t0 * t0));
    }

    return result;
}

float EvaluateCompactCurveAverage(
    float lifeProgress,
    uint keyCount,
    float4 times,
    float4 timesBlock1,
    float4 values,
    float4 valuesBlock1,
    float4 arriveTangents,
    float4 arriveTangentsBlock1,
    float4 leaveTangents,
    float4 leaveTangentsBlock1,
    float4 modes,
    float4 modesBlock1)
{
    keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, keyCount));
    const float clampedProgress = saturate(lifeProgress);
    const float firstTime = saturate(ReadVec4Component(times, timesBlock1, 0u));
    const float firstValue = ReadVec4Component(values, valuesBlock1, 0u);

    float result = firstValue;
    if (keyCount > 1u && clampedProgress > firstTime && clampedProgress > 0.0001f)
    {
        float area = firstValue * firstTime;
        float previousTime = firstTime;
        float previousValue = firstValue;
        bool done = false;

        [loop]
        for (uint keyIndex = 1u; keyIndex < kEffectDistributionCurveMaxKeys; ++keyIndex)
        {
            if (keyIndex >= keyCount || done)
                break;

            const float rightTime = max(previousTime, saturate(ReadVec4Component(times, timesBlock1, keyIndex)));
            const float rightValue = ReadVec4Component(values, valuesBlock1, keyIndex);
            const float segmentEnd = min(clampedProgress, rightTime);
            if (segmentEnd > previousTime)
            {
                area += IntegrateCurveSegment(
                    previousTime,
                    previousValue,
                    rightTime,
                    rightValue,
                    ReadVec4Component(leaveTangents, leaveTangentsBlock1, keyIndex - 1u),
                    ReadVec4Component(arriveTangents, arriveTangentsBlock1, keyIndex),
                    ReadVec4Component(modes, modesBlock1, keyIndex - 1u),
                    previousTime,
                    segmentEnd
                );
            }

            if (clampedProgress <= rightTime)
            {
                done = true;
                break;
            }

            previousTime = rightTime;
            previousValue = rightValue;
        }

        if (!done && clampedProgress > previousTime)
            area += previousValue * (clampedProgress - previousTime);

        result = area / max(0.0001f, clampedProgress);
    }

    return result;
}

// RotationRateScaleByLife curve payload를 0..lifeProgress 구간의 평균 배율로 적분 평가한다.
float EvaluateRotationRateScaleByLifeAverage(float lifeProgress)
{
    const uint keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, (uint)g_RotationRateScaleByLifeCurveParams.x));
    return EvaluateCompactCurveAverage(
        lifeProgress,
        keyCount,
        g_RotationRateScaleByLifeCurveTimes,
        g_RotationRateScaleByLifeCurveTimesBlock1,
        g_RotationRateScaleByLifeCurveValues,
        g_RotationRateScaleByLifeCurveValuesBlock1,
        g_RotationRateScaleByLifeCurveArriveTangents,
        g_RotationRateScaleByLifeCurveArriveTangentsBlock1,
        g_RotationRateScaleByLifeCurveLeaveTangents,
        g_RotationRateScaleByLifeCurveLeaveTangentsBlock1,
        g_RotationRateScaleByLifeCurveModes,
        g_RotationRateScaleByLifeCurveModesBlock1
    );
}

float EvaluateVelocityScaleByLifeChannel(
    float lifeProgress,
    float4 times,
    float4 timesBlock1,
    float4 values,
    float4 valuesBlock1,
    float4 arriveTangents,
    float4 arriveTangentsBlock1,
    float4 leaveTangents,
    float4 leaveTangentsBlock1,
    float4 modes,
    float4 modesBlock1,
    float4 params)
{
    float result = 1.0f;
    if (params.y != 0.0f)
    {
        const uint keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, (uint)params.x));
        result = max(
            0.0f,
            EvaluateCompactCurve(
                lifeProgress,
                keyCount,
                times,
                timesBlock1,
                values,
                valuesBlock1,
                arriveTangents,
                arriveTangentsBlock1,
                leaveTangents,
                leaveTangentsBlock1,
                modes,
                modesBlock1,
                1.0f
            )
        );
    }

    return result;
}

float EvaluateVelocityScaleByLifeChannelAverage(
    float lifeProgress,
    float4 times,
    float4 timesBlock1,
    float4 values,
    float4 valuesBlock1,
    float4 arriveTangents,
    float4 arriveTangentsBlock1,
    float4 leaveTangents,
    float4 leaveTangentsBlock1,
    float4 modes,
    float4 modesBlock1,
    float4 params)
{
    float result = 1.0f;
    if (params.y != 0.0f)
    {
        const uint keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, (uint)params.x));
        result = max(
            0.0f,
            EvaluateCompactCurveAverage(
                lifeProgress,
                keyCount,
                times,
                timesBlock1,
                values,
                valuesBlock1,
                arriveTangents,
                arriveTangentsBlock1,
                leaveTangents,
                leaveTangentsBlock1,
                modes,
                modesBlock1
            )
        );
    }

    return result;
}

float EvaluateInitialVelocityScaleByLife(float lifeProgress)
{
    return EvaluateVelocityScaleByLifeChannel(
        lifeProgress,
        g_InitialVelocityScaleByLifeCurveTimes,
        g_InitialVelocityScaleByLifeCurveTimesBlock1,
        g_InitialVelocityScaleByLifeCurveValues,
        g_InitialVelocityScaleByLifeCurveValuesBlock1,
        g_InitialVelocityScaleByLifeCurveArriveTangents,
        g_InitialVelocityScaleByLifeCurveArriveTangentsBlock1,
        g_InitialVelocityScaleByLifeCurveLeaveTangents,
        g_InitialVelocityScaleByLifeCurveLeaveTangentsBlock1,
        g_InitialVelocityScaleByLifeCurveModes,
        g_InitialVelocityScaleByLifeCurveModesBlock1,
        g_InitialVelocityScaleByLifeCurveParams
    );
}

float EvaluateInitialVelocityScaleByLifeAverage(float lifeProgress)
{
    return EvaluateVelocityScaleByLifeChannelAverage(
        lifeProgress,
        g_InitialVelocityScaleByLifeCurveTimes,
        g_InitialVelocityScaleByLifeCurveTimesBlock1,
        g_InitialVelocityScaleByLifeCurveValues,
        g_InitialVelocityScaleByLifeCurveValuesBlock1,
        g_InitialVelocityScaleByLifeCurveArriveTangents,
        g_InitialVelocityScaleByLifeCurveArriveTangentsBlock1,
        g_InitialVelocityScaleByLifeCurveLeaveTangents,
        g_InitialVelocityScaleByLifeCurveLeaveTangentsBlock1,
        g_InitialVelocityScaleByLifeCurveModes,
        g_InitialVelocityScaleByLifeCurveModesBlock1,
        g_InitialVelocityScaleByLifeCurveParams
    );
}

float EvaluateInitialRadialVelocityScaleByLife(float lifeProgress)
{
    return EvaluateVelocityScaleByLifeChannel(
        lifeProgress,
        g_InitialRadialVelocityScaleByLifeCurveTimes,
        g_InitialRadialVelocityScaleByLifeCurveTimesBlock1,
        g_InitialRadialVelocityScaleByLifeCurveValues,
        g_InitialRadialVelocityScaleByLifeCurveValuesBlock1,
        g_InitialRadialVelocityScaleByLifeCurveArriveTangents,
        g_InitialRadialVelocityScaleByLifeCurveArriveTangentsBlock1,
        g_InitialRadialVelocityScaleByLifeCurveLeaveTangents,
        g_InitialRadialVelocityScaleByLifeCurveLeaveTangentsBlock1,
        g_InitialRadialVelocityScaleByLifeCurveModes,
        g_InitialRadialVelocityScaleByLifeCurveModesBlock1,
        g_InitialRadialVelocityScaleByLifeCurveParams
    );
}

float EvaluateInitialRadialVelocityScaleByLifeAverage(float lifeProgress)
{
    return EvaluateVelocityScaleByLifeChannelAverage(
        lifeProgress,
        g_InitialRadialVelocityScaleByLifeCurveTimes,
        g_InitialRadialVelocityScaleByLifeCurveTimesBlock1,
        g_InitialRadialVelocityScaleByLifeCurveValues,
        g_InitialRadialVelocityScaleByLifeCurveValuesBlock1,
        g_InitialRadialVelocityScaleByLifeCurveArriveTangents,
        g_InitialRadialVelocityScaleByLifeCurveArriveTangentsBlock1,
        g_InitialRadialVelocityScaleByLifeCurveLeaveTangents,
        g_InitialRadialVelocityScaleByLifeCurveLeaveTangentsBlock1,
        g_InitialRadialVelocityScaleByLifeCurveModes,
        g_InitialRadialVelocityScaleByLifeCurveModesBlock1,
        g_InitialRadialVelocityScaleByLifeCurveParams
    );
}

float EvaluateVelocityConeScaleByLife(float lifeProgress)
{
    return EvaluateVelocityScaleByLifeChannel(
        lifeProgress,
        g_VelocityConeScaleByLifeCurveTimes,
        g_VelocityConeScaleByLifeCurveTimesBlock1,
        g_VelocityConeScaleByLifeCurveValues,
        g_VelocityConeScaleByLifeCurveValuesBlock1,
        g_VelocityConeScaleByLifeCurveArriveTangents,
        g_VelocityConeScaleByLifeCurveArriveTangentsBlock1,
        g_VelocityConeScaleByLifeCurveLeaveTangents,
        g_VelocityConeScaleByLifeCurveLeaveTangentsBlock1,
        g_VelocityConeScaleByLifeCurveModes,
        g_VelocityConeScaleByLifeCurveModesBlock1,
        g_VelocityConeScaleByLifeCurveParams
    );
}

float EvaluateVelocityConeScaleByLifeAverage(float lifeProgress)
{
    return EvaluateVelocityScaleByLifeChannelAverage(
        lifeProgress,
        g_VelocityConeScaleByLifeCurveTimes,
        g_VelocityConeScaleByLifeCurveTimesBlock1,
        g_VelocityConeScaleByLifeCurveValues,
        g_VelocityConeScaleByLifeCurveValuesBlock1,
        g_VelocityConeScaleByLifeCurveArriveTangents,
        g_VelocityConeScaleByLifeCurveArriveTangentsBlock1,
        g_VelocityConeScaleByLifeCurveLeaveTangents,
        g_VelocityConeScaleByLifeCurveLeaveTangentsBlock1,
        g_VelocityConeScaleByLifeCurveModes,
        g_VelocityConeScaleByLifeCurveModesBlock1,
        g_VelocityConeScaleByLifeCurveParams
    );
}

float EvaluateSourceMotionVelocityScaleByLife(float lifeProgress)
{
    return EvaluateVelocityScaleByLifeChannel(
        lifeProgress,
        g_SourceMotionVelocityScaleByLifeCurveTimes,
        g_SourceMotionVelocityScaleByLifeCurveTimesBlock1,
        g_SourceMotionVelocityScaleByLifeCurveValues,
        g_SourceMotionVelocityScaleByLifeCurveValuesBlock1,
        g_SourceMotionVelocityScaleByLifeCurveArriveTangents,
        g_SourceMotionVelocityScaleByLifeCurveArriveTangentsBlock1,
        g_SourceMotionVelocityScaleByLifeCurveLeaveTangents,
        g_SourceMotionVelocityScaleByLifeCurveLeaveTangentsBlock1,
        g_SourceMotionVelocityScaleByLifeCurveModes,
        g_SourceMotionVelocityScaleByLifeCurveModesBlock1,
        g_SourceMotionVelocityScaleByLifeCurveParams
    );
}

float EvaluateSourceMotionVelocityScaleByLifeAverage(float lifeProgress)
{
    return EvaluateVelocityScaleByLifeChannelAverage(
        lifeProgress,
        g_SourceMotionVelocityScaleByLifeCurveTimes,
        g_SourceMotionVelocityScaleByLifeCurveTimesBlock1,
        g_SourceMotionVelocityScaleByLifeCurveValues,
        g_SourceMotionVelocityScaleByLifeCurveValuesBlock1,
        g_SourceMotionVelocityScaleByLifeCurveArriveTangents,
        g_SourceMotionVelocityScaleByLifeCurveArriveTangentsBlock1,
        g_SourceMotionVelocityScaleByLifeCurveLeaveTangents,
        g_SourceMotionVelocityScaleByLifeCurveLeaveTangentsBlock1,
        g_SourceMotionVelocityScaleByLifeCurveModes,
        g_SourceMotionVelocityScaleByLifeCurveModesBlock1,
        g_SourceMotionVelocityScaleByLifeCurveParams
    );
}

float EvaluateAccelerationIntegratedVelocityScaleByLife(float lifeProgress)
{
    return EvaluateVelocityScaleByLifeChannel(
        lifeProgress,
        g_AccelerationIntegratedVelocityScaleByLifeCurveTimes,
        g_AccelerationIntegratedVelocityScaleByLifeCurveTimesBlock1,
        g_AccelerationIntegratedVelocityScaleByLifeCurveValues,
        g_AccelerationIntegratedVelocityScaleByLifeCurveValuesBlock1,
        g_AccelerationIntegratedVelocityScaleByLifeCurveArriveTangents,
        g_AccelerationIntegratedVelocityScaleByLifeCurveArriveTangentsBlock1,
        g_AccelerationIntegratedVelocityScaleByLifeCurveLeaveTangents,
        g_AccelerationIntegratedVelocityScaleByLifeCurveLeaveTangentsBlock1,
        g_AccelerationIntegratedVelocityScaleByLifeCurveModes,
        g_AccelerationIntegratedVelocityScaleByLifeCurveModesBlock1,
        g_AccelerationIntegratedVelocityScaleByLifeCurveParams
    );
}

float EvaluateAccelerationIntegratedVelocityScaleByLifeAverage(float lifeProgress)
{
    return EvaluateVelocityScaleByLifeChannelAverage(
        lifeProgress,
        g_AccelerationIntegratedVelocityScaleByLifeCurveTimes,
        g_AccelerationIntegratedVelocityScaleByLifeCurveTimesBlock1,
        g_AccelerationIntegratedVelocityScaleByLifeCurveValues,
        g_AccelerationIntegratedVelocityScaleByLifeCurveValuesBlock1,
        g_AccelerationIntegratedVelocityScaleByLifeCurveArriveTangents,
        g_AccelerationIntegratedVelocityScaleByLifeCurveArriveTangentsBlock1,
        g_AccelerationIntegratedVelocityScaleByLifeCurveLeaveTangents,
        g_AccelerationIntegratedVelocityScaleByLifeCurveLeaveTangentsBlock1,
        g_AccelerationIntegratedVelocityScaleByLifeCurveModes,
        g_AccelerationIntegratedVelocityScaleByLifeCurveModesBlock1,
        g_AccelerationIntegratedVelocityScaleByLifeCurveParams
    );
}

// 정수 seed를 0~1 사이의 pseudo-random float 값으로 바꾼다.
float Hash01(uint seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return (float)(seed & 0x00FFFFFFu) / 16777215.0f;
}

float Resolve_EvenByParticleIndexAngleRatio(uint particleIndex, uint maxActiveCount, float angleMinDegrees, float angleMaxDegrees)
{
    const uint clampedCount = max(1u, maxActiveCount);
    const float angleSpanDegrees = abs(angleMaxDegrees - angleMinDegrees);
    const bool isFullCircle = abs(angleSpanDegrees - 360.0f) <= kPlaneRadialAngleEpsilon;
    const float denominator = isFullCircle ? (float)clampedCount : (float)max(1u, clampedCount - 1u);
    const float ratio = (float)particleIndex / denominator;

    return clampedCount <= 1u ? 0.0f : ratio;
}

float3 ResolveEvenByParticleIndexSphereDirection(uint placementIndex, uint placementCount)
{
    const uint clampedCount = max(1u, placementCount);
    const uint clampedIndex = placementIndex % clampedCount;
    const float ratio = ((float)clampedIndex + 0.5f) / (float)clampedCount;
    const float z = 1.0f - 2.0f * ratio;
    const float radiusOnPlane = sqrt(max(0.0f, 1.0f - z * z));
    const float goldenAngle = 3.14159265359f * (3.0f - sqrt(5.0f));
    const float angle = (float)clampedIndex * goldenAngle;

    const float3 direction = float3(cos(angle) * radiusOnPlane, sin(angle) * radiusOnPlane, z);
    return placementCount <= 1u ? float3(0.0f, 0.0f, 1.0f) : direction;
}

// seed에서 만든 0~1 값으로 min/max 사이의 3축 값을 샘플링한다.
float3 SampleRange3(float3 minValue, float3 maxValue, uint seed)
{
    return lerp(
        minValue,
        maxValue,
        float3(Hash01(seed), Hash01(seed + 17u), Hash01(seed + 31u))
    );
}

float3 SampleCoreColorRgbParticleLifeUniform(uint respawnSeed)
{
    float3 result = float3(1.0f, 0.85f, 0.45f);

    if (g_CoreColorRgbParticleLifeUniformParams.x >= 0.5f)
    {
        const uint seed = g_CoreColorRgbParticleLifeUniformSeed.x + respawnSeed * 3571u;
        result = float3(
            lerp(
                min(g_CoreColorRgbParticleLifeUniformMin.x, g_CoreColorRgbParticleLifeUniformMax.x),
                max(g_CoreColorRgbParticleLifeUniformMin.x, g_CoreColorRgbParticleLifeUniformMax.x),
                Hash01(seed + 1u)
            ),
            lerp(
                min(g_CoreColorRgbParticleLifeUniformMin.y, g_CoreColorRgbParticleLifeUniformMax.y),
                max(g_CoreColorRgbParticleLifeUniformMin.y, g_CoreColorRgbParticleLifeUniformMax.y),
                Hash01(seed + 2u)
            ),
            lerp(
                min(g_CoreColorRgbParticleLifeUniformMin.z, g_CoreColorRgbParticleLifeUniformMax.z),
                max(g_CoreColorRgbParticleLifeUniformMin.z, g_CoreColorRgbParticleLifeUniformMax.z),
                Hash01(seed + 3u)
            )
        );
    }

    return result;
}

float2 SampleRange2(float2 minValue, float2 maxValue, uint seed)
{
    return lerp(
        minValue,
        maxValue,
        float2(Hash01(seed + 11u), Hash01(seed + 23u))
    );
}

// emitter local 축 기준 값을 월드 방향 오프셋으로 변환한다.
float3 ResolveEmitterBasisVector(float3 localValue)
{
    const float rightLength = length(g_EmitterBasisRight.xyz);
    const float upLength = length(g_EmitterBasisUp.xyz);
    const float lookLength = length(g_EmitterBasisLook.xyz);
    const float3 right = rightLength < 0.0001f ? float3(1.0f, 0.0f, 0.0f) : g_EmitterBasisRight.xyz / rightLength;
    const float3 up = upLength < 0.0001f ? float3(0.0f, 1.0f, 0.0f) : g_EmitterBasisUp.xyz / upLength;
    const float3 look = lookLength < 0.0001f ? float3(0.0f, 0.0f, 1.0f) : g_EmitterBasisLook.xyz / lookLength;
    return right * localValue.x + up * localValue.y + look * localValue.z;
}

float3 ResolveEmitterLocalVector(float3 worldValue)
{
    const float rightLength = length(g_EmitterBasisRight.xyz);
    const float upLength = length(g_EmitterBasisUp.xyz);
    const float lookLength = length(g_EmitterBasisLook.xyz);
    const float3 right = rightLength < 0.0001f ? float3(1.0f, 0.0f, 0.0f) : g_EmitterBasisRight.xyz / rightLength;
    const float3 up = upLength < 0.0001f ? float3(0.0f, 1.0f, 0.0f) : g_EmitterBasisUp.xyz / upLength;
    const float3 look = lookLength < 0.0001f ? float3(0.0f, 0.0f, 1.0f) : g_EmitterBasisLook.xyz / lookLength;
    return float3(dot(worldValue, right), dot(worldValue, up), dot(worldValue, look));
}

float EvaluateOrbitAngleDegrees(float lifeProgress)
{
    return EvaluateCompactCurve(
        lifeProgress,
        (uint)g_OrbitAngleCurveParams.x,
        g_OrbitAngleCurveTimes,
        g_OrbitAngleCurveTimesBlock1,
        g_OrbitAngleCurveValues,
        g_OrbitAngleCurveValuesBlock1,
        g_OrbitAngleCurveArriveTangents,
        g_OrbitAngleCurveArriveTangentsBlock1,
        g_OrbitAngleCurveLeaveTangents,
        g_OrbitAngleCurveLeaveTangentsBlock1,
        g_OrbitAngleCurveModes,
        g_OrbitAngleCurveModesBlock1,
        lerp(g_OrbitAngleRange.x, g_OrbitAngleRange.y, lifeProgress)
    );
}

float EvaluateOrbitRadiusScale(float lifeProgress)
{
    return EvaluateCompactCurve(
        lifeProgress,
        (uint)g_OrbitRadiusCurveParams.x,
        g_OrbitRadiusCurveTimes,
        g_OrbitRadiusCurveTimesBlock1,
        g_OrbitRadiusCurveValues,
        g_OrbitRadiusCurveValuesBlock1,
        g_OrbitRadiusCurveArriveTangents,
        g_OrbitRadiusCurveArriveTangentsBlock1,
        g_OrbitRadiusCurveLeaveTangents,
        g_OrbitRadiusCurveLeaveTangentsBlock1,
        g_OrbitRadiusCurveModes,
        g_OrbitRadiusCurveModesBlock1,
        lerp(g_OrbitRadiusRange.x, g_OrbitRadiusRange.y, lifeProgress)
    );
}

float3 RotateOrbitLocalVector(float3 localValue, float angleDegrees, uint plane, float planeScale)
{
    const float c = cos(radians(angleDegrees));
    const float s = sin(radians(angleDegrees));
    float2 planeOffset = localValue.xy;
    if (plane == 1u)
        planeOffset = localValue.xz;
    else if (plane == 2u)
        planeOffset = localValue.yz;

    planeOffset *= planeScale;
    const float2 rotatedOffset = float2(
        planeOffset.x * c - planeOffset.y * s,
        planeOffset.x * s + planeOffset.y * c
    );

    if (plane == 1u)
    {
        localValue.x = rotatedOffset.x;
        localValue.z = rotatedOffset.y;
    }
    else if (plane == 2u)
    {
        localValue.y = rotatedOffset.x;
        localValue.z = rotatedOffset.y;
    }
    else
    {
        localValue.x = rotatedOffset.x;
        localValue.y = rotatedOffset.y;
    }

    return localValue;
}

float3 ApplyOrbitOverLife(float3 spawnOffset, float lifeProgress)
{
    float3 result = spawnOffset;
    if (g_OrbitParams.x != 0.0f)
    {
        float3 localOffset = ResolveEmitterLocalVector(spawnOffset);
        localOffset = RotateOrbitLocalVector(
            localOffset,
            EvaluateOrbitAngleDegrees(lifeProgress),
            (uint)g_OrbitParams.y,
            EvaluateOrbitRadiusScale(lifeProgress)
        );
        result = ResolveEmitterBasisVector(localOffset);
    }

    return result;
}

float3 ApplyOrbitOverLifeDirection(float3 localDirection, float lifeProgress)
{
    float3 result = localDirection;
    if (g_OrbitParams.x != 0.0f && g_CylinderOrientationParams.w != 0.0f)
    {
        result = RotateOrbitLocalVector(
            localDirection,
            EvaluateOrbitAngleDegrees(lifeProgress),
            (uint)g_OrbitParams.y,
            1.0f
        );
    }

    return result;
}

float3 NormalizeOrUp(float3 value)
{
    const float valueLength = length(value);
    return valueLength < 0.0001f ? float3(0.0f, 1.0f, 0.0f) : value / valueLength;
}

float3 SampleConeDirection(float3 axis, float angleDegrees, uint seed)
{
    axis = NormalizeOrUp(axis);
    const float clampedAngleDegrees = clamp(angleDegrees, 0.0f, 180.0f);
    float3 sampledDirection = axis;

    if (clampedAngleDegrees > 0.0001f)
    {
        const float cosMax = cos(radians(clampedAngleDegrees));
        const float cosTheta = lerp(1.0f, cosMax, Hash01(seed + 1703u));
        const float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
        const float phi = Hash01(seed + 1721u) * 6.28318530718f;
        const float3 helper = abs(axis.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
        const float3 tangent = normalize(cross(helper, axis));
        const float3 bitangent = cross(axis, tangent);
        sampledDirection = axis * cosTheta + (tangent * cos(phi) + bitangent * sin(phi)) * sinTheta;
    }

    return sampledDirection;
}

float3 ResolveSourceMotionVelocity(float3 sourceVelocity, uint seed)
{
    const bool sourceVelocityValid = g_SourceMotionVelocity.w != 0.0f;
    const float sourceSpeed = sourceVelocityValid ? length(sourceVelocity) : 0.0f;
    const float3 fallbackForward = ResolveEmitterBasisVector(float3(0.0f, 0.0f, 1.0f));
    const float fallbackLength = length(fallbackForward);
    const float3 fallbackDirection = fallbackLength < 0.0001f ? float3(0.0f, 0.0f, 1.0f) : fallbackForward / fallbackLength;
    const uint directionMode = (uint)g_SourceMotionVelocityParams.y;
    const bool sourceVelocityRequired = directionMode <= 2u;

    float3 sourceDirection = fallbackDirection;
    if (sourceSpeed >= 0.0001f)
        sourceDirection = sourceVelocity / sourceSpeed;
    const float3 tangent = sourceDirection;
    float3 side = cross(tangent, float3(0.0f, 1.0f, 0.0f));
    if (length(side) < 0.0001f)
        side = cross(tangent, float3(0.0f, 0.0f, 1.0f));
    side = length(side) < 0.0001f ? float3(1.0f, 0.0f, 0.0f) : normalize(side);

    float3 resolvedDirection = tangent;
    if (directionMode == 0u || directionMode == 1u)
        resolvedDirection = sourceDirection;
    else if (directionMode == 2u)
        resolvedDirection = -sourceDirection;
    else if (directionMode == 4u)
        resolvedDirection = -tangent;
    else if (directionMode == 5u)
        resolvedDirection = side;
    else if (directionMode == 6u)
        resolvedDirection = Hash01(seed + 3119u) < 0.5f ? side : -side;

    const float resolvedDirectionLength = length(resolvedDirection);
    resolvedDirection = resolvedDirectionLength < 0.0001f ? fallbackDirection : resolvedDirection / resolvedDirectionLength;
    if (directionMode != 0u && g_SourceMotionVelocityParams.z > 0.0f)
        resolvedDirection = SampleConeDirection(resolvedDirection, g_SourceMotionVelocityParams.z, seed + 3251u);

    const float sampledSpeed = lerp(g_SourceMotionVelocitySpeed.x, g_SourceMotionVelocitySpeed.y, Hash01(seed + 2357u));
    float3 result = resolvedDirection * (sampledSpeed + sourceSpeed * g_SourceMotionVelocityParams.w);
    if (directionMode == 0u)
        result = sourceVelocity * g_SourceMotionVelocityParams.w + sourceDirection * sampledSpeed;

    if (sourceVelocityRequired && !sourceVelocityValid)
        result = float3(0.0f, 0.0f, 0.0f);

    return result;
}

struct PlaneRadialFrame
{
    float3 localOffset;
    float3 radialDirection;
    float3 tangentCW;
    float3 tangentCCW;
    float3 planeNormal;
    float sampleAngle;
    float enabled;
};

struct CylinderFrame
{
    float3 localOffset;
    float3 radialDirection;
    float3 tangentCW;
    float3 tangentCCW;
    float3 cylinderAxis;
    float sampleAngle;
    float enabled;
};

void ResolvePlaneRadialBasis(out float3 axisU, out float3 axisV, out float3 planeNormal)
{
    const uint plane = (uint)g_PlaneRadialLocationParams.y;
    axisU = float3(1.0f, 0.0f, 0.0f);
    axisV = float3(0.0f, 1.0f, 0.0f);
    planeNormal = float3(0.0f, 0.0f, 1.0f);

    if (plane == 1u)
    {
        axisV = float3(0.0f, 0.0f, 1.0f);
        planeNormal = float3(0.0f, 1.0f, 0.0f);
    }
    else if (plane == 2u)
    {
        axisU = float3(0.0f, 1.0f, 0.0f);
        axisV = float3(0.0f, 0.0f, 1.0f);
        planeNormal = float3(1.0f, 0.0f, 0.0f);
    }
    else if (plane == 3u)
    {
        axisU = g_PlaneRadialCameraFacingRight.xyz;
        axisV = g_PlaneRadialCameraFacingUp.xyz;
        planeNormal = g_PlaneRadialCameraFacingNormal.xyz;
    }
}

void SamplePlaneRadialFrame(uint seed, uint particleIndex, uint placementIndex, uint placementCount, out PlaneRadialFrame frame)
{
    float3 axisU = float3(1.0f, 0.0f, 0.0f);
    float3 axisV = float3(0.0f, 1.0f, 0.0f);
    float3 planeNormal = float3(0.0f, 0.0f, 1.0f);
    ResolvePlaneRadialBasis(axisU, axisV, planeNormal);

    frame.localOffset = float3(0.0f, 0.0f, 0.0f);
    frame.planeNormal = planeNormal;
    frame.radialDirection = axisU;
    frame.tangentCW = -axisV;
    frame.tangentCCW = axisV;
    frame.sampleAngle = 0.0f;
    frame.enabled = 0.0f;

    if (g_PlaneRadialLocationParams.x == 0.0f)
        return;

    const uint shape = (uint)g_PlaneRadialLocationParams.z;
    const uint placementMode = (uint)g_PlaneRadialLocationParams.w;
    float u = 0.0f;
    float v = 0.0f;

    if (shape == 0u)
    {
        u = lerp(g_PlaneRadialLocationRectRange.x, g_PlaneRadialLocationRectRange.y, Hash01(seed + g_PlaneRadialLocationSeedSalts.x + 1601u));
        v = lerp(g_PlaneRadialLocationRectRange.z, g_PlaneRadialLocationRectRange.w, Hash01(seed + g_PlaneRadialLocationSeedSalts.y + 1613u));
        frame.sampleAngle = atan2(v, u);
    }
    else
    {
        const float radiusMin = min(g_PlaneRadialLocationPolarRange.x, g_PlaneRadialLocationPolarRange.y);
        const float radiusMax = max(g_PlaneRadialLocationPolarRange.x, g_PlaneRadialLocationPolarRange.y);
        const float angleMinDegrees = g_PlaneRadialLocationPolarRange.z;
        const float angleMaxDegrees = g_PlaneRadialLocationPolarRange.w;
        const float angleMin = radians(angleMinDegrees);
        const float angleMax = radians(angleMaxDegrees);
        const float radius = lerp(radiusMin, radiusMax, Hash01(seed + g_PlaneRadialLocationSeedSalts.x + 1601u));
        const float angleRatio = placementMode == 1u
                                 ? Resolve_EvenByParticleIndexAngleRatio(
                                     placementIndex,
                                     placementCount,
                                     angleMinDegrees,
                                     angleMaxDegrees
                                 )
                                 : Hash01(seed + g_PlaneRadialLocationSeedSalts.y + 1613u);
        const float angle = lerp(angleMin, angleMax, angleRatio);
        frame.sampleAngle = angle;
        u = cos(angle) * radius;
        v = sin(angle) * radius;
    }

    const float thickness = max(0.0f, g_PlaneRadialLocationOffsetThickness.w);
    const float normalOffset = thickness > 0.0001f
                               ? (Hash01(seed + 1621u) - 0.5f) * thickness
                               : 0.0f;
    const float3 radialVector = axisU * u + axisV * v;
    const float radialLength = length(radialVector);
    frame.radialDirection = radialLength < 0.0001f
                            ? normalize(axisU * cos(frame.sampleAngle) + axisV * sin(frame.sampleAngle))
                            : radialVector / radialLength;
    frame.tangentCW = normalize(axisU * sin(frame.sampleAngle) - axisV * cos(frame.sampleAngle));
    frame.tangentCCW = -frame.tangentCW;
    frame.localOffset = g_PlaneRadialLocationOffsetThickness.xyz + radialVector + planeNormal * normalOffset;
    frame.enabled = 1.0f;
}

void ResolveCylinderLocationBasis(out float3 axisW, out float3 radialU, out float3 radialV)
{
    axisW = float3(0.0f, 0.0f, 1.0f);
    radialU = float3(1.0f, 0.0f, 0.0f);
    radialV = float3(0.0f, 1.0f, 0.0f);

    const uint axis = (uint)g_CylinderLocationParams.y;
    if (axis == 0u)
    {
        axisW = float3(1.0f, 0.0f, 0.0f);
        radialU = float3(0.0f, 1.0f, 0.0f);
        radialV = float3(0.0f, 0.0f, 1.0f);
    }
    else if (axis == 1u)
    {
        axisW = float3(0.0f, 1.0f, 0.0f);
        radialU = float3(1.0f, 0.0f, 0.0f);
        radialV = float3(0.0f, 0.0f, 1.0f);
    }
}

void SampleCylinderFrame(uint seed, uint placementIndex, uint placementCount, out CylinderFrame frame)
{
    frame.localOffset = float3(0.0f, 0.0f, 0.0f);
    frame.radialDirection = float3(1.0f, 0.0f, 0.0f);
    frame.tangentCW = float3(0.0f, -1.0f, 0.0f);
    frame.tangentCCW = float3(0.0f, 1.0f, 0.0f);
    frame.cylinderAxis = float3(0.0f, 0.0f, 1.0f);
    frame.sampleAngle = 0.0f;
    frame.enabled = 0.0f;

    float3 axisW = float3(0.0f, 0.0f, 1.0f);
    float3 radialU = float3(1.0f, 0.0f, 0.0f);
    float3 radialV = float3(0.0f, 1.0f, 0.0f);
    ResolveCylinderLocationBasis(axisW, radialU, radialV);

    const float radiusMin = max(0.0f, min(g_CylinderLocationRadiusHeight.x, g_CylinderLocationRadiusHeight.y));
    const float radiusMax = max(radiusMin, max(g_CylinderLocationRadiusHeight.x, g_CylinderLocationRadiusHeight.y));
    const float heightMin = min(g_CylinderLocationRadiusHeight.z, g_CylinderLocationRadiusHeight.w);
    const float heightMax = max(g_CylinderLocationRadiusHeight.z, g_CylinderLocationRadiusHeight.w);
    const float angleMinDegrees = g_CylinderLocationAngle.x;
    const float angleMaxDegrees = g_CylinderLocationAngle.y;
    const float radiusRatio = Hash01(seed + g_CylinderLocationSeedSalts.x + 1801u);
    const float radius = (uint)g_CylinderLocationParams.z == 1u
                         ? sqrt(lerp(radiusMin * radiusMin, radiusMax * radiusMax, radiusRatio))
                         : lerp(radiusMin, radiusMax, radiusRatio);
    const float height = lerp(heightMin, heightMax, Hash01(seed + g_CylinderLocationSeedSalts.y + 1811u));
    const float angleRatio = (uint)g_CylinderLocationParams.w == 1u
                             ? Resolve_EvenByParticleIndexAngleRatio(
                                 placementIndex,
                                 placementCount,
                                 angleMinDegrees,
                                 angleMaxDegrees
                             )
                             : Hash01(seed + g_CylinderLocationSeedSalts.z + 1823u);
    const float angle = lerp(radians(angleMinDegrees), radians(angleMaxDegrees), angleRatio);
    const float3 radialVector = (radialU * cos(angle) + radialV * sin(angle)) * radius;
    if (g_CylinderLocationParams.x == 0.0f)
        return;

    frame.localOffset = g_CylinderLocationOffset.xyz + axisW * height + radialVector;
    frame.radialDirection = normalize(radialU * cos(angle) + radialV * sin(angle));
    frame.tangentCW = normalize(radialU * sin(angle) - radialV * cos(angle));
    frame.tangentCCW = -frame.tangentCW;
    frame.cylinderAxis = axisW;
    frame.sampleAngle = angle;
    frame.enabled = 1.0f;
}

float ResolvePlaneRadialSpriteRotation(float frameEnabled, float sampleAngle)
{
    float rotation = 0.0f;
    const uint orientationMode = (uint)g_PlaneRadialOrientationParams.z;
    if (frameEnabled != 0.0f && g_PlaneRadialOrientationParams.x != 0.0f && orientationMode != 0u && orientationMode != 5u)
    {
        if (orientationMode == 2u)
            rotation = sampleAngle + 3.14159265359f;
        else if (orientationMode == 3u)
            rotation = sampleAngle - 1.57079632679f;
        else if (orientationMode == 4u)
            rotation = sampleAngle + 1.57079632679f;
        else
            rotation = sampleAngle;
    }

    return rotation;
}

bool IsCylinderSpriteOrientationEnabled(CylinderFrame frame)
{
    const uint targetKind = (uint)g_CylinderOrientationParams.y;
    const uint orientationMode = (uint)g_CylinderOrientationParams.z;
    return frame.enabled != 0.0f &&
           g_CylinderOrientationParams.x != 0.0f &&
           targetKind != 2u &&
           orientationMode != 0u;
}

float ResolveCylinderSpriteRollOffset(CylinderFrame frame)
{
    float rollOffset = 0.0f;
    if (IsCylinderSpriteOrientationEnabled(frame))
        rollOffset = radians(g_CylinderOrientationAngles.y);

    return rollOffset;
}

float3 ResolveCylinderSpriteLookDirection(CylinderFrame frame, float lifeProgress, float3 fallbackDirection)
{
    float3 result = fallbackDirection;
    if (IsCylinderSpriteOrientationEnabled(frame))
    {
        float3 localDirection = frame.radialDirection;
        const uint orientationMode = (uint)g_CylinderOrientationParams.z;
        if (orientationMode == 2u)
            localDirection = -frame.radialDirection;
        else if (orientationMode == 3u)
            localDirection = frame.tangentCW;
        else if (orientationMode == 4u)
            localDirection = frame.tangentCCW;
        else if (orientationMode == 5u)
            localDirection = frame.cylinderAxis;
        else if (orientationMode == 6u)
            localDirection = -frame.cylinderAxis;

        localDirection = ApplyOrbitOverLifeDirection(localDirection, lifeProgress);
        const float3 worldDirection = ResolveEmitterBasisVector(localDirection);
        const float directionLength = length(worldDirection);
        result = directionLength < 0.0001f ? fallbackDirection : worldDirection / directionLength;
    }

    return result;
}

// sphere location 설정에 따라 구 내부/표면 spawn 오프셋을 샘플링한다.
float3 SampleSphereLocationOffset(uint seed, uint placementIndex, uint placementCount)
{
    const float radius = max(0.0f, g_SphereLocationOffsetRadius.w);
    const float twoPi = 6.28318530718f;
    const uint sphereMode = (uint)g_SphereLocationParams.y;
    const uint placementMode = (uint)g_SphereLocationParams.z;
    float3 direction;
    if (sphereMode == 1u && placementMode == 1u)
        direction = ResolveEvenByParticleIndexSphereDirection(placementIndex, placementCount);
    else
    {
        const float z = Hash01(seed + 1301u) * 2.0f - 1.0f;
        const float angle = Hash01(seed + 1409u) * twoPi;
        const float xy = sqrt(max(0.0f, 1.0f - z * z));
        direction = float3(cos(angle) * xy, sin(angle) * xy, z);
    }
    const float distanceScale = sphereMode == 1u
                                ? 1.0f
                                : pow(Hash01(seed + 1511u), 1.0f / 3.0f);
    const float enabled = g_SphereLocationParams.x != 0.0f && radius > 0.0001f ? 1.0f : 0.0f;

    return ResolveEmitterBasisVector(g_SphereLocationOffsetRadius.xyz + direction * radius * distanceScale) * enabled;
}

// 수명 min/max 범위에서 이번 particle의 최대 수명을 샘플링한다.
float SampleLifeMax(uint seed)
{
    const float lifeMin = max(0.0001f, min(g_LifeTimeParams.x, g_LifeTimeParams.y));
    const float lifeMax = max(lifeMin, max(g_LifeTimeParams.x, g_LifeTimeParams.y));
    float result = lerp(lifeMin, lifeMax, Hash01(seed + 43u));

    if (g_LifetimeCurveParams.y != 0.0f)
    {
        result = EvaluateCompactCurve(
            saturate(g_LifetimeCurveParams.z),
            (uint)g_LifetimeCurveParams.x,
            g_LifetimeCurveTimes,
            g_LifetimeCurveTimesBlock1,
            g_LifetimeCurveValues,
            g_LifetimeCurveValuesBlock1,
            g_LifetimeCurveArriveTangents,
            g_LifetimeCurveArriveTangentsBlock1,
            g_LifetimeCurveLeaveTangents,
            g_LifetimeCurveLeaveTangentsBlock1,
            g_LifetimeCurveModes,
            g_LifetimeCurveModesBlock1,
            lifeMax
        );
    }

    return max(0.0001f, result);
}

struct ComputeIndirectArgs
{
    uint value0;
    uint instanceCount;
    uint value2;
    int value3;
    uint startInstanceLocation;
};

RWStructuredBuffer<ComputeIndirectArgs> ArgsData : register(u1);

[numthreads(64, 1, 1)]
void CS_Main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    const uint maxInstanceCount = (uint)g_InstanceCount;
    const uint maxActiveCount = min(maxInstanceCount, max(1u, g_MaxActiveCount));
    const uint maxDrawCount = min(maxInstanceCount, max(1u, g_MaxDrawCount));
    const uint stableDrawCount = min(maxActiveCount, maxDrawCount);

    if (index >= (uint)g_InstanceCount)
        return;

    if (index == 0u)
        ArgsData[0].instanceCount = stableDrawCount;

    ParticleLifecycleState lifecycle = LifecycleState[index];

    if (g_KillActiveParticles != 0u)
    {
        lifecycle.lifeAge = 0.0f;
        lifecycle.active = 0u;
        LifecycleState[index] = lifecycle;
        return;
    }

    if (index >= stableDrawCount)
    {
        lifecycle.active = 0u;
        LifecycleState[index] = lifecycle;
        return;
    }

    lifecycle.lifeMax = max(lifecycle.lifeMax, 0.0001f);

    if (lifecycle.active != 0u)
    {
        lifecycle.lifeAge += max(0.0f, g_DeltaTime);
        if (lifecycle.lifeAge >= lifecycle.lifeMax)
            lifecycle.active = 0u;
    }

    if (lifecycle.active == 0u && g_SpawnRequest > 0u)
    {
        uint spawnCandidate = 0u;
        InterlockedAdd(ArgsData[0].value2, 1u, spawnCandidate);

        if (spawnCandidate < g_SpawnRequest)
        {
            const uint spawnSerial = g_SpawnSerialParams.x + spawnCandidate;
            lifecycle.respawnSeed = spawnSerial * 9781u + 1u;
            lifecycle.lifeMax = SampleLifeMax(lifecycle.respawnSeed + g_SpawnSeedSalts.x);
            lifecycle.lifeAge = 0.0f;
            lifecycle.active = 1u;
            lifecycle.placementIndex = spawnCandidate;
            lifecycle.placementCount = max(1u, min(maxActiveCount, g_SpawnRequest));
        }
    }

    if (lifecycle.active == 0u)
    {
        lifecycle.lifeAge = 0.0f;
        if (index < stableDrawCount)
            OutputData[index] = (ParticleInstanceVertex)0;
        LifecycleState[index] = lifecycle;
        return;
    }

    const uint seed = lifecycle.respawnSeed;
    const float ratio = ((float)index + 0.5f) / max(1.0f, g_InstanceCount);
    const float scaleRatio = frac(ratio * 7.0f);
    const float colorRatio = frac(ratio * 11.0f);
    const float sizeRatio = g_AppearanceSeedSalts0.x != 0u ? Hash01(seed + g_AppearanceSeedSalts0.x + 233u) : scaleRatio;
    const float initialColorRatio = g_AppearanceSeedSalts0.y != 0u ? Hash01(seed + g_AppearanceSeedSalts0.y + 347u) : colorRatio;
    const float initialAlphaRatio = g_AppearanceSeedSalts0.z != 0u ? Hash01(seed + g_AppearanceSeedSalts0.z + 359u) : colorRatio;
    const float colorOverLifeRatio = g_AppearanceSeedSalts0.w != 0u ? Hash01(seed + g_AppearanceSeedSalts0.w + 367u) : colorRatio;
    const float alphaOverLifeRatio = g_AppearanceSeedSalts1.x != 0u ? Hash01(seed + g_AppearanceSeedSalts1.x + 373u) : colorRatio;
    const float2 baseSize = lerp(g_SizeMin.xy, g_SizeMax.xy, sizeRatio);

    const float lifeMax = max(lifecycle.lifeMax, 0.0001f);
    const float lifeAge = max(0.0f, lifecycle.lifeAge);
    const float lifeProgress = saturate(lifeAge / lifeMax);
    const float2 sizeByLifeMultiplier = EvaluateSizeByLifeMultiplier(lifeProgress);
    const float sizeX = baseSize.x * sizeByLifeMultiplier.x;
    const float sizeY = baseSize.y * sizeByLifeMultiplier.y;

    // particle index에서 만든 deterministic seed다.
    // 같은 particle 생애 안에서는 같은 seed를 사용하므로 spawn offset, speed, rotation 랜덤값이 흔들리지 않는다.
    // 뒤의 +101u, +211u 같은 offset은 X/Y/Z나 속성별 샘플이 서로 같은 패턴을 공유하지 않게 분리하기 위한 값이다.
    const uint initialLocationSeed = seed + g_SpawnSeedSalts.y;
    const uint sphereLocationSeed = seed + g_SpawnSeedSalts.z;
    const uint planeRadialLocationSeed = seed;
    const uint cylinderLocationSeed = seed;
    const uint initialVelocitySeed = seed + g_MotionSeedSalts0.x;
    const uint initialRadialVelocitySeed = seed + g_MotionSeedSalts0.y;
    const uint velocityConeSeed = seed + g_MotionSeedSalts0.z;
    const uint accelerationSeed = seed + g_MotionSeedSalts0.w;
    const uint dragSeed = seed + g_MotionSeedSalts1.x;
    const uint initialRotationSeed = seed + g_RotationSeedSalts.x;
    const uint initialRotationRateSeed = seed + g_RotationSeedSalts.y;
    const uint initialTiltSeed = seed + g_SpriteTiltSeedSalts.x;
    const uint tiltOverLifeSeed = seed + g_SpriteTiltSeedSalts.y;
    const uint subUVRandomFrameSeed = seed + g_SubUVSeedSalts.x;

    const float3 locationRandom = float3(Hash01(initialLocationSeed), Hash01(initialLocationSeed + 101u), Hash01(initialLocationSeed + 211u));
    const float3 fallbackSpawnOffset = ResolveEmitterBasisVector((locationRandom * 2.0f - 1.0f) * g_SpawnRange.xyz * 0.5f);
    const float3 initialLocationOffset = ResolveEmitterBasisVector(lerp(g_InitialLocationMin.xyz, g_InitialLocationMax.xyz, locationRandom));
    PlaneRadialFrame planeRadialFrame;
    SamplePlaneRadialFrame(
        planeRadialLocationSeed,
        index,
        lifecycle.placementIndex,
        lifecycle.placementCount,
        planeRadialFrame
    );
    CylinderFrame cylinderFrame;
    SampleCylinderFrame(
        cylinderLocationSeed,
        lifecycle.placementIndex,
        lifecycle.placementCount,
        cylinderFrame
    );
    const bool hasExplicitLocation = g_InitialLocationMin.w != 0.0f ||
                                     g_SphereLocationParams.x != 0.0f ||
                                     planeRadialFrame.enabled != 0.0f ||
                                     cylinderFrame.enabled != 0.0f;
    const float3 baseLocationOffset = g_InitialLocationMin.w != 0.0f ? initialLocationOffset : float3(0.0f, 0.0f, 0.0f);
    const float3 planeRadialOffset = planeRadialFrame.enabled != 0.0f
                                     ? ResolveEmitterBasisVector(planeRadialFrame.localOffset)
                                     : float3(0.0f, 0.0f, 0.0f);
    const float3 cylinderOffset = cylinderFrame.enabled != 0.0f
                                  ? ResolveEmitterBasisVector(cylinderFrame.localOffset)
                                  : float3(0.0f, 0.0f, 0.0f);
    const float3 spawnOffset = (hasExplicitLocation ? baseLocationOffset : fallbackSpawnOffset) +
                               SampleSphereLocationOffset(sphereLocationSeed, lifecycle.placementIndex, lifecycle.placementCount) +
                               planeRadialOffset +
                               cylinderOffset;
    const float3 orbitSpawnOffset = ApplyOrbitOverLife(spawnOffset, lifeProgress);
    const float3 spawnPosition = g_Center.xyz + orbitSpawnOffset;
    float3 initialVelocity = float3(0.0f, 0.0f, 0.0f);
    float3 initialRadialVelocity = float3(0.0f, 0.0f, 0.0f);
    float3 velocityCone = float3(0.0f, 0.0f, 0.0f);
    float3 sourceMotionVelocity = float3(0.0f, 0.0f, 0.0f);

    if (g_MotionFlags.y != 0.0f)
    {
        const float3 sampledVelocity = SampleRange3(g_InitialVelocityMin.xyz, g_InitialVelocityMax.xyz, initialVelocitySeed + 503u);
        initialVelocity = g_MotionFlags.z != 0.0f ? sampledVelocity : ResolveEmitterBasisVector(sampledVelocity);
    }

    if (g_MotionFlags.w != 0.0f)
    {
        const float3 radialPivotOffset = g_MotionSpaceFlags.x != 0.0f
                                         ? g_MotionRadialPivot.xyz
                                         : ResolveEmitterBasisVector(g_MotionRadialPivot.xyz);
        const float3 pivotPosition = g_Center.xyz + radialPivotOffset;
        float3 radialDirection = spawnPosition - pivotPosition;
        const float3 fallbackRadialDirection = ResolveEmitterBasisVector(
            float3(
                Hash01(initialRadialVelocitySeed + 307u) * 2.0f - 1.0f,
                0.25f,
                Hash01(initialRadialVelocitySeed + 401u) * 2.0f - 1.0f
            )
        );
        float3 centerRadialDirection = fallbackRadialDirection;
        if ((uint)g_MotionSpaceFlags.w == 1u && planeRadialFrame.enabled != 0.0f)
            centerRadialDirection = ResolveEmitterBasisVector(planeRadialFrame.radialDirection);
        const float centerRadialLength = length(centerRadialDirection);
        const float3 normalizedCenterRadialDirection = centerRadialLength < 0.0001f
                                                       ? float3(0.0f, 1.0f, 0.0f)
                                                       : centerRadialDirection / centerRadialLength;
        const float radialDirectionLength = length(radialDirection);
        radialDirection = radialDirectionLength < 0.0001f
                          ? normalizedCenterRadialDirection
                          : radialDirection / radialDirectionLength;
        const float radialSpeed = lerp(g_MotionRadialSpeedDrag.x, g_MotionRadialSpeedDrag.y, Hash01(initialRadialVelocitySeed + 509u));
        initialRadialVelocity += radialDirection * radialSpeed;
    }

    if (g_MotionSpaceFlags.y != 0.0f)
    {
        const float3 sampledConeDirection = SampleConeDirection(g_VelocityConeAxisAngle.xyz, g_VelocityConeAxisAngle.w, velocityConeSeed);
        const float3 coneDirection = g_MotionSpaceFlags.z != 0.0f
                                     ? sampledConeDirection
                                     : ResolveEmitterBasisVector(sampledConeDirection);
        const float coneSpeed = lerp(g_VelocityConeSpeed.x, g_VelocityConeSpeed.y, Hash01(velocityConeSeed + 1747u));
        velocityCone += coneDirection * coneSpeed;
    }

    if (g_SourceMotionVelocityParams.x != 0.0f)
    {
        const uint sourceMotionVelocitySeed = seed + g_SourceMotionVelocitySeedSalts.x;
        sourceMotionVelocity += ResolveSourceMotionVelocity(g_SourceMotionVelocity.xyz, sourceMotionVelocitySeed);
    }

    const float drag = max(0.0f, lerp(g_MotionRadialSpeedDrag.z, g_MotionRadialSpeedDrag.w, Hash01(dragSeed + 607u)));
    const float initialVelocityScaleAverage = EvaluateInitialVelocityScaleByLifeAverage(lifeProgress);
    const float initialRadialVelocityScaleAverage = EvaluateInitialRadialVelocityScaleByLifeAverage(lifeProgress);
    const float velocityConeScaleAverage = EvaluateVelocityConeScaleByLifeAverage(lifeProgress);
    const float sourceMotionVelocityScaleAverage = EvaluateSourceMotionVelocityScaleByLifeAverage(lifeProgress);
    const float accelerationIntegratedVelocityScaleAverage = EvaluateAccelerationIntegratedVelocityScaleByLifeAverage(lifeProgress);
    const float initialVelocityScale = EvaluateInitialVelocityScaleByLife(lifeProgress);
    const float initialRadialVelocityScale = EvaluateInitialRadialVelocityScaleByLife(lifeProgress);
    const float velocityConeScale = EvaluateVelocityConeScaleByLife(lifeProgress);
    const float sourceMotionVelocityScale = EvaluateSourceMotionVelocityScaleByLife(lifeProgress);
    const float accelerationIntegratedVelocityScale = EvaluateAccelerationIntegratedVelocityScaleByLife(lifeProgress);
    const float dragTime = drag > 0.0001f ? (1.0f - exp(-drag * lifeAge)) / drag : lifeAge;
    const float dragDecay = drag > 0.0001f ? exp(-drag * lifeAge) : 1.0f;
    const bool accelerationInWorldSpace = g_AccelerationCurveParams.z != 0.0f;
    const float3 sampledAcceleration = SampleRange3(g_AccelerationMin.xyz, g_AccelerationMax.xyz, accelerationSeed + 701u);
    const float3 acceleration = accelerationInWorldSpace
                                ? sampledAcceleration
                                : ResolveEmitterBasisVector(sampledAcceleration);
    float3 accelerationVelocity = acceleration * lifeAge;
    float3 accelerationOffset = acceleration * 0.5f * lifeAge * lifeAge;
    if (g_AccelerationCurveParams.y != 0.0f)
    {
        float3 accelerationVelocityProgress = float3(0.0f, 0.0f, 0.0f);
        float3 accelerationOffsetProgress = float3(0.0f, 0.0f, 0.0f);
        const float emitterDuration = g_AccelerationCurveParams.w;
        if (emitterDuration > 0.0f)
        {
            const float currentEmitterProgress = saturate(g_ElapsedTime / emitterDuration);
            const float spawnEmitterProgress = saturate((g_ElapsedTime - lifeAge) / emitterDuration);
            IntegrateAccelerationCurveRange(
                spawnEmitterProgress,
                currentEmitterProgress,
                accelerationVelocityProgress,
                accelerationOffsetProgress
            );
            accelerationVelocity = accelerationVelocityProgress * emitterDuration;
            accelerationOffset = accelerationOffsetProgress * emitterDuration * emitterDuration;
        }
        else
        {
            IntegrateAccelerationCurve(lifeProgress, accelerationVelocityProgress, accelerationOffsetProgress);
            accelerationVelocity = accelerationVelocityProgress * lifeMax;
            accelerationOffset = accelerationOffsetProgress * lifeMax * lifeMax;
        }
        if (!accelerationInWorldSpace)
        {
            accelerationVelocity = ResolveEmitterBasisVector(accelerationVelocity);
            accelerationOffset = ResolveEmitterBasisVector(accelerationOffset);
        }
    }

    const float3 scaledInitialVelocityOffset =
        initialVelocity * initialVelocityScaleAverage +
        initialRadialVelocity * initialRadialVelocityScaleAverage +
        velocityCone * velocityConeScaleAverage +
        sourceMotionVelocity * sourceMotionVelocityScaleAverage;
    const float3 scaledAccelerationOffset = accelerationOffset * accelerationIntegratedVelocityScaleAverage;
    const float3 motionOffset = scaledInitialVelocityOffset * dragTime + scaledAccelerationOffset;
    const float3 scaledCurrentVelocity =
        initialVelocity * initialVelocityScale +
        initialRadialVelocity * initialRadialVelocityScale +
        velocityCone * velocityConeScale +
        sourceMotionVelocity * sourceMotionVelocityScale;
    const float3 currentVelocity =
        scaledCurrentVelocity * dragDecay +
        accelerationVelocity * accelerationIntegratedVelocityScale;
    const float currentVelocityLength = length(currentVelocity);
    const float3 velocityAlignmentHint = currentVelocityLength < 0.0001f
                                         ? float3(0.0f, 0.0f, 0.0f)
                                         : currentVelocity / currentVelocityLength;

    const bool useCylinderSpriteOrientation = IsCylinderSpriteOrientationEnabled(cylinderFrame);
    const float radialSpriteRotation = useCylinderSpriteOrientation
                                       ? ResolveCylinderSpriteRollOffset(cylinderFrame)
                                       : ResolvePlaneRadialSpriteRotation(planeRadialFrame.enabled, planeRadialFrame.sampleAngle);
    const float initialRotation = radians(lerp(g_RotationInitialRate.x, g_RotationInitialRate.y, Hash01(initialRotationSeed + 809u))) +
                                  radialSpriteRotation;
    const float rotationOverLife = radians(EvaluateRotationOverLifeDegrees(lifeProgress));
    const float rotationRate = radians(lerp(g_RotationInitialRate.z, g_RotationInitialRate.w, Hash01(initialRotationRateSeed + 907u)));
    const float rotationRateScale = EvaluateRotationRateScaleByLifeAverage(lifeProgress);
    const float rotation = initialRotation + rotationOverLife + rotationRate * rotationRateScale * lifeAge;
    const float rotationCos = cos(rotation);
    const float rotationSin = sin(rotation);
    const float2 initialTiltDegrees =
        g_SpriteTiltParams.x != 0.0f
        ? SampleRange2(g_SpriteTiltInitial.xy, g_SpriteTiltInitial.zw, initialTiltSeed + 617u)
        : float2(0.0f, 0.0f);
    const float2 sampledTiltOverLifeDegrees =
        g_SpriteTiltParams.y != 0.0f && g_SpriteTiltParams.w == 0.0f
        ? SampleRange2(g_SpriteTiltOverLife.xy, g_SpriteTiltOverLife.zw, tiltOverLifeSeed + 631u)
        : float2(0.0f, 0.0f);
    const float2 spriteTiltDegrees = initialTiltDegrees + EvaluateSpriteTiltOverLifeDegrees(lifeProgress, sampledTiltOverLifeDegrees);

    const uint rows = max(1u, (uint)g_SubUVParams.x);
    const uint cols = max(1u, (uint)g_SubUVParams.y);
    const uint frameCount = max(1u, rows * cols);
    uint frameIndex = 0u;

    if (g_SubUVFrameParams.w != 0.0f)
    {
        const uint lastFrame = frameCount - 1u;
        const uint startFrame = min((uint)g_SubUVFrameParams.x, lastFrame);
        const uint endFrame = min((uint)g_SubUVFrameParams.y, lastFrame);
        const uint rangeCount =
            endFrame >= startFrame
            ? endFrame - startFrame + 1u
            : frameCount - startFrame + endFrame + 1u;

        if ((uint)g_SubUVParams.z == 0u)
            frameIndex = startFrame;
        else if ((uint)g_SubUVParams.z == 1u)
        {
            const uint frameOffset = min(
                (uint)floor(saturate(lifeProgress) * (float)rangeCount),
                rangeCount - 1u
            );
            frameIndex = (startFrame + frameOffset) % frameCount;
        }
        else if ((uint)g_SubUVParams.z == 2u)
        {
            const uint frameOffset = (uint)floor(lifeAge * max(0.0f, g_SubUVParams.w));
            const uint phaseOffset =
                g_SubUVFrameCurveParams.y != 0.0f
                ? min((uint)floor(Hash01(subUVRandomFrameSeed + 3919u) * (float)rangeCount), rangeCount - 1u)
                : 0u;
            const uint playbackOffset =
                g_SubUVFrameParams.z != 0.0f
                ? (frameOffset + phaseOffset) % rangeCount
                : min(frameOffset + phaseOffset, rangeCount - 1u);
            frameIndex = (startFrame + playbackOffset) % frameCount;
        }
        else if ((uint)g_SubUVParams.z == 3u)
        {
            const uint frameOffset = min(
                (uint)floor(Hash01(subUVRandomFrameSeed + 1201u) * (float)rangeCount),
                rangeCount - 1u
            );
            frameIndex = (startFrame + frameOffset) % frameCount;
        }
    }

    frameIndex = min(frameIndex, frameCount - 1u);

    const uint row = frameIndex / cols;
    const uint col = frameIndex % cols;
    const float invCols = 1.0f / (float)cols;
    const float invRows = 1.0f / (float)rows;
    const float u0 = (float)col * invCols;
    const float v0 = (float)row * invRows;

    ParticleInstanceVertex instance;
    instance.right = float4(rotationCos * sizeX, rotationSin * sizeX, 0.0f, 0.0f);
    instance.up = float4(-rotationSin * sizeY, rotationCos * sizeY, 0.0f, 0.0f);
    instance.look = float4(ResolveCylinderSpriteLookDirection(cylinderFrame, lifeProgress, velocityAlignmentHint), 0.0f);
    instance.translation = float4(spawnPosition + motionOffset, 1.0f);
    instance.lifeTime = float2(lifeMax, lifeAge);
    const float4 legacyStartColor = float4(
        lerp(g_StartColorMin.rgb, g_StartColorMax.rgb, initialColorRatio),
        lerp(g_StartColorMin.a, g_StartColorMax.a, initialAlphaRatio)
    );
    const float4 legacyEndColor = float4(
        lerp(g_EndColorMin.rgb, g_EndColorMax.rgb, colorOverLifeRatio),
        lerp(g_EndColorMin.a, g_EndColorMax.a, alphaOverLifeRatio)
    );
    const float4 particleColor = EvaluateColorOverLife(lifeProgress, lerp(legacyStartColor, legacyEndColor, lifeProgress));
    instance.startColor = particleColor;
    instance.endColor = particleColor;
    instance.coreColorRgb = float4(SampleCoreColorRgbParticleLifeUniform(seed), 0.0f);
    instance.subUVRect = float4(u0, v0, u0 + invCols, v0 + invRows);
    instance.spriteTiltDegrees = spriteTiltDegrees;

    if (index >= stableDrawCount)
    {
        LifecycleState[index] = lifecycle;
        return;
    }

    OutputData[index] = instance;
    LifecycleState[index] = lifecycle;
}
