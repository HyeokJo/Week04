#pragma once 
#include "PCH.h"

//OBJ의 v/vt/vn 인덱스 조합 하나 = GPU 정점 하나. 같은 조합이 또 나오면 새 정점을 만들지 않고 재사용한다.
struct FFaceVertexKey
{
    int32 PositionIndex;
    int32 UVIndex;
    int32 NormalIndex;

    bool operator==(const FFaceVertexKey& Other) const noexcept
    {
        return PositionIndex == Other.PositionIndex
            && UVIndex == Other.UVIndex
            && NormalIndex == Other.NormalIndex;
    }
};

struct FFaceVertexKeyHash
{
    size_t operator()(const FFaceVertexKey& Key) const noexcept
    {
        size_t Hash = std::hash<int32>{}(Key.PositionIndex);
        Hash = Hash * 31 + std::hash<int32>{}(Key.UVIndex);
        Hash = Hash * 31 + std::hash<int32>{}(Key.NormalIndex);
        return Hash;
    }
};

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
    //다각형에 대응하기 위해서 1개 면이 저장하는 버텍스들을 TArray로 한번 더 감쌉니다.
    TArray<TArray<FFaceVertex>> FaceVertices_Polygon;

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
    TArray<FVector> Positions;
    TArray<FVector> Normals;
    TArray<FVector2> TexCoords;
    TArray<uint32> Indices;

    FString MaterialFileName;
    TArray<FString> MaterialNames;
    TArray<uint32> SubMeshIndexCounts;
};