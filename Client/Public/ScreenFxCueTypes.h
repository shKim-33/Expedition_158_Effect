#pragma once

NS_BEGIN(Client)

enum class ScreenFxShapeMode : int32
{
    FullScreen,
    Circle
};

enum class ScreenFxCueType : int32
{
    Flash,
    WorldDesaturate
};

enum class ScreenFxExclusionMode : int32
{
    None,
    MarkedObjects
};

NS_END
