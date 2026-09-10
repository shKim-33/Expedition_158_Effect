// Engine 원본 경로: Engine/Bin/ShaderFiles/Engine_Shader_ComputeAnimTypes.hlsli
// Client 복사본 경로: Client/Bin/ShaderFiles/Engine_Shader_ComputeAnimTypes.hlsli - 먼저 이 원본의 의도를 확인하세요.
#ifndef ENGINE_SHADER_COMPUTE_ANIM_TYPES_HLSLI
#define ENGINE_SHADER_COMPUTE_ANIM_TYPES_HLSLI

struct PoseSRT
{
    float3 Scale;
    float4 Rotation;
    float3 Translation;
    float Pad0;
};

struct TrackInput
{
    uint AnimationIndex;
    float TrackPositionSec;
    float Weight;
    uint Flags;
};

struct AnimationMeta
{
    uint ChannelOffset;
    uint ChannelCount;
    float DurationSec;
    float TickPerSecond;
};

struct ChannelMeta
{
    int BoneIndex;
    uint KeyframeOffset;
    uint KeyframeCount;
};

struct Keyframe
{
    float3 Scale;
    float4 Rotation;
    float3 Translation;
    float TrackPosition;
};

struct BoneParent
{
    int ParentIndex;
    uint Pad0;
    uint Pad1;
    uint Pad2;
};

struct MeshBoneRef
{
    uint sourceBoneIndex;
    uint pad0;
    uint pad1;
    uint pad2;
    row_major float4x4 offsetMatrix;
};

struct BoneMatrixElement
{
    row_major float4x4 BoneMatrix;
};

struct MorphVertexRange
{
    uint Start;
    uint Count;
};

struct MorphGpuInfluence
{
    uint TargetIndex;
    float WeightScale;
    float2 Pad0;
    float3 DeltaPosition;
    float Pad1;
    float3 DeltaNormal;
    float Pad2;
};

cbuffer AnimComputeCB : register(b0)
{
    uint g_NumBones;
    uint g_NumPrimaryTracks;
    uint g_NumMixTracks;
    uint g_NumAdditiveTracks;

    float g_BlendRatio;
    float g_AdjustedTimeDelta;
    float g_PreviousTimeSec;
    int g_RootIndex;

    row_major float4x4 g_PreLocalTransformMatrix;
};

#endif
