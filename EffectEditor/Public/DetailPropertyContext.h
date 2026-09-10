#pragma once

#include "EffectAuthoring_Types.h"
#include "EffectAuthoringValueRange.h"

NS_BEGIN(EffectEditor)

class DetailPropertyContext final
{
public:
    DetailPropertyContext();
    ~DetailPropertyContext();

public:
    bool Begin_PropertyTable(const char* id);
    void End_PropertyTable();

    void Draw_PropertyLabel(const char* label, const char* tooltip = nullptr);
    bool Draw_ResetButton(bool canReset);
    bool Draw_StringProperty(const char* label, string& value, const string& defaultValue, const char* tooltip = nullptr);
    bool Draw_BoolProperty(const char* label, bool& value, bool defaultValue, const char* tooltip = nullptr);
    bool Draw_FloatProperty(const char* label, float& value, float defaultValue, float speed = 0.01f, const char* tooltip = nullptr, const AuthoringValueRange* range = nullptr);
    bool Draw_ColorProperty(const char* label, Color& value, const Color& defaultValue, const char* tooltip = nullptr);
    bool Draw_ColorRgbProperty(const char* label, Color& value, const Color& defaultValue, const char* tooltip = nullptr);
    bool Draw_FloatDistributionGroup(
        const char* label,
        FloatDistributionData& value,
        const FloatDistributionData& defaultValue,
        float speed = 0.01f,
        bool allowUniform = true,
        bool allowConstantCurve = true,
        const AuthoringValueRange* range = nullptr,
        const char* tooltip = nullptr);
    bool Draw_Vector2DistributionGroup(
        const char* label,
        Vector2DistributionData& value,
        const Vector2DistributionData& defaultValue,
        float speed = 0.01f,
        bool allowConstantCurve = true,
        const AuthoringValueRange* range = nullptr,
        const char* tooltip = nullptr);
    bool Draw_Vector3DistributionGroup(
        const char* label,
        Vector3DistributionData& value,
        const Vector3DistributionData& defaultValue,
        float speed = 0.01f,
        bool allowConstantCurve = true,
        bool showConstantCurveAsUnimplemented = false,
        const AuthoringValueRange* range = nullptr,
        const char* tooltip = nullptr);
    bool Draw_ColorDistributionGroup(const char* label, ColorDistributionData& value, const ColorDistributionData& defaultValue);
    bool Draw_ColorRgbDistributionGroup(const char* label, ColorDistributionData& value, const ColorDistributionData& defaultValue);
    bool Draw_BurstListProperty(vector<SpawnModuleData::ParticleBurstData>& burstList, const vector<SpawnModuleData::ParticleBurstData>& defaultValue);
    bool Draw_ScreenAlignmentProperty(const char* label, EmitterScreenAlignment& value, EmitterScreenAlignment defaultValue);
    bool Draw_SourceHistorySpriteTrailScreenAlignmentProperty(const char* label, EmitterScreenAlignment& value, EmitterScreenAlignment defaultValue);
    bool Draw_DirectionalAlignmentModeProperty(const char* label, EmitterDirectionalAlignmentMode& outValue, EmitterDirectionalAlignmentMode defaultValue);
    bool Draw_SpriteTextureAxisProperty(
        const char* label,
        EmitterSpriteTextureAxis& outValue,
        EmitterSpriteTextureAxis defaultValue,
        const char* textureXAxisTooltip = nullptr,
        const char* textureYAxisTooltip = nullptr);
    void Draw_ScreenAlignmentImplementationNote(EmitterScreenAlignment value);
    bool Draw_SortModeProperty(const char* label, EmitterSortMode& value, EmitterSortMode defaultValue);
    bool Draw_UintProperty(const char* label, uint32& value, uint32 defaultValue, const char* tooltip = nullptr);
    bool Draw_Vec2Property(
        const char* label,
        Vec2& value,
        const Vec2& defaultValue,
        float speed = 0.01f,
        const AuthoringValueRange* range = nullptr,
        const char* tooltip = nullptr);
    bool Draw_Vec3Property(
        const char* label,
        Vec3& value,
        const Vec3& defaultValue,
        float speed = 0.01f,
        const AuthoringValueRange* range = nullptr,
        const char* tooltip = nullptr);

private: //## Types::Drawers
    class BasicDetailPropertyDrawer;
    class DistributionDetailPropertyDrawer;

private: //## Data::Drawers
    Unique<BasicDetailPropertyDrawer> _basicDrawer{};
    Unique<DistributionDetailPropertyDrawer> _distributionDrawer{};
};

NS_END
