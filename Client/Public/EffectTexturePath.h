#pragma once

// 이펙트 텍스처 경로 해석 helper를 모은다. GUID나 에셋 상대 경로를 실제 파일 경로로 바꾼다.

#include "Client_Defines.h"

namespace Client::EffectTexturePath
{
// 텍스처 GUID를 실제 파일 경로로 바꾼다. 텍스처 에셋이 아니거나 파일이 없으면 빈 문자열이다.
wstring Resolve_ByGuid(const string& textureGuid);

// 텍스처 경로(에셋 루트 기준 상대 경로 포함)를 실제 파일 경로로 바꾼다. 파일이 없으면 빈 문자열이다.
wstring Resolve_ByPath(const string& texturePath);

// 두 경로가 정규화 후 같은 파일을 가리키는지 비교한다. 하나라도 비어 있으면 false다.
bool Is_SamePath(const wstring& lhs, const wstring& rhs);
}
