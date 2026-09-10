// Engine 원본 경로: Engine/Bin/ShaderFiles/Engine_Shader_Samplers.hlsl
// Client 복사본 경로: Client/Bin/ShaderFiles/Engine_Shader_Samplers.hlsl - 먼저 이 원본의 의도를 확인하세요.
#ifndef ENGINE_SHADER_SAMPLERS_HLSL
#define ENGINE_SHADER_SAMPLERS_HLSL

SamplerState LinearWrapSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState LinearClampSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState PointClampSampler
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState LinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
};

#endif
