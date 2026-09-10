#include "pch.h"
#include "EffectAssetRuntimeLoader_Support.h"

NS_BEGIN(Client)

namespace EffectAssetRuntimeLoad::Curves
{
    using namespace Json;

    float Evaluate_FloatCurveSegmentLinear(const json& leftKey, const json& rightKey, float x)
    {
        const float leftTime = Read_Float(leftKey, "time");
        const float rightTime = Read_Float(rightKey, "time", 1.f);
        const float width = max(0.0001f, rightTime - leftTime);
        const float t = clamp((x - leftTime) / width, 0.f, 1.f);
        return lerp(Read_Float(leftKey, "value"), Read_Float(rightKey, "value"), t);
    }

    float Evaluate_FloatCurveSegmentAutoClamped(const json& leftKey, const json& rightKey, float x)
    {
        const float leftTime = Read_Float(leftKey, "time");
        const float rightTime = Read_Float(rightKey, "time", 1.f);
        const float width = max(0.0001f, rightTime - leftTime);
        const float t = clamp((x - leftTime) / width, 0.f, 1.f);
        const float m0 = Read_Float(leftKey, "leaveTangent") * width;
        const float m1 = Read_Float(rightKey, "arriveTangent") * width;
        const float t2 = t * t;
        const float t3 = t2 * t;

        return (2.f * t3 - 3.f * t2 + 1.f) * Read_Float(leftKey, "value") +
               (t3 - 2.f * t2 + t) * m0 +
               (-2.f * t3 + 3.f * t2) * Read_Float(rightKey, "value") +
               (t3 - t2) * m1;
    }

    float Evaluate_FloatConstantCurve(const json& keys, float x, float fallbackValue)
    {
        if (!keys.is_array() || keys.empty())
            return fallbackValue;

        if (keys.size() == 1)
            return Read_Float(keys.front(), "value", fallbackValue);

        const float clampedX = clamp(x, 0.f, 1.f);
        for (size_t index = 1; index < keys.size(); ++index)
        {
            const json& leftKey = keys[index - 1];
            const json& rightKey = keys[index];
            if (clampedX > Read_Float(rightKey, "time", 1.f))
                continue;

            const string interpolationMode = Read_String(leftKey, "interpolationMode", "Linear");
            if ("Constant" == interpolationMode)
                return Read_Float(leftKey, "value", fallbackValue);

            if ("CurveAutoClamped" == interpolationMode)
                return Evaluate_FloatCurveSegmentAutoClamped(leftKey, rightKey, clampedX);

            return Evaluate_FloatCurveSegmentLinear(leftKey, rightKey, clampedX);
        }

        return Read_Float(keys.back(), "value", fallbackValue);
    }

    Vec2 Evaluate_Vector2CurveSegmentLinear(const json& leftKey, const json& rightKey, float x)
    {
        const float leftTime = Read_Float(leftKey, "time");
        const float rightTime = Read_Float(rightKey, "time", 1.f);
        const float width = max(0.0001f, rightTime - leftTime);
        const float t = clamp((x - leftTime) / width, 0.f, 1.f);

        Vec2 leftValue{};
        Vec2 rightValue{};
        if (const auto iter = leftKey.find("value"); iter != leftKey.end())
            Read_Vec2(*iter, leftValue);
        if (const auto iter = rightKey.find("value"); iter != rightKey.end())
            Read_Vec2(*iter, rightValue);

        return Vec2{ lerp(leftValue.x, rightValue.x, t), lerp(leftValue.y, rightValue.y, t) };
    }

    Vec2 Evaluate_Vector2CurveSegmentAutoClamped(const json& leftKey, const json& rightKey, float x)
    {
        const float leftTime = Read_Float(leftKey, "time");
        const float rightTime = Read_Float(rightKey, "time", 1.f);
        const float width = max(0.0001f, rightTime - leftTime);
        const float t = clamp((x - leftTime) / width, 0.f, 1.f);
        const float t2 = t * t;
        const float t3 = t2 * t;

        Vec2 leftValue{};
        Vec2 rightValue{};
        Vec2 leftTangent{};
        Vec2 rightTangent{};
        if (const auto iter = leftKey.find("value"); iter != leftKey.end())
            Read_Vec2(*iter, leftValue);
        if (const auto iter = rightKey.find("value"); iter != rightKey.end())
            Read_Vec2(*iter, rightValue);
        if (const auto iter = leftKey.find("leaveTangent"); iter != leftKey.end())
            Read_Vec2(*iter, leftTangent);
        if (const auto iter = rightKey.find("arriveTangent"); iter != rightKey.end())
            Read_Vec2(*iter, rightTangent);

        const Vec2 m0{ leftTangent.x * width, leftTangent.y * width };
        const Vec2 m1{ rightTangent.x * width, rightTangent.y * width };
        return Vec2{
            (2.f * t3 - 3.f * t2 + 1.f) * leftValue.x +
            (t3 - 2.f * t2 + t) * m0.x +
            (-2.f * t3 + 3.f * t2) * rightValue.x +
            (t3 - t2) * m1.x,
            (2.f * t3 - 3.f * t2 + 1.f) * leftValue.y +
            (t3 - 2.f * t2 + t) * m0.y +
            (-2.f * t3 + 3.f * t2) * rightValue.y +
            (t3 - t2) * m1.y
        };
    }

    Vec2 Evaluate_Vector2ConstantCurve(const json& keys, float x, const Vec2& fallbackValue)
    {
        if (!keys.is_array() || keys.empty())
            return fallbackValue;

        auto read_value = [](const json& key, const Vec2& fallback)
        {
            Vec2 value = fallback;
            if (const auto iter = key.find("value"); iter != key.end())
                Read_Vec2(*iter, value);
            return value;
        };

        if (keys.size() == 1)
            return read_value(keys.front(), fallbackValue);

        const float clampedX = clamp(x, 0.f, 1.f);
        for (size_t index = 1; index < keys.size(); ++index)
        {
            const json& leftKey = keys[index - 1];
            const json& rightKey = keys[index];
            if (clampedX > Read_Float(rightKey, "time", 1.f))
                continue;

            const string interpolationMode = Read_String(leftKey, "interpolationMode", "Linear");
            if ("Constant" == interpolationMode)
                return read_value(leftKey, fallbackValue);

            if ("CurveAutoClamped" == interpolationMode)
                return Evaluate_Vector2CurveSegmentAutoClamped(leftKey, rightKey, clampedX);

            return Evaluate_Vector2CurveSegmentLinear(leftKey, rightKey, clampedX);
        }

        return read_value(keys.back(), fallbackValue);
    }

    Vec3 Evaluate_Vector3ConstantCurve(const json& keys, float x, const Vec3& fallbackValue)
    {
        if (!keys.is_array() || keys.empty())
            return fallbackValue;

        auto read_value = [](const json& key, const Vec3& fallback)
        {
            Vec3 value = fallback;
            if (const auto iter = key.find("value"); iter != key.end())
                Read_Vec3(*iter, value);
            return value;
        };

        if (keys.size() == 1)
            return read_value(keys.front(), fallbackValue);

        const float clampedX = clamp(x, 0.f, 1.f);
        for (size_t index = 1; index < keys.size(); ++index)
        {
            const json& leftKey = keys[index - 1];
            const json& rightKey = keys[index];
            if (clampedX > Read_Float(rightKey, "time", 1.f))
                continue;

            const string interpolationMode = Read_String(leftKey, "interpolationMode", "Linear");
            if ("Constant" == interpolationMode)
                return read_value(leftKey, fallbackValue);

            const float leftTime = Read_Float(leftKey, "time");
            const float rightTime = Read_Float(rightKey, "time", 1.f);
            const float width = max(0.0001f, rightTime - leftTime);
            const float t = clamp((clampedX - leftTime) / width, 0.f, 1.f);
            const Vec3 leftValue = read_value(leftKey, fallbackValue);
            const Vec3 rightValue = read_value(rightKey, fallbackValue);
            return Vec3{
                lerp(leftValue.x, rightValue.x, t),
                lerp(leftValue.y, rightValue.y, t),
                lerp(leftValue.z, rightValue.z, t)
            };
        }

        return read_value(keys.back(), fallbackValue);
    }

    Vec4 Evaluate_ColorConstantCurve(const json& keys, float x, const Vec4& fallbackValue)
    {
        if (!keys.is_array() || keys.empty())
            return fallbackValue;

        auto read_value = [](const json& key, const Vec4& fallback)
        {
            Vec3 rgb{ fallback.x, fallback.y, fallback.z };
            if (const auto iter = key.find("value"); iter != key.end())
                Read_Vec3(*iter, rgb);
            return Vec4{ rgb.x, rgb.y, rgb.z, fallback.w };
        };

        if (keys.size() == 1)
            return read_value(keys.front(), fallbackValue);

        const float clampedX = clamp(x, 0.f, 1.f);
        for (size_t index = 1; index < keys.size(); ++index)
        {
            const json& leftKey = keys[index - 1];
            const json& rightKey = keys[index];
            if (clampedX > Read_Float(rightKey, "time", 1.f))
                continue;

            const string interpolationMode = Read_String(leftKey, "interpolationMode", "Linear");
            if ("Constant" == interpolationMode)
                return read_value(leftKey, fallbackValue);

            const float leftTime = Read_Float(leftKey, "time");
            const float rightTime = Read_Float(rightKey, "time", 1.f);
            const float width = max(0.0001f, rightTime - leftTime);
            const float t = clamp((clampedX - leftTime) / width, 0.f, 1.f);
            const Vec4 leftValue = read_value(leftKey, fallbackValue);
            const Vec4 rightValue = read_value(rightKey, fallbackValue);
            return Vec4{
                lerp(leftValue.x, rightValue.x, t),
                lerp(leftValue.y, rightValue.y, t),
                lerp(leftValue.z, rightValue.z, t),
                fallbackValue.w
            };
        }

        return read_value(keys.back(), fallbackValue);
    }

    const json* Find_DistributionPayload(const json& distribution)
    {
        const auto payloadIter = distribution.find("payload");
        if (payloadIter == distribution.end() || !payloadIter->is_object())
            return nullptr;

        return &*payloadIter;
    }

    float Evaluate_FloatDistributionMin(const json& distribution, float fallbackValue)
    {
        const json* payload = Find_DistributionPayload(distribution);
        if (nullptr == payload)
            return fallbackValue;

        const string mode = Read_String(distribution, "mode");
        if ("Constant" == mode)
            return Read_Float(*payload, "value", fallbackValue);

        if ("Uniform" == mode)
            return Read_Float(*payload, "minValue", fallbackValue);

        if ("ConstantCurve" == mode)
        {
            const auto keysIter = payload->find("keys");
            return keysIter != payload->end() ? Evaluate_FloatConstantCurve(*keysIter, 0.f, fallbackValue) : fallbackValue;
        }

        return fallbackValue;
    }

    float Evaluate_FloatDistributionMax(const json& distribution, float fallbackValue)
    {
        const json* payload = Find_DistributionPayload(distribution);
        if (nullptr == payload)
            return fallbackValue;

        const string mode = Read_String(distribution, "mode");
        if ("Constant" == mode)
            return Read_Float(*payload, "value", fallbackValue);

        if ("Uniform" == mode)
            return Read_Float(*payload, "maxValue", fallbackValue);

        if ("ConstantCurve" == mode)
        {
            const auto keysIter = payload->find("keys");
            return keysIter != payload->end() ? Evaluate_FloatConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue;
        }

        return fallbackValue;
    }

