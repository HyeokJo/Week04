#pragma once
#include <d3d11.h>

class FAssetRegistry;
class UMesh;

struct FFaceVertex
{
    int32 PositionIndex = -1;
    int32 UVIndex = -1;
    int32 NormalIndex = -1;
};

struct FObjInfo
{
    FString AssetName;
    //이 오브젝트의 머티리얼 정보가 담긴 mtl 파일 이름
    FString MaterialFileName;

    TArray<FVector>  Positions;
    TArray<FVector2> UVs;
    TArray<FVector>  Normals;

    // face 하나를 구성하는 정점 순서대로 : 나중에 인덱스 버퍼의 순서가 된다.
    // v/vt/vn 조합
    TArray<FFaceVertex> FaceVertices;

    //머티리얼 이름들
    //SubMesh와 인덱스 매칭한다.
    TArray<FString> MaterialNames;

    //동일한 머티리얼을 쓰는 정점들의 개수
    //MaterialNames와 인덱스 매칭한다.
    //MaterialNames[0]를 쓰는 정점의 개수는 SubMesh[0]개
    TArray<int32> SubMesh;
};

struct FGeometry
{
    TArray<FVector>  Positions;
    TArray<FVector>  Normals;
    TArray<FVector2> TexCoords;
    TArray<uint32>    Indices;
};

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

private:
    FString LastError{};
    FVector PositionCoordTrans_X = FVector(0.f, 0.f, -1.f);
    FVector PositionCoordTrans_Y = FVector(1.f, 0.f, 0.f);
    FVector PositionCoordTrans_Z = FVector(0.f, 1.f, 0.f);
};