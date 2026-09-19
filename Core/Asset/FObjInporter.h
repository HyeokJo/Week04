#pragma once
#include <d3d11.h>

#include "FObjInfo.h"

class FAssetRegistry;
class UMesh;

class FObjInporter
{
public:
    FObjInporter() = default;
    ~FObjInporter() = default;

    FObjInporter(const FObjInporter&) = delete;
    FObjInporter& operator=(const FObjInporter&) = delete;

    FObjInporter(FObjInporter&&) noexcept = default;
    FObjInporter& operator=(FObjInporter&&) noexcept = default;

    //Obj 파일 로드
    //std::unique_ptr<UObject> LoadObjFile(const FString& FilePath);
    bool LoadObjFile(const FString& FilePath, FGeometry& OutGeometry);

    //한줄 나누기
    static TArray<FString> SplitTokens(const FString& Line);

    //obj의 f값을 인덱스로 바꾸기 위해 1을 뺍니다.
    //음수값이라면 뒤에서부터 가져옵니다.
    int32 NormalizeIndex(int32 RawIndex, int32 ArraySize) const;

private:
    //파싱된 ObjInfo로 Vertex Position, Index, Normal, TexCoord 배열을 만든다.
    bool BuildGeometry(const FObjInfo& ObjInfo, FGeometry& OutGeometry) const;

    //다각형이라면 계산을 다르게 처리
    bool BuildPolygonGeometry(const FObjInfo& ObjInfo, FGeometry& OutGeometry) const;

    //1개 버텍스 데이터 저장
    void AddPNTIArray(const FFaceVertex& TargetVertex, const FObjInfo& ObjInfo, FGeometry& OutGeometry, 
                      std::unordered_map<FFaceVertexKey, uint32, FFaceVertexKeyHash>& CacheMap) const;

private:
    FString LastError{};
    FVector PositionCoordTrans_X = FVector(0.f, 0.f, -1.f);
    FVector PositionCoordTrans_Y = FVector(1.f, 0.f, 0.f);
    FVector PositionCoordTrans_Z = FVector(0.f, 1.f, 0.f);
};