    Vec2 Evaluate_Vector2DistributionMin(const json& distribution, const Vec2& fallbackValue)
    {
        const json* payload = Find_DistributionPayload(distribution);
        if (nullptr == payload)
            return fallbackValue;

        Vec2 value = fallbackValue;
        if ("Uniform" == Read_String(distribution, "mode"))
        {
            const auto iter = payload->find("minValue");
            if (iter != payload->end())
                Read_Vec2(*iter, value);
            return value;
        }

        if ("ConstantCurve" == Read_String(distribution, "mode"))
        {
            const auto keysIter = payload->find("keys");
            return keysIter != payload->end() ? Evaluate_Vector2ConstantCurve(*keysIter, 0.f, fallbackValue) : fallbackValue;
        }

        const auto iter = payload->find("value");
        if (iter != payload->end())
            Read_Vec2(*iter, value);
        return value;
    }

    Vec2 Evaluate_Vector2DistributionMax(const json& distribution, const Vec2& fallbackValue)
    {
        const json* payload = Find_DistributionPayload(distribution);
        if (nullptr == payload)
            return fallbackValue;

        Vec2 value = fallbackValue;
        if ("Uniform" == Read_String(distribution, "mode"))
        {
            const auto iter = payload->find("maxValue");
            if (iter != payload->end())
                Read_Vec2(*iter, value);
            return value;
        }

        if ("ConstantCurve" == Read_String(distribution, "mode"))
        {
            const auto keysIter = payload->find("keys");
            return keysIter != payload->end() ? Evaluate_Vector2ConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue;
        }

        const auto iter = payload->find("value");
        if (iter != payload->end())
            Read_Vec2(*iter, value);
        return value;
    }

    Vec3 Evaluate_Vector3DistributionMin(const json& distribution, const Vec3& fallbackValue)
    {
        const json* payload = Find_DistributionPayload(distribution);
        if (nullptr == payload)
            return fallbackValue;

        Vec3 value = fallbackValue;
        if ("Uniform" == Read_String(distribution, "mode"))
        {
            const auto iter = payload->find("minValue");
            if (iter != payload->end())
                Read_Vec3(*iter, value);
            return value;
        }

        if ("ConstantCurve" == Read_String(distribution, "mode"))
        {
            const auto keysIter = payload->find("keys");
            return keysIter != payload->end() ? Evaluate_Vector3ConstantCurve(*keysIter, 0.f, fallbackValue) : fallbackValue;
        }

        const auto iter = payload->find("value");
        if (iter != payload->end())
            Read_Vec3(*iter, value);
        return value;
    }

    Vec3 Evaluate_Vector3DistributionMax(const json& distribution, const Vec3& fallbackValue)
    {
        const json* payload = Find_DistributionPayload(distribution);
        if (nullptr == payload)
            return fallbackValue;

        Vec3 value = fallbackValue;
        if ("Uniform" == Read_String(distribution, "mode"))
        {
            const auto iter = payload->find("maxValue");
            if (iter != payload->end())
                Read_Vec3(*iter, value);
            return value;
        }

        if ("ConstantCurve" == Read_String(distribution, "mode"))
        {
            const auto keysIter = payload->find("keys");
            return keysIter != payload->end() ? Evaluate_Vector3ConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue;
        }

        const auto iter = payload->find("value");
        if (iter != payload->end())
            Read_Vec3(*iter, value);
        return value;
    }

    Vec4 Clamp_Color(Vec4 value)
    {
        value.x = clamp(value.x, 0.f, 1.f);
        value.y = clamp(value.y, 0.f, 1.f);
        value.z = clamp(value.z, 0.f, 1.f);
        value.w = clamp(value.w, 0.f, 1.f);
        return value;
    }

    Vec4 Evaluate_ColorDistributionMin(const json& distribution, const Vec4& fallbackValue)
    {
        const json* payload = Find_DistributionPayload(distribution);
        if (nullptr == payload)
            return Clamp_Color(fallbackValue);

        Vec4 value = fallbackValue;
        if ("Uniform" == Read_String(distribution, "mode"))
        {
            const auto iter = payload->find("minValue");
            if (iter != payload->end())
                Read_Vec4(*iter, value);
            return Clamp_Color(value);
        }

        if ("ConstantCurve" == Read_String(distribution, "mode"))
        {
            const auto keysIter = payload->find("keys");
            return Clamp_Color(keysIter != payload->end() ? Evaluate_ColorConstantCurve(*keysIter, 0.f, fallbackValue) : fallbackValue);
        }

        const auto iter = payload->find("value");
        if (iter != payload->end())
            Read_Vec4(*iter, value);
        return Clamp_Color(value);
    }

    Vec4 Evaluate_ColorDistributionMax(const json& distribution, const Vec4& fallbackValue)
    {
        const json* payload = Find_DistributionPayload(distribution);
        if (nullptr == payload)
            return Clamp_Color(fallbackValue);

        Vec4 value = fallbackValue;
        if ("Uniform" == Read_String(distribution, "mode"))
        {
            const auto iter = payload->find("maxValue");
            if (iter != payload->end())
                Read_Vec4(*iter, value);
            return Clamp_Color(value);
        }

        if ("ConstantCurve" == Read_String(distribution, "mode"))
        {
            const auto keysIter = payload->find("keys");
            return Clamp_Color(keysIter != payload->end() ? Evaluate_ColorConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue);
        }

        const auto iter = payload->find("value");
        if (iter != payload->end())
            Read_Vec4(*iter, value);
        return Clamp_Color(value);
    }

    Vec4 Evaluate_ColorOverLifeEndpointColorMin(const json& distribution, const Vec4& fallbackValue)
    {
        const json* payload = Find_DistributionPayload(distribution);
        if (nullptr == payload)
            return Clamp_Color(fallbackValue);

        if ("ConstantCurve" == Read_String(distribution, "mode"))
        {
            const auto keysIter = payload->find("keys");
            return Clamp_Color(keysIter != payload->end() ? Evaluate_ColorConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue);
        }

        return Evaluate_ColorDistributionMin(distribution, fallbackValue);
    }

    Vec4 Evaluate_ColorOverLifeEndpointColorMax(const json& distribution, const Vec4& fallbackValue)
    {
        const json* payload = Find_DistributionPayload(distribution);
        if (nullptr == payload)
            return Clamp_Color(fallbackValue);

        if ("ConstantCurve" == Read_String(distribution, "mode"))
        {
            const auto keysIter = payload->find("keys");
            return Clamp_Color(keysIter != payload->end() ? Evaluate_ColorConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue);
        }

        return Evaluate_ColorDistributionMax(distribution, fallbackValue);
    }

    float Evaluate_ColorOverLifeEndpointAlphaMin(const json& distribution, float fallbackValue)
    {
        const json* payload = Find_DistributionPayload(distribution);
        if (nullptr == payload)
            return fallbackValue;

        if ("ConstantCurve" == Read_String(distribution, "mode"))
        {
            const auto keysIter = payload->find("keys");
            return keysIter != payload->end() ? Evaluate_FloatConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue;
        }

        return Evaluate_FloatDistributionMin(distribution, fallbackValue);
    }

    float Evaluate_ColorOverLifeEndpointAlphaMax(const json& distribution, float fallbackValue)
    {
        const json* payload = Find_DistributionPayload(distribution);
        if (nullptr == payload)
            return fallbackValue;

        if ("ConstantCurve" == Read_String(distribution, "mode"))
        {
            const auto keysIter = payload->find("keys");
            return keysIter != payload->end() ? Evaluate_FloatConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue;
        }

        return Evaluate_FloatDistributionMax(distribution, fallbackValue);
    }

    float Resolve_CurveInterpolationMode(const json& key)
    {
        const string interpolationMode = Read_String(key, "interpolationMode", "Linear");
        if ("Constant" == interpolationMode)
            return 0.f;
        if ("CurveAutoClamped" == interpolationMode)
            return 2.f;
        return 1.f;
    }

