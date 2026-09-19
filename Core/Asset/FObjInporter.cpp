#include "PCH.h"
#include "FObjInporter.h"
#include <filesystem>
#include <fstream>
#include <format>
#include <unordered_map>

#include "../Console/Console.h"

namespace
{
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
}

bool FObjInporter::LoadObjFile(const FString& FilePath, FGeometry& OutGeometry)
{
	std::ifstream File(FilePath.c_str());
	if (!File.is_open())
	{
		Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Obj Load Failed. Can not Open File");
		return false;
	}

	std::string RawLine;

	FObjInfo ObjInfo;

	//SubMesh에 넣기 전 임시 저장 변수
	int32 TempFaceCount = 0;

	while (std::getline(File, RawLine))
	{
		if (!RawLine.empty() && RawLine.back() == '\r')
		{
			RawLine.pop_back();
		}

		//한줄 씩
		FString Line(RawLine.c_str());
		TArray<FString> Tokens = SplitTokens(Line);
		/*for (auto& Elem : Tokens)
		{
			Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, Elem.c_str());
		}*/

		if (Tokens.empty())
		{
			continue;
		}
		
		//Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, Tokens[0].c_str());
		
		const FString& Tag = Tokens[0];
			

		if (Tag == "v") // v x y z
		{
			if (Tokens.size() != 4) continue;
			FVector Pos = FVector(std::stof(Tokens[1].c_str()), std::stof(Tokens[2].c_str()), std::stof(Tokens[3].c_str()));
			//각 x,y,z에 좌표계 변환
			Pos = FVector(Pos.Dot(PositionCoordTrans_X), Pos.Dot(PositionCoordTrans_Y), Pos.Dot(PositionCoordTrans_Z));
			ObjInfo.Positions.push_back(Pos);
		}
		else if (Tag == "vn") // vn x y z
		{
			if (Tokens.size() != 4) continue;
			FVector Normal = FVector(std::stof(Tokens[1].c_str()), std::stof(Tokens[2].c_str()), std::stof(Tokens[3].c_str()));
			Normal = FVector(Normal.Dot(PositionCoordTrans_X), Normal.Dot(PositionCoordTrans_Y), Normal.Dot(PositionCoordTrans_Z));
			ObjInfo.Normals.push_back(Normal);
		}
		else if (Tag == "vt") // vt u v
		{
			if (Tokens.size() != 3) continue;
			FVector2 UV = FVector2(std::stof(Tokens[1].c_str()), std::stof(Tokens[2].c_str()));
			ObjInfo.UVs.push_back(UV);
		}
		else if (Tag == "o") // o Name
		{
			if (Tokens.size() != 2) continue;
			//이름 저장
			ObjInfo.AssetName = Tokens[1];
		}
		else if (Tag == "s")
		{
			//스무딩 그룹이라는데 일단 대기
		}
		else if (Tag == "usemtl") // usemtl Name
		{
			if (Tokens.size() != 2) continue;
			//머티리얼 이름 넣기
			ObjInfo.MaterialNames.push_back(Tokens[1]);

			//처음엔 FaceViertices가 없어서 넣으면 안된다.
			if (ObjInfo.FaceVertices.size() != 0)
			{
				//face들을 순회하다가 새로운 머티리얼을 만난다면 지금까지의 face들의 개수를 SubMesh에 넣어준다.
				ObjInfo.SubMesh.push_back(TempFaceCount);
				TempFaceCount = 0;
			}
		}
		else if (Tag == "f")
		{
			//v, v/vt, v//vn, v/vt/vn 네가지 형태 있음.

			if (Tokens.size() == 0) continue;

			//임시 저장
			TArray<FFaceVertex> Vertices;

			//가장 앞인 f를 제외한 나머지 버텍스에 대해서 순회하며 저장
			for (int i = 1; i < Tokens.size(); i++)
			{
				FString VertexData = Tokens[i];
				size_t FirstIndex = VertexData.find('/');
				size_t SecondIndex = 0;
				FFaceVertex Face;

				// '/'를 찾지 못했다면 vertex position만 있는 것.
				if (FirstIndex == FString::npos)
				{
					Face.PositionIndex = std::stoi(VertexData.c_str());
				}
				else
				{
					// v는 '/'의 위치까지 잘라낸 값
					//std.substr(pos, count) : pos부터 count개 문자열 반환. count 기본값은 npos로 끝까지 추출
					Face.PositionIndex = std::stoi(VertexData.substr(0, FirstIndex).c_str());

					//위에서 찾은 곳 다음 칸부터 '/'를 또 찾는다.
					SecondIndex = VertexData.find('/', FirstIndex + 1);
					if (SecondIndex == FString::npos)
					{
						//두번째에 '/'가 없다면 vt만 있다. v/vt
						//처음 찾은 PositionIndex 다음부터 끝까지(기본값 npos) 잘라내면 vt다
						Face.UVIndex = std::stoi(VertexData.substr(FirstIndex + 1).c_str());
					}
					else
					{
						// v//vn 아니면 v/vt/vn 이다

						// v/vt/n 인 경우만 vt를 계산하도록 한다.
						if (SecondIndex > FirstIndex + 1)
						{
							// vt는 PositonIndex 다음부터 SecondIndex - positonindex - 1개를 잘라낸다
							Face.UVIndex = std::stoi(VertexData.substr(FirstIndex + 1, SecondIndex - FirstIndex - 1).c_str());
						}

						//두번째 다음부터 끝까지 잘라내면 n이다
						Face.NormalIndex = std::stoi(VertexData.substr(SecondIndex + 1).c_str());
					}
				}
				Vertices.push_back(Face);
			}

			//SubMesh에 넣어주기 위한 FaceCount값
			TempFaceCount += static_cast<int32>(Vertices.size());

			//다각형일 경우 삼각형으로 만들어서 넣어야 한다.. 일단 보류
			for (int32 i = static_cast<int32>(Vertices.size()) - 1; i >= 0; i--)
			{
				//인덱스 순서 반대로 넣어주기
				ObjInfo.FaceVertices.push_back(Vertices[i]);
			}

		}
		else if (Tag == "mtllib")
		{
			ObjInfo.MaterialFileName = Tokens[1];
		}

	}

	//가장 마지막 Face들의 개수를 SubMesh에 넣어준다.
	ObjInfo.SubMesh.push_back(TempFaceCount);
	TempFaceCount = 0;

	return BuildGeometry(ObjInfo, OutGeometry);
}

