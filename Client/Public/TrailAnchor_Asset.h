#pragma once

#include "Client_Defines.h"

NS_BEGIN(Client)

struct TrailAnchorPointDesc
{
    string boneName{};    // 이 anchor가 어느 본 기준인지 저장한다.
    Vec3 localPosition{}; // 선택 본 local 좌표계 기준 anchor 위치다.
};

struct TrailAnchorAsset
{
    int32 version = 1;
    string modelGuid{};          // 어떤 모델용 anchor 데이터인지 식별한다.
    TrailAnchorPointDesc base{}; // 검 손잡이 쪽 trail 시작점이다.
    TrailAnchorPointDesc tip{};  // 검 끝쪽 trail 끝점이다.
};

class TrailAnchor_Parser final
{
public:
    static fs::path Get_ModelTrailAnchorFilePath(const string& modelGuid); // 모델 guid 기준 저장 경로를 만든다.
    static bool Load_FromFile(const wstring& filePath, TrailAnchorAsset& outAsset);
    static bool Save_ToFile(const wstring& filePath, const TrailAnchorAsset& asset);
};

NS_END