    void Fill_FloatCurveKeyComponent(
        Vec4& times,
        Vec4& timesBlock1,
        Vec4& values,
        Vec4& valuesBlock1,
        Vec4& arriveTangents,
        Vec4& arriveTangentsBlock1,
        Vec4& leaveTangents,
        Vec4& leaveTangentsBlock1,
        Vec4& modes,
        Vec4& modesBlock1,
        uint32 index,
        const json& key,
        float fallbackValue = 0.f)
    {
        Set_CurveComponent(times, timesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
        Set_CurveComponent(values, valuesBlock1, index, Read_Float(key, "value", fallbackValue));
        Set_CurveComponent(arriveTangents, arriveTangentsBlock1, index, Read_Float(key, "arriveTangent"));
        Set_CurveComponent(leaveTangents, leaveTangentsBlock1, index, Read_Float(key, "leaveTangent"));
        Set_CurveComponent(modes, modesBlock1, index, Resolve_CurveInterpolationMode(key));
    }

    void Fill_ColorOverLifeCurvePayload(
        PointParticleColorOverLifeCurveDesc& desc,
        const json& colorOverLife)
    {
        constexpr uint32 maxColorOverLifeCurveKeys{ ::Engine::kEffectDistributionCurveMaxKeys };

        if (const auto colorIter = colorOverLife.find("colorOverLife"); colorIter != colorOverLife.end())
        {
            const json* payload = Find_DistributionPayload(*colorIter);
            if (payload != nullptr && "ConstantCurve" == Read_String(*colorIter, "mode"))
            {
                const auto keysIter = payload->find("keys");
                if (keysIter != payload->end() && keysIter->is_array() && !keysIter->empty())
                {
                    vector<const json*> keys{};
                    keys.reserve(keysIter->size());
                    for (const json& key : *keysIter)
                        keys.push_back(&key);
                    ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

                    const uint32 keyCount = min(maxColorOverLifeCurveKeys, static_cast<uint32>(keys.size()));
                    desc.colorCurveEnabled = true;
                    desc.colorCurveKeyCount = keyCount;
                    desc.colorCurveTimes = Vec4{};
                    desc.colorCurveTimesBlock1 = Vec4{};
                    desc.colorCurveValuesR = Vec4{};
                    desc.colorCurveValuesRBlock1 = Vec4{};
                    desc.colorCurveValuesG = Vec4{};
                    desc.colorCurveValuesGBlock1 = Vec4{};
                    desc.colorCurveValuesB = Vec4{};
                    desc.colorCurveValuesBBlock1 = Vec4{};
                    desc.colorCurveArriveR = Vec4{};
                    desc.colorCurveArriveRBlock1 = Vec4{};
                    desc.colorCurveArriveG = Vec4{};
                    desc.colorCurveArriveGBlock1 = Vec4{};
                    desc.colorCurveArriveB = Vec4{};
                    desc.colorCurveArriveBBlock1 = Vec4{};
                    desc.colorCurveLeaveR = Vec4{};
                    desc.colorCurveLeaveRBlock1 = Vec4{};
                    desc.colorCurveLeaveG = Vec4{};
                    desc.colorCurveLeaveGBlock1 = Vec4{};
                    desc.colorCurveLeaveB = Vec4{};
                    desc.colorCurveLeaveBBlock1 = Vec4{};
                    desc.colorCurveModes = Vec4{};
                    desc.colorCurveModesBlock1 = Vec4{};
                    for (uint32 index = 0; index < keyCount; ++index)
                    {
                        const json& key = *keys[index];
                        Vec3 value{ 1.f, 1.f, 1.f };
                        Vec3 arriveTangent{};
                        Vec3 leaveTangent{};
                        if (const auto valueIter = key.find("value"); valueIter != key.end())
                            Read_Vec3(*valueIter, value);
                        if (const auto arriveIter = key.find("arriveTangent"); arriveIter != key.end())
                            Read_Vec3(*arriveIter, arriveTangent);
                        if (const auto leaveIter = key.find("leaveTangent"); leaveIter != key.end())
                            Read_Vec3(*leaveIter, leaveTangent);

                        Set_CurveComponent(desc.colorCurveTimes, desc.colorCurveTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
                        Set_CurveComponent(desc.colorCurveValuesR, desc.colorCurveValuesRBlock1, index, value.x);
                        Set_CurveComponent(desc.colorCurveValuesG, desc.colorCurveValuesGBlock1, index, value.y);
                        Set_CurveComponent(desc.colorCurveValuesB, desc.colorCurveValuesBBlock1, index, value.z);
                        Set_CurveComponent(desc.colorCurveArriveR, desc.colorCurveArriveRBlock1, index, arriveTangent.x);
                        Set_CurveComponent(desc.colorCurveArriveG, desc.colorCurveArriveGBlock1, index, arriveTangent.y);
                        Set_CurveComponent(desc.colorCurveArriveB, desc.colorCurveArriveBBlock1, index, arriveTangent.z);
                        Set_CurveComponent(desc.colorCurveLeaveR, desc.colorCurveLeaveRBlock1, index, leaveTangent.x);
                        Set_CurveComponent(desc.colorCurveLeaveG, desc.colorCurveLeaveGBlock1, index, leaveTangent.y);
                        Set_CurveComponent(desc.colorCurveLeaveB, desc.colorCurveLeaveBBlock1, index, leaveTangent.z);
                        Set_CurveComponent(desc.colorCurveModes, desc.colorCurveModesBlock1, index, Resolve_CurveInterpolationMode(key));
                    }
                }
            }
        }

        if (const auto alphaIter = colorOverLife.find("alphaOverLife"); alphaIter != colorOverLife.end())
        {
            const json* payload = Find_DistributionPayload(*alphaIter);
            if (payload != nullptr && "ConstantCurve" == Read_String(*alphaIter, "mode"))
            {
                const auto keysIter = payload->find("keys");
                if (keysIter != payload->end() && keysIter->is_array() && !keysIter->empty())
                {
                    vector<const json*> keys{};
                    keys.reserve(keysIter->size());
                    for (const json& key : *keysIter)
                        keys.push_back(&key);
                    ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

                    const uint32 keyCount = min(maxColorOverLifeCurveKeys, static_cast<uint32>(keys.size()));
                    desc.alphaCurveEnabled = true;
                    desc.alphaCurveKeyCount = keyCount;
                    desc.alphaCurveTimes = Vec4{};
                    desc.alphaCurveTimesBlock1 = Vec4{};
                    desc.alphaCurveValues = Vec4{};
                    desc.alphaCurveValuesBlock1 = Vec4{};
                    desc.alphaCurveArrive = Vec4{};
                    desc.alphaCurveArriveBlock1 = Vec4{};
                    desc.alphaCurveLeave = Vec4{};
                    desc.alphaCurveLeaveBlock1 = Vec4{};
                    desc.alphaCurveModes = Vec4{};
                    desc.alphaCurveModesBlock1 = Vec4{};
                    for (uint32 index = 0; index < keyCount; ++index)
                    {
                        const json& key = *keys[index];
                        Set_CurveComponent(desc.alphaCurveTimes, desc.alphaCurveTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
                        Set_CurveComponent(desc.alphaCurveValues, desc.alphaCurveValuesBlock1, index, Read_Float(key, "value", 1.f));
                        Set_CurveComponent(desc.alphaCurveArrive, desc.alphaCurveArriveBlock1, index, Read_Float(key, "arriveTangent"));
                        Set_CurveComponent(desc.alphaCurveLeave, desc.alphaCurveLeaveBlock1, index, Read_Float(key, "leaveTangent"));
                        Set_CurveComponent(desc.alphaCurveModes, desc.alphaCurveModesBlock1, index, Resolve_CurveInterpolationMode(key));
                    }
                }
            }
        }
    }

    void Set_SubUVFrameCurveComponent(Vec4& values, uint32 index, float value)
    {
        switch (index)
        {
        case 0:
            values.x = value;
            break;
        case 1:
            values.y = value;
            break;
        case 2:
            values.z = value;
            break;
        case 3:
            values.w = value;
            break;
        default:
            break;
        }
    }

    void Set_CurveComponent(Vec4& valuesBlock0, Vec4& valuesBlock1, uint32 index, float value)
    {
        if (index < 4u)
        {
            Set_SubUVFrameCurveComponent(valuesBlock0, index, value);
            return;
        }

        Set_SubUVFrameCurveComponent(valuesBlock1, index - 4u, value);
    }

    void Fill_FloatCurvePayload(PointParticleFloatCurveRuntimeDesc& desc, const json& distribution, float fallbackValue)
    {
        desc.enabled = false;
        desc.curveKeyCount = 2u;
        desc.curveKeyTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.curveKeyTimesBlock1 = Vec4{};
        desc.curveKeyValues = Vec4{ fallbackValue, fallbackValue, 0.f, 0.f };
        desc.curveKeyValuesBlock1 = Vec4{};
        desc.curveKeyArriveTangents = Vec4{};
        desc.curveKeyArriveTangentsBlock1 = Vec4{};
        desc.curveKeyLeaveTangents = Vec4{};
        desc.curveKeyLeaveTangentsBlock1 = Vec4{};
        desc.curveKeyModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
        desc.curveKeyModesBlock1 = Vec4{};

        const json* payload = Find_DistributionPayload(distribution);
        if (payload == nullptr || "ConstantCurve" != Read_String(distribution, "mode"))
            return;

        const auto keysIter = payload->find("keys");
        if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
            return;

        vector<const json*> keys{};
        keys.reserve(keysIter->size());
        for (const json& key : *keysIter)
            keys.push_back(&key);
        ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

        const uint32 keyCount = min(::Engine::kEffectDistributionCurveMaxKeys, static_cast<uint32>(keys.size()));
        if (keyCount == 0u)
            return;

        desc.enabled = true;
        desc.curveKeyCount = keyCount;
        desc.curveKeyTimes = Vec4{};
        desc.curveKeyTimesBlock1 = Vec4{};
        desc.curveKeyValues = Vec4{};
        desc.curveKeyValuesBlock1 = Vec4{};
        desc.curveKeyArriveTangents = Vec4{};
        desc.curveKeyArriveTangentsBlock1 = Vec4{};
        desc.curveKeyLeaveTangents = Vec4{};
        desc.curveKeyLeaveTangentsBlock1 = Vec4{};
        desc.curveKeyModes = Vec4{};
        desc.curveKeyModesBlock1 = Vec4{};
        for (uint32 index = 0; index < keyCount; ++index)
        {
            const json& key = *keys[index];
            Set_CurveComponent(desc.curveKeyTimes, desc.curveKeyTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
            Set_CurveComponent(desc.curveKeyValues, desc.curveKeyValuesBlock1, index, Read_Float(key, "value", fallbackValue));
            Set_CurveComponent(desc.curveKeyArriveTangents, desc.curveKeyArriveTangentsBlock1, index, Read_Float(key, "arriveTangent"));
            Set_CurveComponent(desc.curveKeyLeaveTangents, desc.curveKeyLeaveTangentsBlock1, index, Read_Float(key, "leaveTangent"));
            Set_CurveComponent(desc.curveKeyModes, desc.curveKeyModesBlock1, index, Resolve_CurveInterpolationMode(key));
        }
    }

    void Fill_SubUVFrameCurvePayload(PointParticleSubUVFrameOverLifeDesc& desc, const json& frameIndex)
    {
        desc.frameCurveKeyCount = 1;
        desc.frameCurveTimes = Vec4{};
        desc.frameCurveTimesBlock1 = Vec4{};
        desc.frameCurveValues = Vec4{};
        desc.frameCurveValuesBlock1 = Vec4{};

        const json* payload = Find_DistributionPayload(frameIndex);
        if (nullptr == payload)
            return;

        if ("Constant" == Read_String(frameIndex, "mode"))
        {
            desc.frameCurveValues.x = Read_Float(*payload, "value");
            return;
        }

        if ("ConstantCurve" != Read_String(frameIndex, "mode"))
            return;

        const auto keysIter = payload->find("keys");
        if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
            return;

        const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keysIter->size()));
        desc.frameCurveKeyCount = keyCount;

        for (uint32 index = 0; index < keyCount; ++index)
        {
            const json& key = (*keysIter)[index];
            Set_CurveComponent(desc.frameCurveTimes, desc.frameCurveTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
            Set_CurveComponent(desc.frameCurveValues, desc.frameCurveValuesBlock1, index, Read_Float(key, "value"));
        }
    }

    void Fill_SizeByLifePayload(PointParticleSizeByLifeDesc& desc, const json& sizeByLife)
    {
        desc.enabled = true;
        desc.multiplyX = Read_Bool(sizeByLife, "multiplyX", desc.multiplyX);
        desc.multiplyY = Read_Bool(sizeByLife, "multiplyY", desc.multiplyY);
        Read_Enum(sizeByLife, "axisLock", desc.axisLock);

        const auto scaleOverLifeIter = sizeByLife.find("scaleOverLife");
        if (scaleOverLifeIter == sizeByLife.end())
        {
            desc.multiplyXStart = Read_Float(sizeByLife, "multiplyXStart", desc.multiplyXStart);
            desc.multiplyXEnd = Read_Float(sizeByLife, "multiplyXEnd", desc.multiplyXEnd);
            desc.multiplyYStart = Read_Float(sizeByLife, "multiplyYStart", desc.multiplyYStart);
            desc.multiplyYEnd = Read_Float(sizeByLife, "multiplyYEnd", desc.multiplyYEnd);
            desc.curveKeyCount = 2;
            desc.curveKeyTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
            desc.curveKeyTimesBlock1 = Vec4{};
            desc.curveKeyValuesX = Vec4{ desc.multiplyXStart, desc.multiplyXEnd, 0.f, 0.f };
            desc.curveKeyValuesXBlock1 = Vec4{};
            desc.curveKeyValuesY = Vec4{ desc.multiplyYStart, desc.multiplyYEnd, 0.f, 0.f };
            desc.curveKeyValuesYBlock1 = Vec4{};
            desc.curveKeyArriveTangentsX = Vec4{};
            desc.curveKeyArriveTangentsXBlock1 = Vec4{};
            desc.curveKeyLeaveTangentsX = Vec4{};
            desc.curveKeyLeaveTangentsXBlock1 = Vec4{};
            desc.curveKeyArriveTangentsY = Vec4{};
            desc.curveKeyArriveTangentsYBlock1 = Vec4{};
            desc.curveKeyLeaveTangentsY = Vec4{};
            desc.curveKeyLeaveTangentsYBlock1 = Vec4{};
            desc.curveKeyModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
            desc.curveKeyModesBlock1 = Vec4{};
            return;
        }

        const Vec2 startValue = Evaluate_Vector2DistributionMin(*scaleOverLifeIter, Vec2{ 1.f, 1.f });
        const Vec2 endValue = Evaluate_Vector2DistributionMax(*scaleOverLifeIter, Vec2{ 1.f, 1.f });
        desc.multiplyXStart = startValue.x;
        desc.multiplyXEnd = endValue.x;
        desc.multiplyYStart = startValue.y;
        desc.multiplyYEnd = endValue.y;
        desc.curveKeyCount = 2;
        desc.curveKeyTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.curveKeyTimesBlock1 = Vec4{};
        desc.curveKeyValuesX = Vec4{ startValue.x, endValue.x, 0.f, 0.f };
        desc.curveKeyValuesXBlock1 = Vec4{};
        desc.curveKeyValuesY = Vec4{ startValue.y, endValue.y, 0.f, 0.f };
        desc.curveKeyValuesYBlock1 = Vec4{};
        desc.curveKeyArriveTangentsX = Vec4{};
        desc.curveKeyArriveTangentsXBlock1 = Vec4{};
        desc.curveKeyLeaveTangentsX = Vec4{};
        desc.curveKeyLeaveTangentsXBlock1 = Vec4{};
        desc.curveKeyArriveTangentsY = Vec4{};
        desc.curveKeyArriveTangentsYBlock1 = Vec4{};
        desc.curveKeyLeaveTangentsY = Vec4{};
        desc.curveKeyLeaveTangentsYBlock1 = Vec4{};
        desc.curveKeyModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
        desc.curveKeyModesBlock1 = Vec4{};

        const json* payload = Find_DistributionPayload(*scaleOverLifeIter);
        if (payload == nullptr || "ConstantCurve" != Read_String(*scaleOverLifeIter, "mode"))
            return;

        const auto keysIter = payload->find("keys");
        if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
            return;

        vector<const json*> keys{};
        keys.reserve(keysIter->size());
        for (const json& key : *keysIter)
            keys.push_back(&key);
        ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

        const uint32 keyCount = min(::Engine::kEffectDistributionCurveMaxKeys, static_cast<uint32>(keys.size()));
        desc.curveKeyCount = keyCount;
        desc.curveKeyTimes = Vec4{};
        desc.curveKeyTimesBlock1 = Vec4{};
        desc.curveKeyValuesX = Vec4{};
        desc.curveKeyValuesXBlock1 = Vec4{};
        desc.curveKeyValuesY = Vec4{};
        desc.curveKeyValuesYBlock1 = Vec4{};
        desc.curveKeyArriveTangentsX = Vec4{};
        desc.curveKeyArriveTangentsXBlock1 = Vec4{};
        desc.curveKeyLeaveTangentsX = Vec4{};
        desc.curveKeyLeaveTangentsXBlock1 = Vec4{};
        desc.curveKeyArriveTangentsY = Vec4{};
        desc.curveKeyArriveTangentsYBlock1 = Vec4{};
        desc.curveKeyLeaveTangentsY = Vec4{};
        desc.curveKeyLeaveTangentsYBlock1 = Vec4{};
        desc.curveKeyModes = Vec4{};
        desc.curveKeyModesBlock1 = Vec4{};

        for (uint32 index = 0; index < keyCount; ++index)
        {
            const json& key = *keys[index];
            Vec2 value{ 1.f, 1.f };
            if (const auto valueIter = key.find("value"); valueIter != key.end())
                Read_Vec2(*valueIter, value);
            Vec2 arriveTangent{};
            Vec2 leaveTangent{};
            if (const auto tangentIter = key.find("arriveTangent"); tangentIter != key.end())
                Read_Vec2(*tangentIter, arriveTangent);
            if (const auto tangentIter = key.find("leaveTangent"); tangentIter != key.end())
                Read_Vec2(*tangentIter, leaveTangent);

            Set_CurveComponent(desc.curveKeyTimes, desc.curveKeyTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
            Set_CurveComponent(desc.curveKeyValuesX, desc.curveKeyValuesXBlock1, index, value.x);
            Set_CurveComponent(desc.curveKeyValuesY, desc.curveKeyValuesYBlock1, index, value.y);
            Set_CurveComponent(desc.curveKeyArriveTangentsX, desc.curveKeyArriveTangentsXBlock1, index, arriveTangent.x);
            Set_CurveComponent(desc.curveKeyLeaveTangentsX, desc.curveKeyLeaveTangentsXBlock1, index, leaveTangent.x);
            Set_CurveComponent(desc.curveKeyArriveTangentsY, desc.curveKeyArriveTangentsYBlock1, index, arriveTangent.y);
            Set_CurveComponent(desc.curveKeyLeaveTangentsY, desc.curveKeyLeaveTangentsYBlock1, index, leaveTangent.y);
            Set_CurveComponent(desc.curveKeyModes, desc.curveKeyModesBlock1, index, Resolve_CurveInterpolationMode(key));
        }
    }

    json Extract_FloatDistributionChannel(const json& distribution, bool useY)
    {
        json root = json::object();
        const string mode = Read_String(distribution, "mode");
        root["mode"] = mode.empty() ? "Constant" : mode;
        root["payload"] = json::object();

        const json* payload = Find_DistributionPayload(distribution);
        if (payload == nullptr)
        {
            root["mode"] = "Constant";
            root["payload"]["value"] = 0.f;
            return root;
        }

        if ("Uniform" == mode)
        {
            Vec2 minValue{};
            Vec2 maxValue{};
            if (const auto minIter = payload->find("minValue"); minIter != payload->end())
                Read_Vec2(*minIter, minValue);
            if (const auto maxIter = payload->find("maxValue"); maxIter != payload->end())
                Read_Vec2(*maxIter, maxValue);
            root["payload"]["minValue"] = useY ? minValue.y : minValue.x;
            root["payload"]["maxValue"] = useY ? maxValue.y : maxValue.x;
            if (const auto seedIter = payload->find("randomSeed"); seedIter != payload->end())
                root["payload"]["randomSeed"] = *seedIter;
            return root;
        }

        if ("ConstantCurve" == mode)
        {
            root["payload"]["keys"] = json::array();
            const auto keysIter = payload->find("keys");
            if (keysIter == payload->end() || !keysIter->is_array())
                return root;

            for (const json& key : *keysIter)
            {
                Vec2 value{};
                Vec2 arriveTangent{};
                Vec2 leaveTangent{};
                if (const auto iter = key.find("value"); iter != key.end())
                    Read_Vec2(*iter, value);
                if (const auto iter = key.find("arriveTangent"); iter != key.end())
                    Read_Vec2(*iter, arriveTangent);
                if (const auto iter = key.find("leaveTangent"); iter != key.end())
                    Read_Vec2(*iter, leaveTangent);

                json floatKey = key;
                floatKey["value"] = useY ? value.y : value.x;
                floatKey["arriveTangent"] = useY ? arriveTangent.y : arriveTangent.x;
                floatKey["leaveTangent"] = useY ? leaveTangent.y : leaveTangent.x;
                root["payload"]["keys"].push_back(move(floatKey));
            }
            return root;
        }

        Vec2 value{};
        if (const auto valueIter = payload->find("value"); valueIter != payload->end())
            Read_Vec2(*valueIter, value);
        root["mode"] = "Constant";
        root["payload"]["value"] = useY ? value.y : value.x;
        return root;
    }

    void Fill_BeamEnvelopeOverLifePayload(EffectBeamEnvelopeOverLifeRuntimeDesc& desc, const json& beamEnvelopeOverLife)
    {
        const auto legacyEnvelopeIter = beamEnvelopeOverLife.find("envelopeOverLife");
        const auto startRatioIter = beamEnvelopeOverLife.find("startRatioOverLife");
        const auto endRatioIter = beamEnvelopeOverLife.find("endRatioOverLife");
        const auto widthScaleIter = beamEnvelopeOverLife.find("widthScaleOverLife");
        const json* startRatio = startRatioIter != beamEnvelopeOverLife.end() ? &*startRatioIter : nullptr;
        const json* endRatio = endRatioIter != beamEnvelopeOverLife.end() ? &*endRatioIter : nullptr;
        const json* widthScale = widthScaleIter != beamEnvelopeOverLife.end() ? &*widthScaleIter : nullptr;

        json legacyEndRatio{};
        json legacyWidthScale{};
        if (legacyEnvelopeIter != beamEnvelopeOverLife.end())
        {
            legacyEndRatio = Extract_FloatDistributionChannel(*legacyEnvelopeIter, false);
            legacyWidthScale = Extract_FloatDistributionChannel(*legacyEnvelopeIter, true);
            if (endRatio == nullptr)
                endRatio = &legacyEndRatio;
            if (widthScale == nullptr)
                widthScale = &legacyWidthScale;
        }

        desc.enabled = true;
        if (startRatio != nullptr)
        {
            desc.startRatioFallback = Evaluate_FloatDistributionMin(*startRatio, desc.startRatioFallback);
            Fill_FloatCurvePayload(desc.startRatioOverLife, *startRatio, desc.startRatioFallback);
        }

        if (endRatio != nullptr)
        {
            desc.endRatioFallback = Evaluate_FloatDistributionMax(*endRatio, desc.endRatioFallback);
            Fill_FloatCurvePayload(desc.endRatioOverLife, *endRatio, desc.endRatioFallback);
        }

        if (widthScale != nullptr)
        {
            desc.widthScaleFallback = Evaluate_FloatDistributionMax(*widthScale, desc.widthScaleFallback);
            Fill_FloatCurvePayload(desc.widthScaleOverLife, *widthScale, desc.widthScaleFallback);
        }
    }

    void Fill_SpriteTiltOverLifeCurveKey(PointParticleSpriteTiltDesc& desc, uint32 index, const json& key)
    {
        Vec2 value{};
        Vec2 arriveTangent{};
        Vec2 leaveTangent{};
        if (const auto iter = key.find("value"); iter != key.end())
            Read_Vec2(*iter, value);
        if (const auto tangentIter = key.find("arriveTangent"); tangentIter != key.end())
            Read_Vec2(*tangentIter, arriveTangent);
        if (const auto tangentIter = key.find("leaveTangent"); tangentIter != key.end())
            Read_Vec2(*tangentIter, leaveTangent);

        Set_CurveComponent(desc.tiltOverLifeCurveTimes, desc.tiltOverLifeCurveTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
        Set_CurveComponent(desc.tiltOverLifeCurveValuesX, desc.tiltOverLifeCurveValuesXBlock1, index, value.x);
        Set_CurveComponent(desc.tiltOverLifeCurveValuesY, desc.tiltOverLifeCurveValuesYBlock1, index, value.y);
        Set_CurveComponent(desc.tiltOverLifeCurveArriveTangentsX, desc.tiltOverLifeCurveArriveTangentsXBlock1, index, arriveTangent.x);
        Set_CurveComponent(desc.tiltOverLifeCurveLeaveTangentsX, desc.tiltOverLifeCurveLeaveTangentsXBlock1, index, leaveTangent.x);
        Set_CurveComponent(desc.tiltOverLifeCurveArriveTangentsY, desc.tiltOverLifeCurveArriveTangentsYBlock1, index, arriveTangent.y);
        Set_CurveComponent(desc.tiltOverLifeCurveLeaveTangentsY, desc.tiltOverLifeCurveLeaveTangentsYBlock1, index, leaveTangent.y);
        Set_CurveComponent(desc.tiltOverLifeCurveModes, desc.tiltOverLifeCurveModesBlock1, index, Resolve_CurveInterpolationMode(key));
    }

    void Fill_SpriteTiltPayload(PointParticleSpriteTiltDesc& desc, const json* spriteTilt, const json* spriteTiltOverLife)
    {
        desc = PointParticleSpriteTiltDesc{};

        if (spriteTilt != nullptr)
        {
            const auto tiltIter = spriteTilt->find("tiltDegrees");
            if (tiltIter != spriteTilt->end())
            {
                desc.initialTiltEnabled = true;
                desc.tiltDegreesMin = Evaluate_Vector2DistributionMin(*tiltIter, desc.tiltDegreesMin);
                desc.tiltDegreesMax = Evaluate_Vector2DistributionMax(*tiltIter, desc.tiltDegreesMax);
                desc.tiltSeed = Read_DistributionRandomSeedDesc(*tiltIter, spriteTilt);
            }
        }

        if (spriteTiltOverLife == nullptr)
            return;

        const auto tiltOverLifeIter = spriteTiltOverLife->find("tiltOverLife");
        if (tiltOverLifeIter == spriteTiltOverLife->end())
            return;

        desc.tiltOverLifeEnabled = true;
        desc.tiltOverLifeMin = Evaluate_Vector2DistributionMin(*tiltOverLifeIter, desc.tiltOverLifeMin);
        desc.tiltOverLifeMax = Evaluate_Vector2DistributionMax(*tiltOverLifeIter, desc.tiltOverLifeMax);
        desc.tiltOverLifeSeed = Read_DistributionRandomSeedDesc(*tiltOverLifeIter, spriteTiltOverLife);
        desc.tiltOverLifeCurveKeyCount = 2;
        desc.tiltOverLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.tiltOverLifeCurveTimesBlock1 = Vec4{};
        desc.tiltOverLifeCurveValuesX = Vec4{ desc.tiltOverLifeMin.x, desc.tiltOverLifeMax.x, 0.f, 0.f };
        desc.tiltOverLifeCurveValuesXBlock1 = Vec4{};
        desc.tiltOverLifeCurveValuesY = Vec4{ desc.tiltOverLifeMin.y, desc.tiltOverLifeMax.y, 0.f, 0.f };
        desc.tiltOverLifeCurveValuesYBlock1 = Vec4{};
        desc.tiltOverLifeCurveArriveTangentsX = Vec4{};
        desc.tiltOverLifeCurveArriveTangentsXBlock1 = Vec4{};
        desc.tiltOverLifeCurveLeaveTangentsX = Vec4{};
        desc.tiltOverLifeCurveLeaveTangentsXBlock1 = Vec4{};
        desc.tiltOverLifeCurveArriveTangentsY = Vec4{};
        desc.tiltOverLifeCurveArriveTangentsYBlock1 = Vec4{};
        desc.tiltOverLifeCurveLeaveTangentsY = Vec4{};
        desc.tiltOverLifeCurveLeaveTangentsYBlock1 = Vec4{};
        desc.tiltOverLifeCurveModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
        desc.tiltOverLifeCurveModesBlock1 = Vec4{};

        const json* payload = Find_DistributionPayload(*tiltOverLifeIter);
        if (payload == nullptr || "ConstantCurve" != Read_String(*tiltOverLifeIter, "mode"))
            return;

        const auto keysIter = payload->find("keys");
        if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
            return;

        vector<const json*> keys{};
        keys.reserve(keysIter->size());
        for (const json& key : *keysIter)
            keys.push_back(&key);
        ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

        const uint32 keyCount = min(::Engine::kEffectDistributionCurveMaxKeys, static_cast<uint32>(keys.size()));
        desc.tiltOverLifeCurveEnabled = keyCount > 0u;
        desc.tiltOverLifeCurveKeyCount = keyCount;
        desc.tiltOverLifeCurveTimes = Vec4{};
        desc.tiltOverLifeCurveTimesBlock1 = Vec4{};
        desc.tiltOverLifeCurveValuesX = Vec4{};
        desc.tiltOverLifeCurveValuesXBlock1 = Vec4{};
        desc.tiltOverLifeCurveValuesY = Vec4{};
        desc.tiltOverLifeCurveValuesYBlock1 = Vec4{};
        desc.tiltOverLifeCurveArriveTangentsX = Vec4{};
        desc.tiltOverLifeCurveArriveTangentsXBlock1 = Vec4{};
        desc.tiltOverLifeCurveLeaveTangentsX = Vec4{};
        desc.tiltOverLifeCurveLeaveTangentsXBlock1 = Vec4{};
        desc.tiltOverLifeCurveArriveTangentsY = Vec4{};
        desc.tiltOverLifeCurveArriveTangentsYBlock1 = Vec4{};
        desc.tiltOverLifeCurveLeaveTangentsY = Vec4{};
        desc.tiltOverLifeCurveLeaveTangentsYBlock1 = Vec4{};
        desc.tiltOverLifeCurveModes = Vec4{};
        desc.tiltOverLifeCurveModesBlock1 = Vec4{};

        for (uint32 index = 0; index < keyCount; ++index)
            Fill_SpriteTiltOverLifeCurveKey(desc, index, *keys[index]);
    }

    void Fill_RotationOverLifePayload(PointParticleRotationDesc& desc, const json& rotationOverLife)
    {
        const auto rotationOverLifeIter = rotationOverLife.find("rotationOverLife");
        if (rotationOverLifeIter == rotationOverLife.end())
        {
            desc.rotationOverLifeDegrees = Vec2{
                Read_Float(rotationOverLife, "rotationStartDegrees", desc.rotationOverLifeDegrees.x),
                Read_Float(rotationOverLife, "rotationEndDegrees", desc.rotationOverLifeDegrees.y)
            };
            desc.rotationOverLifeCurveKeyCount = 2;
            desc.rotationOverLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
            desc.rotationOverLifeCurveTimesBlock1 = Vec4{};
            desc.rotationOverLifeCurveValues = Vec4{ desc.rotationOverLifeDegrees.x, desc.rotationOverLifeDegrees.y, 0.f, 0.f };
            desc.rotationOverLifeCurveValuesBlock1 = Vec4{};
            desc.rotationOverLifeCurveArriveTangents = Vec4{};
            desc.rotationOverLifeCurveArriveTangentsBlock1 = Vec4{};
            desc.rotationOverLifeCurveLeaveTangents = Vec4{};
            desc.rotationOverLifeCurveLeaveTangentsBlock1 = Vec4{};
            desc.rotationOverLifeCurveModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
            desc.rotationOverLifeCurveModesBlock1 = Vec4{};
            return;
        }

        const float startValue = Evaluate_FloatDistributionMin(*rotationOverLifeIter, 0.f);
        const float endValue = Evaluate_FloatDistributionMax(*rotationOverLifeIter, 0.f);
        desc.rotationOverLifeDegrees = Vec2{ startValue, endValue };
        desc.rotationOverLifeCurveKeyCount = 2;
        desc.rotationOverLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.rotationOverLifeCurveTimesBlock1 = Vec4{};
        desc.rotationOverLifeCurveValues = Vec4{ startValue, endValue, 0.f, 0.f };
        desc.rotationOverLifeCurveValuesBlock1 = Vec4{};
        desc.rotationOverLifeCurveArriveTangents = Vec4{};
        desc.rotationOverLifeCurveArriveTangentsBlock1 = Vec4{};
        desc.rotationOverLifeCurveLeaveTangents = Vec4{};
        desc.rotationOverLifeCurveLeaveTangentsBlock1 = Vec4{};
        desc.rotationOverLifeCurveModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
        desc.rotationOverLifeCurveModesBlock1 = Vec4{};

        const json* payload = Find_DistributionPayload(*rotationOverLifeIter);
        if (payload == nullptr || "ConstantCurve" != Read_String(*rotationOverLifeIter, "mode"))
            return;

        const auto keysIter = payload->find("keys");
        if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
            return;

        vector<const json*> keys{};
        keys.reserve(keysIter->size());
        for (const json& key : *keysIter)
            keys.push_back(&key);
        ranges::sort(
            keys,
            [](const json* lhs, const json* rhs)
            {
                return Read_Float(*lhs, "time") < Read_Float(*rhs, "time");
            }
        );

        const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keys.size()));
        desc.rotationOverLifeCurveKeyCount = keyCount;
        desc.rotationOverLifeCurveTimes = Vec4{};
        desc.rotationOverLifeCurveTimesBlock1 = Vec4{};
        desc.rotationOverLifeCurveValues = Vec4{};
        desc.rotationOverLifeCurveValuesBlock1 = Vec4{};
        desc.rotationOverLifeCurveArriveTangents = Vec4{};
        desc.rotationOverLifeCurveArriveTangentsBlock1 = Vec4{};
        desc.rotationOverLifeCurveLeaveTangents = Vec4{};
        desc.rotationOverLifeCurveLeaveTangentsBlock1 = Vec4{};
        desc.rotationOverLifeCurveModes = Vec4{};
        desc.rotationOverLifeCurveModesBlock1 = Vec4{};

        for (uint32 index = 0; index < keyCount; ++index)
        {
            const json& key = *keys[index];
            Fill_FloatCurveKeyComponent(
                desc.rotationOverLifeCurveTimes,
                desc.rotationOverLifeCurveTimesBlock1,
                desc.rotationOverLifeCurveValues,
                desc.rotationOverLifeCurveValuesBlock1,
                desc.rotationOverLifeCurveArriveTangents,
                desc.rotationOverLifeCurveArriveTangentsBlock1,
                desc.rotationOverLifeCurveLeaveTangents,
                desc.rotationOverLifeCurveLeaveTangentsBlock1,
                desc.rotationOverLifeCurveModes,
                desc.rotationOverLifeCurveModesBlock1,
                index,
                key
            );
        }
    }

    void Fill_RotationRateScaleByLifePayload(PointParticleRotationDesc& desc, const json& rotationRateScaleByLife)
    {
        const auto scaleOverLifeIter = rotationRateScaleByLife.find("scaleOverLife");
        if (scaleOverLifeIter == rotationRateScaleByLife.end())
        {
            desc.rotationRateScaleByLife = Vec2{
                Read_Float(rotationRateScaleByLife, "scaleStart", desc.rotationRateScaleByLife.x),
                Read_Float(rotationRateScaleByLife, "scaleEnd", desc.rotationRateScaleByLife.y)
            };
            desc.rotationRateScaleByLifeCurveKeyCount = 2;
            desc.rotationRateScaleByLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
            desc.rotationRateScaleByLifeCurveTimesBlock1 = Vec4{};
            desc.rotationRateScaleByLifeCurveValues = Vec4{ desc.rotationRateScaleByLife.x, desc.rotationRateScaleByLife.y, 0.f, 0.f };
            desc.rotationRateScaleByLifeCurveValuesBlock1 = Vec4{};
            desc.rotationRateScaleByLifeCurveArriveTangents = Vec4{};
            desc.rotationRateScaleByLifeCurveArriveTangentsBlock1 = Vec4{};
            desc.rotationRateScaleByLifeCurveLeaveTangents = Vec4{};
            desc.rotationRateScaleByLifeCurveLeaveTangentsBlock1 = Vec4{};
            desc.rotationRateScaleByLifeCurveModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
            desc.rotationRateScaleByLifeCurveModesBlock1 = Vec4{};
            return;
        }

        const float startValue = Evaluate_FloatDistributionMin(*scaleOverLifeIter, 1.f);
        const float endValue = Evaluate_FloatDistributionMax(*scaleOverLifeIter, 1.f);
        desc.rotationRateScaleByLife = Vec2{ startValue, endValue };
        desc.rotationRateScaleByLifeCurveKeyCount = 2;
        desc.rotationRateScaleByLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.rotationRateScaleByLifeCurveTimesBlock1 = Vec4{};
        desc.rotationRateScaleByLifeCurveValues = Vec4{ startValue, endValue, 0.f, 0.f };
        desc.rotationRateScaleByLifeCurveValuesBlock1 = Vec4{};
        desc.rotationRateScaleByLifeCurveArriveTangents = Vec4{};
        desc.rotationRateScaleByLifeCurveArriveTangentsBlock1 = Vec4{};
        desc.rotationRateScaleByLifeCurveLeaveTangents = Vec4{};
        desc.rotationRateScaleByLifeCurveLeaveTangentsBlock1 = Vec4{};
        desc.rotationRateScaleByLifeCurveModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
        desc.rotationRateScaleByLifeCurveModesBlock1 = Vec4{};

        const json* payload = Find_DistributionPayload(*scaleOverLifeIter);
        if (payload == nullptr || "ConstantCurve" != Read_String(*scaleOverLifeIter, "mode"))
            return;

        const auto keysIter = payload->find("keys");
        if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
            return;

        vector<const json*> keys{};
        keys.reserve(keysIter->size());
        for (const json& key : *keysIter)
            keys.push_back(&key);
        ranges::sort(
            keys,
            [](const json* lhs, const json* rhs)
            {
                return Read_Float(*lhs, "time") < Read_Float(*rhs, "time");
            }
        );

        const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keys.size()));
        desc.rotationRateScaleByLifeCurveKeyCount = keyCount;
        desc.rotationRateScaleByLifeCurveTimes = Vec4{};
        desc.rotationRateScaleByLifeCurveTimesBlock1 = Vec4{};
        desc.rotationRateScaleByLifeCurveValues = Vec4{};
        desc.rotationRateScaleByLifeCurveValuesBlock1 = Vec4{};
        desc.rotationRateScaleByLifeCurveArriveTangents = Vec4{};
        desc.rotationRateScaleByLifeCurveArriveTangentsBlock1 = Vec4{};
        desc.rotationRateScaleByLifeCurveLeaveTangents = Vec4{};
        desc.rotationRateScaleByLifeCurveLeaveTangentsBlock1 = Vec4{};
        desc.rotationRateScaleByLifeCurveModes = Vec4{};
        desc.rotationRateScaleByLifeCurveModesBlock1 = Vec4{};

        for (uint32 index = 0; index < keyCount; ++index)
        {
            const json& key = *keys[index];
            Fill_FloatCurveKeyComponent(
                desc.rotationRateScaleByLifeCurveTimes,
                desc.rotationRateScaleByLifeCurveTimesBlock1,
                desc.rotationRateScaleByLifeCurveValues,
                desc.rotationRateScaleByLifeCurveValuesBlock1,
                desc.rotationRateScaleByLifeCurveArriveTangents,
                desc.rotationRateScaleByLifeCurveArriveTangentsBlock1,
                desc.rotationRateScaleByLifeCurveLeaveTangents,
                desc.rotationRateScaleByLifeCurveLeaveTangentsBlock1,
                desc.rotationRateScaleByLifeCurveModes,
                desc.rotationRateScaleByLifeCurveModesBlock1,
                index,
                key,
                1.f
            );
        }
    }

    void Fill_VelocityScaleByLifePayload(PointParticleMotionDesc& desc, const json& velocityOverLife)
    {
        const auto scaleOverLifeIter = velocityOverLife.find("scaleOverLife");
        if (scaleOverLifeIter == velocityOverLife.end())
        {
            desc.velocityScaleByLife = Vec2{
                Read_Float(velocityOverLife, "scaleStart", desc.velocityScaleByLife.x),
                Read_Float(velocityOverLife, "scaleEnd", desc.velocityScaleByLife.y)
            };
            desc.velocityScaleByLifeCurveKeyCount = 2;
            desc.velocityScaleByLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
            desc.velocityScaleByLifeCurveTimesBlock1 = Vec4{};
            desc.velocityScaleByLifeCurveValues = Vec4{ desc.velocityScaleByLife.x, desc.velocityScaleByLife.y, 0.f, 0.f };
            desc.velocityScaleByLifeCurveValuesBlock1 = Vec4{};
            desc.velocityScaleByLifeCurveArriveTangents = Vec4{};
            desc.velocityScaleByLifeCurveArriveTangentsBlock1 = Vec4{};
            desc.velocityScaleByLifeCurveLeaveTangents = Vec4{};
            desc.velocityScaleByLifeCurveLeaveTangentsBlock1 = Vec4{};
            desc.velocityScaleByLifeCurveModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
            desc.velocityScaleByLifeCurveModesBlock1 = Vec4{};
            return;
        }

        const float startValue = Evaluate_FloatDistributionMin(*scaleOverLifeIter, 1.f);
        const float endValue = Evaluate_FloatDistributionMax(*scaleOverLifeIter, 1.f);
        desc.velocityScaleByLife = Vec2{ startValue, endValue };
        desc.velocityScaleByLifeCurveKeyCount = 2;
        desc.velocityScaleByLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.velocityScaleByLifeCurveTimesBlock1 = Vec4{};
        desc.velocityScaleByLifeCurveValues = Vec4{ startValue, endValue, 0.f, 0.f };
        desc.velocityScaleByLifeCurveValuesBlock1 = Vec4{};
        desc.velocityScaleByLifeCurveArriveTangents = Vec4{};
        desc.velocityScaleByLifeCurveArriveTangentsBlock1 = Vec4{};
        desc.velocityScaleByLifeCurveLeaveTangents = Vec4{};
        desc.velocityScaleByLifeCurveLeaveTangentsBlock1 = Vec4{};
        desc.velocityScaleByLifeCurveModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
        desc.velocityScaleByLifeCurveModesBlock1 = Vec4{};

        const json* payload = Find_DistributionPayload(*scaleOverLifeIter);
        if (payload == nullptr || "ConstantCurve" != Read_String(*scaleOverLifeIter, "mode"))
            return;

        const auto keysIter = payload->find("keys");
        if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
            return;

        vector<const json*> keys{};
        keys.reserve(keysIter->size());
        for (const json& key : *keysIter)
            keys.push_back(&key);
        ranges::sort(
            keys,
            [](const json* lhs, const json* rhs)
            {
                return Read_Float(*lhs, "time") < Read_Float(*rhs, "time");
            }
        );

        const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keys.size()));
        desc.velocityScaleByLifeCurveKeyCount = keyCount;
        desc.velocityScaleByLifeCurveTimes = Vec4{};
        desc.velocityScaleByLifeCurveTimesBlock1 = Vec4{};
        desc.velocityScaleByLifeCurveValues = Vec4{};
        desc.velocityScaleByLifeCurveValuesBlock1 = Vec4{};
        desc.velocityScaleByLifeCurveArriveTangents = Vec4{};
        desc.velocityScaleByLifeCurveArriveTangentsBlock1 = Vec4{};
        desc.velocityScaleByLifeCurveLeaveTangents = Vec4{};
        desc.velocityScaleByLifeCurveLeaveTangentsBlock1 = Vec4{};
        desc.velocityScaleByLifeCurveModes = Vec4{};
        desc.velocityScaleByLifeCurveModesBlock1 = Vec4{};

