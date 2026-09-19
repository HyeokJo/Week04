#include "PCH.h"
#include "FObjInporter.h"
#include <filesystem>
#include <fstream>
#include <format>
#include <unordered_map>

#include "../Console/Console.h"

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

	//삼각형으로만 되어 있는지, 다각형도 있는지 검사
	bool Ispolygon = false;

	while (std::getline(File, RawLine))
	{
		if (!RawLine.empty() && RawLine.back() == '\r')
		{
			RawLine.pop_back();
		}

		//한줄 씩
		FString Line(RawLine.c_str());
		TArray<FString> Tokens = SplitTokens(Line);

		if (Tokens.empty())
		{
			continue;
		}
		
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
			if (ObjInfo.FaceVertices_Polygon.size() != 0)
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

			// f  v  v  v  : 삼각형이면 개수가 4개
			// f  v  v  v  v : 다각형이면 개수가 5개 이상
			if (Tokens.size() > 4) Ispolygon = true;

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

			//반대로 뒤집기
			std::reverse(Vertices.begin(), Vertices.end());
			
			ObjInfo.FaceVertices_Polygon.push_back(Vertices);
		}
		else if (Tag == "mtllib")
		{
			ObjInfo.MaterialFileName = Tokens[1];
		}

	}

	//가장 마지막 Face들의 개수를 SubMesh에 넣어준다.
	ObjInfo.SubMesh.push_back(TempFaceCount);
	TempFaceCount = 0;
	
	//다각형이 포함된 모델이라면 배열 조합을 다르게 처리한다.
	return Ispolygon? BuildPolygonGeometry(ObjInfo, OutGeometry) : BuildGeometry(ObjInfo, OutGeometry);
}

bool FObjInporter::BuildGeometry(const FObjInfo& ObjInfo, FGeometry& OutGeometry) const
{
	if (ObjInfo.Positions.empty() || ObjInfo.FaceVertices_Polygon.empty())
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
	VertexCache.reserve(ObjInfo.FaceVertices_Polygon.size());

	OutGeometry.Positions.reserve(ObjInfo.FaceVertices_Polygon.size());
	OutGeometry.Normals.reserve(ObjInfo.FaceVertices_Polygon.size());
	OutGeometry.TexCoords.reserve(ObjInfo.FaceVertices_Polygon.size());
	OutGeometry.Indices.reserve(ObjInfo.FaceVertices_Polygon.size());

	//모든 Face Vertex 들
	for (auto& FaceVertics : ObjInfo.FaceVertices_Polygon)
	{
		// 1개 면의 모음
		for (const FFaceVertex& Face : FaceVertics)
		{
			//기본값(-1)이면 파싱이 깨진 코너이므로 건너뛴다.
			if (Face.PositionIndex == -1)
			{
				continue;
			}
						
			AddPNTIArray(Face, ObjInfo, OutGeometry, VertexCache);
		}
	}


	if (OutGeometry.Positions.empty() || OutGeometry.Indices.empty())
	{
		Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Obj Load Failed. Resulting geometry is empty.");
		return false;
	}

	return true;
}

