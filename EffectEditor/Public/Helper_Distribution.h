#pragma once

#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

inline void Normalize_FloatUniform(UniformFloatDistributionData& data)
{
    if (data.minValue <= data.maxValue)
        return;

    const float previousMin = data.minValue;
    data.minValue = data.maxValue;
    data.maxValue = previousMin;
}

inline void Normalize_Vector2Uniform(UniformVector2DistributionData& data)
{
    if (data.minValue.x > data.maxValue.x)
    {
        const float previousMin = data.minValue.x;
        data.minValue.x = data.maxValue.x;
        data.maxValue.x = previousMin;
    }

    if (data.minValue.y > data.maxValue.y)
    {
        const float previousMin = data.minValue.y;
        data.minValue.y = data.maxValue.y;
        data.maxValue.y = previousMin;
    }
}

inline void Normalize_Vector3Uniform(UniformVector3DistributionData& data)
{
    if (data.minValue.x > data.maxValue.x)
    {
        const float previousMin = data.minValue.x;
        data.minValue.x = data.maxValue.x;
        data.maxValue.x = previousMin;
    }

    if (data.minValue.y > data.maxValue.y)
    {
        const float previousMin = data.minValue.y;
        data.minValue.y = data.maxValue.y;
        data.maxValue.y = previousMin;
    }

    if (data.minValue.z > data.maxValue.z)
    {
        const float previousMin = data.minValue.z;
        data.minValue.z = data.maxValue.z;
        data.maxValue.z = previousMin;
    }
}

inline void Clamp_Color01(Color& color)
{
    color.R(clamp(color.R(), 0.f, 1.f));
    color.G(clamp(color.G(), 0.f, 1.f));
    color.B(clamp(color.B(), 0.f, 1.f));
    color.A(clamp(color.A(), 0.f, 1.f));
}

inline void Normalize_ColorUniform(UniformColorDistributionData& data)
{
    Clamp_Color01(data.minValue);
    Clamp_Color01(data.maxValue);

    if (data.minValue.R() > data.maxValue.R())
    {
        const float previousMin = data.minValue.R();
        data.minValue.R(data.maxValue.R());
        data.maxValue.R(previousMin);
    }

    if (data.minValue.G() > data.maxValue.G())
    {
        const float previousMin = data.minValue.G();
        data.minValue.G(data.maxValue.G());
        data.maxValue.G(previousMin);
    }

    if (data.minValue.B() > data.maxValue.B())
    {
        const float previousMin = data.minValue.B();
        data.minValue.B(data.maxValue.B());
        data.maxValue.B(previousMin);
    }

    if (data.minValue.A() > data.maxValue.A())
    {
        const float previousMin = data.minValue.A();
        data.minValue.A(data.maxValue.A());
        data.maxValue.A(previousMin);
    }
}

NS_END
