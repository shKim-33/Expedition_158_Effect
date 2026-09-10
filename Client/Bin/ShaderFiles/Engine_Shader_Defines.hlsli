// Engine 원본 경로: Engine/Bin/ShaderFiles/Engine_Shader_Defines.hlsli
// Client 복사본 경로: Client/Bin/ShaderFiles/Engine_Shader_Defines.hlsli - 먼저 이 원본의 의도를 확인하세요.
#ifndef ENGINE_SHADER_DEFINES_HLSLI
#define ENGINE_SHADER_DEFINES_HLSLI

#define EPSILON 0.0001f
#define EPSILON_3 0.001f

// C++ enum 순서
// BaseColor    = 0
// Normal       = 1
// Emissive     = 2
// OpacityMask  = 3
// ORM          = 4
// HairDepth    = 5
// HairID       = 6
// IrisHeight   = 7
// IrisMask     = 8
// END          = 9

// Material Texture Slot
#define MATERIAL_SLOT_BASECOLOR      0
#define MATERIAL_SLOT_NORMAL         1
#define MATERIAL_SLOT_EMISSIVE       2
#define MATERIAL_SLOT_OPACITY_MASK   3
#define MATERIAL_SLOT_ORM            4
#define MATERIAL_SLOT_HAIR_DEPTH     5 
#define MATERIAL_SLOT_HAIR_ID        6 
#define MATERIAL_SLOT_IRIS_HEIGHT    7
#define MATERIAL_SLOT_IRIS_MASK      8
#define MATERIAL_SLOT_ORM2           9
#define MATERIAL_SLOT_ORM3           10
#define MATERIAL_SLOT_END            11

// Material Flags
#define MATERIAL_FLAG_BASECOLOR      (1u << MATERIAL_SLOT_BASECOLOR)
#define MATERIAL_FLAG_NORMAL         (1u << MATERIAL_SLOT_NORMAL)
#define MATERIAL_FLAG_EMISSIVE       (1u << MATERIAL_SLOT_EMISSIVE)
#define MATERIAL_FLAG_OPACITY_MASK   (1u << MATERIAL_SLOT_OPACITY_MASK)
#define MATERIAL_FLAG_ORM            (1u << MATERIAL_SLOT_ORM)
#define MATERIAL_FLAG_HAIR_DEPTH     (1u << MATERIAL_SLOT_HAIR_DEPTH)
#define MATERIAL_FLAG_HAIR_ID        (1u << MATERIAL_SLOT_HAIR_ID)
#define MATERIAL_FLAG_IRIS_HEIGHT    (1u << MATERIAL_SLOT_IRIS_HEIGHT)
#define MATERIAL_FLAG_IRIS_MASK      (1u << MATERIAL_SLOT_IRIS_MASK)
#define MATERIAL_FLAG_ORM2           (1u << MATERIAL_SLOT_ORM2)
#define MATERIAL_FLAG_ORM3           (1u << MATERIAL_SLOT_ORM3)

#define MATERIAL_CHANNEL_CONST0      0u
#define MATERIAL_CHANNEL_CONST1      1u
#define MATERIAL_CHANNEL_R           2u
#define MATERIAL_CHANNEL_G           3u
#define MATERIAL_CHANNEL_B           4u
#define MATERIAL_CHANNEL_A           5u
#define MATERIAL_CHANNEL_MAX_RGB     6u
#define MATERIAL_CHANNEL_LUMA        7u

// Feature Texture Slot
#define FEATURE_TEX_DISSOLVE_NOISE   0
#define FEATURE_TEX_DISSOLVE_MASK    1
#define FEATURE_TEX_DISTORTION_FLOW  2
#define FEATURE_TEX_DISTORTION_NOISE 3
#define FEATURE_TEX_END              4

// Draw Feature Flags
#define FEATURE_FLAG_DISSOLVE        (1u << 0)
#define FEATURE_FLAG_DISTORTION      (1u << 1)
#define FEATURE_FLAG_PREVIEW_RIM     (1u << 2)

// PostProcess Feature Flags
// PostProcess Feature Flags
#define POSTPROCESS_FLAG_HDR         (1u << 0)
#define POSTPROCESS_FLAG_TONEMAP     (1u << 1)
#define POSTPROCESS_FLAG_EXPOSURE    (1u << 2)
#define POSTPROCESS_FLAG_BLOOM       (1u << 3) // 아직 미사용
#define POSTPROCESS_FLAG_GAMMA       (1u << 4)
#define POSTPROCESS_FLAG_LIGHTSHAFT  (1u << 5)
#define POSTPROCESS_FLAG_DOF         (1u << 6)
#define POSTPROCESS_FLAG_COLORGRADE  (1u << 7)

#endif