bool FObjInporter::BuildPolygonGeometry(const FObjInfo& ObjInfo, FGeometry& OutGeometry) const
{
	//다각형이라면 Ear Clipping 방식을 따라갑니다.
	//아래 조건을 만족하는 삼각형을 찾아갑니다.
	//ObjInfo.FaceVertices를 순회하며 순서대로 Prev, Current, Next의 정점 3개로 삼각형을 구성합니다.
	//Current는 볼록해야합니다. 오목하다면 다음 삼각형으로 넘어갑니다.
	//		Current - Prev 벡터, Next - Current 벡터를 외적하여 노멀 벡터를 구하고, Face의 노멀과 내적하여 방향이 같다면 볼록, 방향이 다르다면 오목
	//Current가 볼록하다면 3개 정점으로 이루어진 삼각형 안에 다른 정점이 없어야 합니다. 다른 정점이 있다면 넘어갑니다.
	//		다른 모든 정점을 순회하며 검사합니다. 다른 정점 V에 대해 C - P, N - C, P - N 벡터와 V - P, V - C, V - N 벡터와 외적하여 모두가 양수라면 내부에 존재합니다.
	//		즉, 하나라도 양수가 아니라면 외부에 존재하니 성립합니다.
	//위 두가지 조건을 만족하는 삼각형이 나온다면 캐시 버텍스 검사를 통해 없는 버텍스라면 버텍스 위치 배열, 노멀 배열, uv 배열에 같은 인덱스로 해당하는 값들을 추가하고
	// 인덱스 배열에 추가합니다. 있는 버텍스라면 해당 인덱스를 인덱스 버퍼에만 추가합니다.
	//Current 정점을 제외하고 남은 정점에 대하여 해당 과정을 반복합니다.
	//남은 정점이 3개만 남는다면 인덱스 버퍼를 채우고 종료합니다.


	if (ObjInfo.Positions.empty() || ObjInfo.FaceVertices_Polygon.empty())
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
	VertexCache.reserve(ObjInfo.FaceVertices_Polygon.size());

	OutGeometry.Positions.reserve(ObjInfo.FaceVertices_Polygon.size());
	OutGeometry.Normals.reserve(ObjInfo.FaceVertices_Polygon.size());
	OutGeometry.TexCoords.reserve(ObjInfo.FaceVertices_Polygon.size());
	OutGeometry.Indices.reserve(ObjInfo.FaceVertices_Polygon.size());

	FFaceVertex Prev;
	FFaceVertex Current;
	FFaceVertex Next;
	FFaceVertex OtherVertex;
	FVector FaceNormal;

	//Face의 버텍스들을 순회할 인덱스
	int32 FaceVertexIndex = 0;

	const TArray<FVector>& FacePositions = ObjInfo.Positions;
	const TArray<FVector2>& FaceUVs = ObjInfo.UVs;
	const TArray<FVector>& FaceNormals = ObjInfo.Normals;

	//모든 Face Vertex 들
	//복사본으로 순회한다.
	for (auto FaceVertics : ObjInfo.FaceVertices_Polygon)
	{
		FaceVertexIndex = 0;

		// 1개 면의 모음
		while(FaceVertics.size() > 3 && FaceVertexIndex < FaceVertics.size())
		{
			//순서대로 버텍스 할당
			Prev = FaceVertics[FaceVertexIndex % FaceVertics.size()];
			Current = FaceVertics[(FaceVertexIndex + 1) % FaceVertics.size()];
			Next = FaceVertics[(FaceVertexIndex + 2) % FaceVertics.size()];

			int32 PrevPositionIndex = NormalizeIndex(Prev.PositionIndex, PositionCount);
			int32 CurrentPositionIndex = NormalizeIndex(Current.PositionIndex, PositionCount);
			int32 NextPositionIndex = NormalizeIndex(Next.PositionIndex, PositionCount);

			FaceNormal = (FaceNormals[NormalizeIndex(Prev.NormalIndex, NormalCount)] +
						  FaceNormals[NormalizeIndex(Current.NormalIndex, NormalCount)] +
						  FaceNormals[NormalizeIndex(Next.NormalIndex, NormalCount)]) / 3.f;
						

			//Current가 볼록한지 오목한지 검사
			//Current - Prev 벡터, Next - Current 벡터를 외적하여 노멀 벡터를 구하고, Face의 노멀과 내적하여 방향이 같다면 볼록, 방향이 다르다면 오목
			FVector Vector_1 = FacePositions[CurrentPositionIndex] - FacePositions[PrevPositionIndex];
			FVector Vector_2 = FacePositions[NextPositionIndex] - FacePositions[CurrentPositionIndex];
			
			Vector_1 = Vector_1.Cross(Vector_2);

			//내적값이 음수라면 오목
			if (Vector_1.Dot(FaceNormal) < 0)
			{
				FaceVertexIndex = (FaceVertexIndex + 1) % FaceVertics.size();
				Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "[Load OBJ File] Has Concave Vertex");
				continue;
			}


			//다른 모든 정점을 순회하며 검사합니다.
			//다른 정점 O에 대해 C - P, N - C, P - N 벡터와 O - P, O - C, O - N 벡터와 외적하여 노멀과 내적했을 때 모두가 양수라면 내부에 존재합니다.
			for (int i = FaceVertexIndex + 3; i < FaceVertics.size(); i++)
			{
				//다른 버텍스
				OtherVertex = FaceVertics[i];
				
				FVector C_PVector = FacePositions[CurrentPositionIndex] - FacePositions[PrevPositionIndex];
				FVector N_CVector = FacePositions[NextPositionIndex] - FacePositions[CurrentPositionIndex];
				FVector P_NVector = FacePositions[PrevPositionIndex] - FacePositions[NextPositionIndex];

				FVector O_PVector = FacePositions[NormalizeIndex(OtherVertex.PositionIndex, PositionCount)] - FacePositions[PrevPositionIndex];
				FVector O_CVector = FacePositions[NormalizeIndex(OtherVertex.PositionIndex, PositionCount)] - FacePositions[CurrentPositionIndex];
				FVector O_NVector = FacePositions[NormalizeIndex(OtherVertex.PositionIndex, PositionCount)] - FacePositions[NextPositionIndex];

				//다른 정점들을 순회하다가 내부에 있는 정점이 나온다면 중단
				if (C_PVector.Cross(O_PVector).Dot(FaceNormal) > 0 &&
					N_CVector.Cross(O_CVector).Dot(FaceNormal) > 0 &&
					P_NVector.Cross(O_NVector).Dot(FaceNormal) > 0)
				{
					FaceVertexIndex = (FaceVertexIndex + 1) % FaceVertics.size();
					Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "[Load OBJ File] Vertex located inside the triangle");
					continue;
				}
			}


			//여기까지 통과했으면 해당 삼각형을 추가하고 Current를 잘라낸다.
			for (int i = 0; i < 3; i++)
			{
				AddPNTIArray(FaceVertics[FaceVertexIndex + i], ObjInfo, OutGeometry, VertexCache);
			}

			//Current 제거
			FaceVertics.erase(FaceVertics.begin() + (FaceVertexIndex + 1) % FaceVertics.size());
		}

		//남은 삼각형 1개만 남았다. 순서대로 입력
		for (auto& Face : FaceVertics)
		{
			AddPNTIArray(Face, ObjInfo, OutGeometry, VertexCache);
		}		
	}

	if (OutGeometry.Positions.empty() || OutGeometry.Indices.empty())
	{
		Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Obj Load Failed. Resulting geometry is empty.");
		return false;
	}

	return true;
}