        for (uint32 index = 0; index < keyCount; ++index)
        {
            const json& key = *keys[index];
            Fill_FloatCurveKeyComponent(
                desc.velocityScaleByLifeCurveTimes,
                desc.velocityScaleByLifeCurveTimesBlock1,
                desc.velocityScaleByLifeCurveValues,
                desc.velocityScaleByLifeCurveValuesBlock1,
                desc.velocityScaleByLifeCurveArriveTangents,
                desc.velocityScaleByLifeCurveArriveTangentsBlock1,
                desc.velocityScaleByLifeCurveLeaveTangents,
                desc.velocityScaleByLifeCurveLeaveTangentsBlock1,
                desc.velocityScaleByLifeCurveModes,
                desc.velocityScaleByLifeCurveModesBlock1,
                index,
                key,
                1.f
            );
        }
    }

    void Fill_VelocityScaleByLifeChannelPayload(PointParticleFloatCurveRuntimeDesc& desc, const json& velocityOverLife)
    {
        const auto scaleOverLifeIter = velocityOverLife.find("scaleOverLife");
        float startValue{ 1.f };
        float endValue{ 1.f };
        if (scaleOverLifeIter == velocityOverLife.end())
        {
            startValue = Read_Float(velocityOverLife, "scaleStart", startValue);
            endValue = Read_Float(velocityOverLife, "scaleEnd", endValue);
        }
        else
        {
            startValue = Evaluate_FloatDistributionMin(*scaleOverLifeIter, startValue);
            endValue = Evaluate_FloatDistributionMax(*scaleOverLifeIter, endValue);
        }

        desc.enabled = true;
        desc.curveKeyCount = 2u;
        desc.curveKeyTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.curveKeyTimesBlock1 = Vec4{};
        desc.curveKeyValues = Vec4{ startValue, endValue, 0.f, 0.f };
        desc.curveKeyValuesBlock1 = Vec4{};
        desc.curveKeyArriveTangents = Vec4{};
        desc.curveKeyArriveTangentsBlock1 = Vec4{};
        desc.curveKeyLeaveTangents = Vec4{};
        desc.curveKeyLeaveTangentsBlock1 = Vec4{};
        desc.curveKeyModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
        desc.curveKeyModesBlock1 = Vec4{};

        if (scaleOverLifeIter == velocityOverLife.end())
            return;

        const json* payload = Find_DistributionPayload(*scaleOverLifeIter);
        if (payload == nullptr || "ConstantCurve" != Read_String(*scaleOverLifeIter, "mode"))
            return;

        const auto keysIter = payload->find("keys");
        if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
            return;

        vector<const json*> keys{};
        keys.reserve(keysIter->size());
        for (const json& key : *keysIter)
            keys.push_back(&key);
        ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

        const uint32 keyCount = min(::Engine::kEffectDistributionCurveMaxKeys, static_cast<uint32>(keys.size()));
        if (keyCount == 0u)
            return;

        desc.curveKeyCount = keyCount;
        desc.curveKeyTimes = Vec4{};
        desc.curveKeyTimesBlock1 = Vec4{};
        desc.curveKeyValues = Vec4{};
        desc.curveKeyValuesBlock1 = Vec4{};
        desc.curveKeyArriveTangents = Vec4{};
        desc.curveKeyArriveTangentsBlock1 = Vec4{};
        desc.curveKeyLeaveTangents = Vec4{};
        desc.curveKeyLeaveTangentsBlock1 = Vec4{};
        desc.curveKeyModes = Vec4{};
        desc.curveKeyModesBlock1 = Vec4{};
        for (uint32 index = 0; index < keyCount; ++index)
        {
            const json& key = *keys[index];
            Fill_FloatCurveKeyComponent(
                desc.curveKeyTimes,
                desc.curveKeyTimesBlock1,
                desc.curveKeyValues,
                desc.curveKeyValuesBlock1,
                desc.curveKeyArriveTangents,
                desc.curveKeyArriveTangentsBlock1,
                desc.curveKeyLeaveTangents,
                desc.curveKeyLeaveTangentsBlock1,
                desc.curveKeyModes,
                desc.curveKeyModesBlock1,
                index,
                key,
                1.f
            );
        }
    }

    void Fill_VelocityConePayload(PointParticleMotionDesc& desc, const json& velocityCone)
    {
        desc.velocityConeEnabled = true;
        desc.velocityConeInWorldSpace = Read_Bool(velocityCone, "inWorldSpace", desc.velocityConeInWorldSpace);
        desc.velocityConeAngleDegrees = clamp(Read_Float(velocityCone, "angleDegrees", desc.velocityConeAngleDegrees), 0.f, 180.f);

        const auto axisIter = velocityCone.find("axis");
        if (axisIter != velocityCone.end())
            Read_Vec3(*axisIter, desc.velocityConeAxis);

        const auto speedIter = velocityCone.find("speed");
        if (speedIter != velocityCone.end())
        {
            desc.velocityConeSpeed = Vec2{
                Evaluate_FloatDistributionMin(*speedIter, desc.velocityConeSpeed.x),
                Evaluate_FloatDistributionMax(*speedIter, desc.velocityConeSpeed.y)
            };
        }
    }

    void Fill_AccelerationCurvePayload(PointParticleMotionDesc& desc, const json& distribution)
    {
        desc.accelerationCurveEnabled = false;
        desc.accelerationCurveKeyCount = 2;
        desc.accelerationCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.accelerationCurveTimesBlock1 = Vec4{};
        desc.accelerationCurveValuesX = Vec4{ desc.accelerationMin.x, desc.accelerationMax.x, 0.f, 0.f };
        desc.accelerationCurveValuesXBlock1 = Vec4{};
        desc.accelerationCurveValuesY = Vec4{ desc.accelerationMin.y, desc.accelerationMax.y, 0.f, 0.f };
        desc.accelerationCurveValuesYBlock1 = Vec4{};
        desc.accelerationCurveValuesZ = Vec4{ desc.accelerationMin.z, desc.accelerationMax.z, 0.f, 0.f };
        desc.accelerationCurveValuesZBlock1 = Vec4{};
        desc.accelerationCurveArriveTangentsX = Vec4{};
        desc.accelerationCurveArriveTangentsXBlock1 = Vec4{};
        desc.accelerationCurveLeaveTangentsX = Vec4{};
        desc.accelerationCurveLeaveTangentsXBlock1 = Vec4{};
        desc.accelerationCurveArriveTangentsY = Vec4{};
        desc.accelerationCurveArriveTangentsYBlock1 = Vec4{};
        desc.accelerationCurveLeaveTangentsY = Vec4{};
        desc.accelerationCurveLeaveTangentsYBlock1 = Vec4{};
        desc.accelerationCurveArriveTangentsZ = Vec4{};
        desc.accelerationCurveArriveTangentsZBlock1 = Vec4{};
        desc.accelerationCurveLeaveTangentsZ = Vec4{};
        desc.accelerationCurveLeaveTangentsZBlock1 = Vec4{};
        desc.accelerationCurveModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
        desc.accelerationCurveModesBlock1 = Vec4{};

        const json* payload = Find_DistributionPayload(distribution);
        if (payload == nullptr || "ConstantCurve" != Read_String(distribution, "mode"))
            return;

        const auto keysIter = payload->find("keys");
        if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
            return;

        vector<const json*> keys{};
        keys.reserve(keysIter->size());
        for (const json& key : *keysIter)
            keys.push_back(&key);
        ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

        const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keys.size()));
        desc.accelerationCurveEnabled = true;
        desc.accelerationCurveKeyCount = keyCount;
        desc.accelerationCurveTimes = Vec4{};
        desc.accelerationCurveTimesBlock1 = Vec4{};
        desc.accelerationCurveValuesX = Vec4{};
        desc.accelerationCurveValuesXBlock1 = Vec4{};
        desc.accelerationCurveValuesY = Vec4{};
        desc.accelerationCurveValuesYBlock1 = Vec4{};
        desc.accelerationCurveValuesZ = Vec4{};
        desc.accelerationCurveValuesZBlock1 = Vec4{};
        desc.accelerationCurveArriveTangentsX = Vec4{};
        desc.accelerationCurveArriveTangentsXBlock1 = Vec4{};
        desc.accelerationCurveLeaveTangentsX = Vec4{};
        desc.accelerationCurveLeaveTangentsXBlock1 = Vec4{};
        desc.accelerationCurveArriveTangentsY = Vec4{};
        desc.accelerationCurveArriveTangentsYBlock1 = Vec4{};
        desc.accelerationCurveLeaveTangentsY = Vec4{};
        desc.accelerationCurveLeaveTangentsYBlock1 = Vec4{};
        desc.accelerationCurveArriveTangentsZ = Vec4{};
        desc.accelerationCurveArriveTangentsZBlock1 = Vec4{};
        desc.accelerationCurveLeaveTangentsZ = Vec4{};
        desc.accelerationCurveLeaveTangentsZBlock1 = Vec4{};
        desc.accelerationCurveModes = Vec4{};
        desc.accelerationCurveModesBlock1 = Vec4{};

        for (uint32 index = 0; index < keyCount; ++index)
        {
            const json& key = *keys[index];
            Vec3 value{};
            Vec3 arriveTangent{};
            Vec3 leaveTangent{};
            if (const auto valueIter = key.find("value"); valueIter != key.end())
                Read_Vec3(*valueIter, value);
            if (const auto arriveIter = key.find("arriveTangent"); arriveIter != key.end())
                Read_Vec3(*arriveIter, arriveTangent);
            if (const auto leaveIter = key.find("leaveTangent"); leaveIter != key.end())
                Read_Vec3(*leaveIter, leaveTangent);

            Set_CurveComponent(desc.accelerationCurveTimes, desc.accelerationCurveTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
            Set_CurveComponent(desc.accelerationCurveValuesX, desc.accelerationCurveValuesXBlock1, index, value.x);
            Set_CurveComponent(desc.accelerationCurveValuesY, desc.accelerationCurveValuesYBlock1, index, value.y);
            Set_CurveComponent(desc.accelerationCurveValuesZ, desc.accelerationCurveValuesZBlock1, index, value.z);
            Set_CurveComponent(desc.accelerationCurveArriveTangentsX, desc.accelerationCurveArriveTangentsXBlock1, index, arriveTangent.x);
            Set_CurveComponent(desc.accelerationCurveLeaveTangentsX, desc.accelerationCurveLeaveTangentsXBlock1, index, leaveTangent.x);
            Set_CurveComponent(desc.accelerationCurveArriveTangentsY, desc.accelerationCurveArriveTangentsYBlock1, index, arriveTangent.y);
            Set_CurveComponent(desc.accelerationCurveLeaveTangentsY, desc.accelerationCurveLeaveTangentsYBlock1, index, leaveTangent.y);
            Set_CurveComponent(desc.accelerationCurveArriveTangentsZ, desc.accelerationCurveArriveTangentsZBlock1, index, arriveTangent.z);
            Set_CurveComponent(desc.accelerationCurveLeaveTangentsZ, desc.accelerationCurveLeaveTangentsZBlock1, index, leaveTangent.z);
            Set_CurveComponent(desc.accelerationCurveModes, desc.accelerationCurveModesBlock1, index, Resolve_CurveInterpolationMode(key));
        }
    }

    void Fill_OrbitOverLifePayload(EffectOrbitOverLifeRuntimeDesc& desc, const json& orbitOverLife)
    {
        desc.enabled = true;
        Read_Enum(orbitOverLife, "plane", desc.plane);

        auto fill_float_curve = [](
            Vec2& range,
            uint32& keyCount,
            Vec4& times,
            Vec4& timesBlock1,
            Vec4& values,
            Vec4& valuesBlock1,
            Vec4& arriveTangents,
            Vec4& arriveTangentsBlock1,
            Vec4& leaveTangents,
            Vec4& leaveTangentsBlock1,
            Vec4& modes,
            Vec4& modesBlock1,
            const json& distribution,
            float fallbackValue)
        {
            const float startValue = Evaluate_FloatDistributionMin(distribution, fallbackValue);
            const float endValue = Evaluate_FloatDistributionMax(distribution, fallbackValue);
            range = Vec2{ startValue, endValue };
            keyCount = 2;
            times = Vec4{ 0.f, 1.f, 0.f, 0.f };
            timesBlock1 = Vec4{};
            values = Vec4{ startValue, endValue, 0.f, 0.f };
            valuesBlock1 = Vec4{};
            arriveTangents = Vec4{};
            arriveTangentsBlock1 = Vec4{};
            leaveTangents = Vec4{};
            leaveTangentsBlock1 = Vec4{};
            modes = Vec4{ 1.f, 1.f, 0.f, 0.f };
            modesBlock1 = Vec4{};

            const json* payload = Find_DistributionPayload(distribution);
            if (payload == nullptr || "ConstantCurve" != Read_String(distribution, "mode"))
                return;

            const auto keysIter = payload->find("keys");
            if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
                return;

            vector<const json*> keys{};
            keys.reserve(keysIter->size());
            for (const json& key : *keysIter)
                keys.push_back(&key);
            ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

            const uint32 clampedKeyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keys.size()));
            keyCount = clampedKeyCount;
            times = Vec4{};
            timesBlock1 = Vec4{};
            values = Vec4{};
            valuesBlock1 = Vec4{};
            arriveTangents = Vec4{};
            arriveTangentsBlock1 = Vec4{};
            leaveTangents = Vec4{};
            leaveTangentsBlock1 = Vec4{};
            modes = Vec4{};
            modesBlock1 = Vec4{};
            for (uint32 index = 0; index < clampedKeyCount; ++index)
            {
                const json& key = *keys[index];
                Fill_FloatCurveKeyComponent(
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
                    index,
                    key,
                    fallbackValue
                );
            }
        };

        if (const auto angleIter = orbitOverLife.find("angleDegreesOverLife"); angleIter != orbitOverLife.end())
        {
            fill_float_curve(
                desc.angleDegreesOverLife,
                desc.angleCurveKeyCount,
                desc.angleCurveTimes,
                desc.angleCurveTimesBlock1,
                desc.angleCurveValues,
                desc.angleCurveValuesBlock1,
                desc.angleCurveArriveTangents,
                desc.angleCurveArriveTangentsBlock1,
                desc.angleCurveLeaveTangents,
                desc.angleCurveLeaveTangentsBlock1,
                desc.angleCurveModes,
                desc.angleCurveModesBlock1,
                *angleIter,
                0.f
            );
        }

        if (const auto radiusIter = orbitOverLife.find("radiusScaleOverLife"); radiusIter != orbitOverLife.end())
        {
            fill_float_curve(
                desc.radiusScaleOverLife,
                desc.radiusScaleCurveKeyCount,
                desc.radiusScaleCurveTimes,
                desc.radiusScaleCurveTimesBlock1,
                desc.radiusScaleCurveValues,
                desc.radiusScaleCurveValuesBlock1,
                desc.radiusScaleCurveArriveTangents,
                desc.radiusScaleCurveArriveTangentsBlock1,
                desc.radiusScaleCurveLeaveTangents,
                desc.radiusScaleCurveLeaveTangentsBlock1,
                desc.radiusScaleCurveModes,
                desc.radiusScaleCurveModesBlock1,
                *radiusIter,
                1.f
            );
        }
    }

    namespace
    {
        Vec2 Read_LegacyFloatRange(const json& node, const Vec2& fallbackValue)
        {
            Vec2 range = fallbackValue;
            Read_Vec2(node, range);
            return Vec2{ min(range.x, range.y), max(range.x, range.y) };
        }

        Vec2 Read_FloatDistributionRangeOrLegacy(
            const json& root,
            const char* distributionKey,
            const char* legacyRangeKey,
            const Vec2& fallbackValue)
        {
            if (const auto distributionIter = root.find(distributionKey);
                distributionIter != root.end() && distributionIter->is_object())
            {
                const float minValue = Evaluate_FloatDistributionMin(*distributionIter, fallbackValue.x);
                const float maxValue = Evaluate_FloatDistributionMax(*distributionIter, fallbackValue.y);
                return Vec2{ min(minValue, maxValue), max(minValue, maxValue) };
            }

            if (const auto legacyIter = root.find(legacyRangeKey); legacyIter != root.end())
                return Read_LegacyFloatRange(*legacyIter, fallbackValue);

            return fallbackValue;
        }
    }

    void Fill_PlaneRadialLocationPayload(PointParticlePlaneRadialLocationDesc& desc, const json& planeRadialLocation)
    {
        desc.enabled = true;
        Read_Enum(planeRadialLocation, "plane", desc.plane);
        Read_Enum(planeRadialLocation, "shape", desc.shape);
        Read_Enum(planeRadialLocation, "placementMode", desc.placementMode);

        if (const auto offsetIter = planeRadialLocation.find("offset"); offsetIter != planeRadialLocation.end())
            Read_Vec3(*offsetIter, desc.offset);

        desc.uRange = Read_FloatDistributionRangeOrLegacy(planeRadialLocation, "uDistribution", "rangeU", desc.uRange);
        if (const auto distributionIter = planeRadialLocation.find("uDistribution");
            distributionIter != planeRadialLocation.end() && distributionIter->is_object())
            desc.uSeed = Read_DistributionRandomSeedDesc(*distributionIter, &planeRadialLocation);
        desc.vRange = Read_FloatDistributionRangeOrLegacy(planeRadialLocation, "vDistribution", "rangeV", desc.vRange);
        if (const auto distributionIter = planeRadialLocation.find("vDistribution");
            distributionIter != planeRadialLocation.end() && distributionIter->is_object())
            desc.vSeed = Read_DistributionRandomSeedDesc(*distributionIter, &planeRadialLocation);

        const Vec2 radiusRange = Read_FloatDistributionRangeOrLegacy(planeRadialLocation, "radiusDistribution", "radiusRange", desc.radiusRange);
        desc.radiusRange = Vec2{ max(0.f, radiusRange.x), max(0.f, radiusRange.y) };
        if (const auto distributionIter = planeRadialLocation.find("radiusDistribution");
            distributionIter != planeRadialLocation.end() && distributionIter->is_object())
            desc.radiusSeed = Read_DistributionRandomSeedDesc(*distributionIter, &planeRadialLocation);
        desc.angleDegreesRange = Read_FloatDistributionRangeOrLegacy(planeRadialLocation, "angleDegreesDistribution", "angleDegreesRange", desc.angleDegreesRange);
        if (const auto distributionIter = planeRadialLocation.find("angleDegreesDistribution");
            distributionIter != planeRadialLocation.end() && distributionIter->is_object())
            desc.angleDegreesSeed = Read_DistributionRandomSeedDesc(*distributionIter, &planeRadialLocation);
        desc.thickness = max(0.f, Read_Float(planeRadialLocation, "thickness", desc.thickness));
    }

    void Fill_CylinderLocationPayload(PointParticleCylinderLocationDesc& desc, const json& cylinderLocation)
    {
        desc.enabled = true;
        Read_Enum(cylinderLocation, "axis", desc.axis);
        Read_Enum(cylinderLocation, "spawnMode", desc.mode);
        Read_Enum(cylinderLocation, "placementMode", desc.placementMode);

        if (const auto offsetIter = cylinderLocation.find("offset"); offsetIter != cylinderLocation.end())
            Read_Vec3(*offsetIter, desc.offset);

        const Vec2 radiusRange = Read_FloatDistributionRangeOrLegacy(cylinderLocation, "radiusDistribution", "radiusRange", desc.radiusRange);
        desc.radiusRange = Vec2{ max(0.f, radiusRange.x), max(0.f, radiusRange.y) };
        const Vec2 heightRange = Read_FloatDistributionRangeOrLegacy(cylinderLocation, "heightDistribution", "heightRange", desc.heightRange);
        desc.heightRange = Vec2{ min(heightRange.x, heightRange.y), max(heightRange.x, heightRange.y) };
        const Vec2 angleDegreesRange = Read_FloatDistributionRangeOrLegacy(cylinderLocation, "angleDegreesDistribution", "angleDegreesRange", desc.angleDegreesRange);
        desc.angleDegreesRange = Vec2{ min(angleDegreesRange.x, angleDegreesRange.y), max(angleDegreesRange.x, angleDegreesRange.y) };

        if (const auto distributionIter = cylinderLocation.find("radiusDistribution");
            distributionIter != cylinderLocation.end() && distributionIter->is_object())
            desc.radiusSeed = Read_DistributionRandomSeedDesc(*distributionIter, &cylinderLocation);
        if (const auto distributionIter = cylinderLocation.find("heightDistribution");
            distributionIter != cylinderLocation.end() && distributionIter->is_object())
            desc.heightSeed = Read_DistributionRandomSeedDesc(*distributionIter, &cylinderLocation);
        if (const auto distributionIter = cylinderLocation.find("angleDegreesDistribution");
            distributionIter != cylinderLocation.end() && distributionIter->is_object())
            desc.angleDegreesSeed = Read_DistributionRandomSeedDesc(*distributionIter, &cylinderLocation);
    }

    void Fill_SphereRadialOrientationPayload(
        PointParticleSphereRadialOrientationDesc& desc,
        const json& sphereRadialOrientation)
    {
        desc.enabled = true;
        Read_Enum(sphereRadialOrientation, "orientationMode", desc.orientationMode);
        Read_Enum(sphereRadialOrientation, "meshForwardAxis", desc.meshForwardAxis);
        Read_Enum(sphereRadialOrientation, "meshUpAxis", desc.meshUpAxis);
        desc.tiltDegrees = Read_Float(sphereRadialOrientation, "tiltDegrees", desc.tiltDegrees);
        desc.rollOffsetDegrees = Read_Float(sphereRadialOrientation, "rollOffsetDegrees", desc.rollOffsetDegrees);
    }

    void Fill_PlaneRadialOrientationPayload(
        PointParticlePlaneRadialOrientationDesc& desc,
        const json& planeRadialOrientation)
    {
        desc.enabled = true;
        Read_Enum(planeRadialOrientation, "targetKind", desc.targetKind);
        Read_Enum(planeRadialOrientation, "orientationMode", desc.orientationMode);
        Read_Enum(planeRadialOrientation, "meshForwardAxis", desc.meshForwardAxis);
        Read_Enum(planeRadialOrientation, "meshUpAxis", desc.meshUpAxis);
        desc.tiltDegrees = Read_Float(planeRadialOrientation, "tiltDegrees", desc.tiltDegrees);
        desc.rollOffsetDegrees = Read_Float(planeRadialOrientation, "rollOffsetDegrees", desc.rollOffsetDegrees);
    }

    void Fill_CylinderOrientationPayload(
        PointParticleCylinderOrientationDesc& desc,
        const json& cylinderOrientation)
    {
        desc.enabled = true;
        Read_Enum(cylinderOrientation, "targetKind", desc.targetKind);
        Read_Enum(cylinderOrientation, "orientationMode", desc.orientationMode);
        desc.followOrbitOverLife = Read_Bool(cylinderOrientation, "followOrbitOverLife", desc.followOrbitOverLife);
        Read_Enum(cylinderOrientation, "meshForwardAxis", desc.meshForwardAxis);
        Read_Enum(cylinderOrientation, "meshUpAxis", desc.meshUpAxis);
        desc.tiltDegrees = Read_Float(cylinderOrientation, "tiltDegrees", desc.tiltDegrees);
        desc.rollOffsetDegrees = Read_Float(cylinderOrientation, "rollOffsetDegrees", desc.rollOffsetDegrees);
    }

    void Fill_MeshVector3CurvePayload(
        MeshVector3CurveRuntimeDesc& desc,
        const json& distribution,
        const Vec3& fallbackValue)
    {
        desc.enabled = true;
        const Vec3 startValue = Evaluate_Vector3DistributionMin(distribution, fallbackValue);
        const Vec3 endValue = Evaluate_Vector3DistributionMax(distribution, fallbackValue);
        desc.start = startValue;
        desc.end = endValue;
        desc.curveKeyCount = 2;
        desc.curveKeyTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.curveKeyTimesBlock1 = Vec4{};
        desc.curveKeyValuesX = Vec4{ startValue.x, endValue.x, 0.f, 0.f };
        desc.curveKeyValuesXBlock1 = Vec4{};
        desc.curveKeyValuesY = Vec4{ startValue.y, endValue.y, 0.f, 0.f };
        desc.curveKeyValuesYBlock1 = Vec4{};
        desc.curveKeyValuesZ = Vec4{ startValue.z, endValue.z, 0.f, 0.f };
        desc.curveKeyValuesZBlock1 = Vec4{};
        desc.curveKeyArriveTangentsX = Vec4{};
        desc.curveKeyArriveTangentsXBlock1 = Vec4{};
        desc.curveKeyLeaveTangentsX = Vec4{};
        desc.curveKeyLeaveTangentsXBlock1 = Vec4{};
        desc.curveKeyArriveTangentsY = Vec4{};
        desc.curveKeyArriveTangentsYBlock1 = Vec4{};
        desc.curveKeyLeaveTangentsY = Vec4{};
        desc.curveKeyLeaveTangentsYBlock1 = Vec4{};
        desc.curveKeyArriveTangentsZ = Vec4{};
        desc.curveKeyArriveTangentsZBlock1 = Vec4{};
        desc.curveKeyLeaveTangentsZ = Vec4{};
        desc.curveKeyLeaveTangentsZBlock1 = Vec4{};
        desc.curveKeyModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
        desc.curveKeyModesBlock1 = Vec4{};

        const json* payload = Find_DistributionPayload(distribution);
        if (payload == nullptr || "ConstantCurve" != Read_String(distribution, "mode"))
            return;

        const auto keysIter = payload->find("keys");
        if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
            return;

        vector<const json*> keys{};
        keys.reserve(keysIter->size());
        for (const json& key : *keysIter)
            keys.push_back(&key);
        ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

        const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keys.size()));
        desc.curveKeyCount = keyCount;
        desc.curveKeyTimes = Vec4{};
        desc.curveKeyTimesBlock1 = Vec4{};
        desc.curveKeyValuesX = Vec4{};
        desc.curveKeyValuesXBlock1 = Vec4{};
        desc.curveKeyValuesY = Vec4{};
        desc.curveKeyValuesYBlock1 = Vec4{};
        desc.curveKeyValuesZ = Vec4{};
        desc.curveKeyValuesZBlock1 = Vec4{};
        desc.curveKeyArriveTangentsX = Vec4{};
        desc.curveKeyArriveTangentsXBlock1 = Vec4{};
        desc.curveKeyLeaveTangentsX = Vec4{};
        desc.curveKeyLeaveTangentsXBlock1 = Vec4{};
        desc.curveKeyArriveTangentsY = Vec4{};
        desc.curveKeyArriveTangentsYBlock1 = Vec4{};
        desc.curveKeyLeaveTangentsY = Vec4{};
        desc.curveKeyLeaveTangentsYBlock1 = Vec4{};
        desc.curveKeyArriveTangentsZ = Vec4{};
        desc.curveKeyArriveTangentsZBlock1 = Vec4{};
        desc.curveKeyLeaveTangentsZ = Vec4{};
        desc.curveKeyLeaveTangentsZBlock1 = Vec4{};
        desc.curveKeyModes = Vec4{};
        desc.curveKeyModesBlock1 = Vec4{};

        for (uint32 index = 0; index < keyCount; ++index)
        {
            const json& key = *keys[index];
            Vec3 value = fallbackValue;
            Vec3 arriveTangent{};
            Vec3 leaveTangent{};
            if (const auto valueIter = key.find("value"); valueIter != key.end())
                Read_Vec3(*valueIter, value);
            if (const auto arriveIter = key.find("arriveTangent"); arriveIter != key.end())
                Read_Vec3(*arriveIter, arriveTangent);
            if (const auto leaveIter = key.find("leaveTangent"); leaveIter != key.end())
                Read_Vec3(*leaveIter, leaveTangent);

            Set_CurveComponent(desc.curveKeyTimes, desc.curveKeyTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
            Set_CurveComponent(desc.curveKeyValuesX, desc.curveKeyValuesXBlock1, index, value.x);
            Set_CurveComponent(desc.curveKeyValuesY, desc.curveKeyValuesYBlock1, index, value.y);
            Set_CurveComponent(desc.curveKeyValuesZ, desc.curveKeyValuesZBlock1, index, value.z);
            Set_CurveComponent(desc.curveKeyArriveTangentsX, desc.curveKeyArriveTangentsXBlock1, index, arriveTangent.x);
            Set_CurveComponent(desc.curveKeyLeaveTangentsX, desc.curveKeyLeaveTangentsXBlock1, index, leaveTangent.x);
            Set_CurveComponent(desc.curveKeyArriveTangentsY, desc.curveKeyArriveTangentsYBlock1, index, arriveTangent.y);
            Set_CurveComponent(desc.curveKeyLeaveTangentsY, desc.curveKeyLeaveTangentsYBlock1, index, leaveTangent.y);
            Set_CurveComponent(desc.curveKeyArriveTangentsZ, desc.curveKeyArriveTangentsZBlock1, index, arriveTangent.z);
            Set_CurveComponent(desc.curveKeyLeaveTangentsZ, desc.curveKeyLeaveTangentsZBlock1, index, leaveTangent.z);
            Set_CurveComponent(desc.curveKeyModes, desc.curveKeyModesBlock1, index, Resolve_CurveInterpolationMode(key));
        }
    }
}

NS_END