bool FObjInporter::BuildGeometry(const FObjInfo& ObjInfo, FGeometry& OutGeometry) const
{
	if (ObjInfo.Positions.empty() || ObjInfo.FaceVertices.empty())
	{
		Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Obj Load Failed. No geometry data to build.");
		return false;
	}

	//초기화
	OutGeometry.Positions.clear();
	OutGeometry.Normals.clear();
	OutGeometry.TexCoords.clear();
	OutGeometry.Indices.clear();

	const int32 PositionCount = static_cast<int32>(ObjInfo.Positions.size());
	const int32 UVCount = static_cast<int32>(ObjInfo.UVs.size());
	const int32 NormalCount = static_cast<int32>(ObjInfo.Normals.size());

	//(PositionIndex, UVIndex, NormalIndex) 조합 -> 이미 만들어둔 OutGeometry 상의 정점 인덱스
	std::unordered_map<FFaceVertexKey, uint32, FFaceVertexKeyHash> VertexCache;
	VertexCache.reserve(ObjInfo.FaceVertices.size());

	OutGeometry.Positions.reserve(ObjInfo.FaceVertices.size());
	OutGeometry.Normals.reserve(ObjInfo.FaceVertices.size());
	OutGeometry.TexCoords.reserve(ObjInfo.FaceVertices.size());
	OutGeometry.Indices.reserve(ObjInfo.FaceVertices.size());

	/*OutGeometry.Positions.resize(ObjInfo.Positions.size());
	OutGeometry.Normals.resize(ObjInfo.Positions.size());
	OutGeometry.TexCoords.resize(ObjInfo.Positions.size());
	OutGeometry.Indices.resize(ObjInfo.FaceVertices.size());*/

	for (const FFaceVertex& Face : ObjInfo.FaceVertices)
	{
		//기본값(-1)이면 파싱이 깨진 코너이므로 건너뛴다.
		if (Face.PositionIndex == -1)
		{
			continue;
		}

		//버텍스 배열에서 index
		const int32 NormalizedPosition = NormalizeIndex(Face.PositionIndex, PositionCount);

		//vt/vn은 obj 상에서 생략 가능하므로, -1(생략)일 때는 정규화를 시도하지 않는다.
		const int32 NormalizedUV = (Face.UVIndex != -1) ? NormalizeIndex(Face.UVIndex, UVCount) : -1;
		const int32 NormalizedNormal = (Face.NormalIndex != -1) ? NormalizeIndex(Face.NormalIndex, NormalCount) : -1;

		if (NormalizedPosition < 0 || NormalizedPosition >= PositionCount)
		{
			continue;
		}

		const FFaceVertexKey Key{ NormalizedPosition, NormalizedUV, NormalizedNormal };

		const auto ExistingEntry = VertexCache.find(Key);
		if (ExistingEntry != VertexCache.end())
		{
			//이미 같은 조합의 정점이 있다면 새로 만들지 않고 인덱스만 재사용한다.
			OutGeometry.Indices.push_back(ExistingEntry->second);
			continue;
		}

		const uint32 NewIndex = static_cast<uint32>(OutGeometry.Positions.size());

		OutGeometry.Positions.push_back(ObjInfo.Positions[NormalizedPosition]);

		OutGeometry.TexCoords.push_back(
			(NormalizedUV >= 0 && NormalizedUV < UVCount) ? ObjInfo.UVs[NormalizedUV] : FVector2(0.f, 0.f));

		OutGeometry.Normals.push_back(
			(NormalizedNormal >= 0 && NormalizedNormal < NormalCount) ? ObjInfo.Normals[NormalizedNormal] : FVector(0.f, 0.f, 0.f));

		VertexCache.emplace(Key, NewIndex);
		OutGeometry.Indices.push_back(NewIndex);
	}

	if (OutGeometry.Positions.empty() || OutGeometry.Indices.empty())
	{
		Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Obj Load Failed. Resulting geometry is empty.");
		return false;
	}

	return true;
}

TArray<FString> FObjInporter::SplitTokens(const FString& Line)
{
	TArray<FString> Tokens;
	std::istringstream Stream(Line);
	std::string Token;
	while (Stream >> Token)
	{
		Tokens.push_back(FString(Token.c_str()));
	}
	return Tokens;
}

int32 FObjInporter::NormalizeIndex(int32 RawIndex, int32 ArraySize) const
{
	if (RawIndex > 0){
		return RawIndex - 1;
	}
	//RawIndex가 음수라면 뒤에서부터 인덱스를 샌다.
	return ArraySize + RawIndex;
}