void FObjInporter::AddPNTIArray(const FFaceVertex& TargetVertex, const FObjInfo& ObjInfo, FGeometry& OutGeometry, 
								std::unordered_map<FFaceVertexKey, uint32, FFaceVertexKeyHash>& CacheMap) const
{

	const int32 PositionCount = static_cast<int32>(ObjInfo.Positions.size());
	const int32 UVCount = static_cast<int32>(ObjInfo.UVs.size());
	const int32 NormalCount = static_cast<int32>(ObjInfo.Normals.size());

	//버텍스 배열에서 index
	const int32 NormalizedPosition = NormalizeIndex(TargetVertex.PositionIndex, PositionCount);

	//vt/vn은 obj 상에서 생략 가능하므로, -1(생략)일 때는 정규화를 시도하지 않는다.
	const int32 NormalizedUV = (TargetVertex.UVIndex != -1) ? NormalizeIndex(TargetVertex.UVIndex, UVCount) : -1;
	const int32 NormalizedNormal = (TargetVertex.NormalIndex != -1) ? NormalizeIndex(TargetVertex.NormalIndex, NormalCount) : -1;

	if (NormalizedPosition < 0 || NormalizedPosition >= PositionCount)
	{
		return;
	}

	const FFaceVertexKey Key{ NormalizedPosition, NormalizedUV, NormalizedNormal };

	const auto ExistingEntry = CacheMap.find(Key);
	if (ExistingEntry != CacheMap.end())
	{
		//이미 같은 조합의 정점이 있다면 새로 만들지 않고 인덱스만 재사용한다.
		OutGeometry.Indices.push_back(ExistingEntry->second);
		return;
	}

	const uint32 NewIndex = static_cast<uint32>(OutGeometry.Positions.size());

	OutGeometry.Positions.push_back(ObjInfo.Positions[NormalizedPosition]);

	OutGeometry.TexCoords.push_back(
		(NormalizedUV >= 0 && NormalizedUV < UVCount) ? ObjInfo.UVs[NormalizedUV] : FVector2(0.f, 0.f));

	OutGeometry.Normals.push_back(
		(NormalizedNormal >= 0 && NormalizedNormal < NormalCount) ? ObjInfo.Normals[NormalizedNormal] : FVector(0.f, 0.f, 0.f));

	CacheMap.emplace(Key, NewIndex);
	OutGeometry.Indices.push_back(NewIndex);
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
