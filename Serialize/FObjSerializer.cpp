#include "PCH.h"
#include "FObjSerializer.h"
#include <fstream>

bool FObjSerializer::SaveBinary(const FGeometry& GeometryData, const FString& FilePath)
{
	std::ofstream Out(FilePath.c_str(), std::ios::binary);
	if (!Out.is_open())
	{
		return false;
	}

	Out.write(reinterpret_cast<const char*>(&MagicNumber), sizeof(MagicNumber));
	Out.write(reinterpret_cast<const char*>(&CurrentVersion), sizeof(CurrentVersion));

	//Position
	uint32 PositionCount = static_cast<uint32>(GeometryData.Positions.size());
	//배열 개수
	Out.write(reinterpret_cast<const char*>(&PositionCount), sizeof(PositionCount));
	//버텍스 위치값 저장
	Out.write(reinterpret_cast<const char*>(GeometryData.Positions.data()), PositionCount * sizeof(FVector));

	//Normal
	uint32 NormalCount = static_cast<uint32>(GeometryData.Normals.size());
	Out.write(reinterpret_cast<const char*>(&NormalCount), sizeof(NormalCount));
	Out.write(reinterpret_cast<const char*>(GeometryData.Normals.data()), NormalCount * sizeof(FVector));

	//UV
	uint32 UVCount = static_cast<uint32>(GeometryData.TexCoords.size());
	Out.write(reinterpret_cast<const char*>(&UVCount), sizeof(UVCount));
	Out.write(reinterpret_cast<const char*>(GeometryData.TexCoords.data()), UVCount * sizeof(FVector2));

	//Index
	uint32 IndexCount = static_cast<uint32>(GeometryData.Indices.size());
	Out.write(reinterpret_cast<const char*>(&IndexCount), sizeof(IndexCount));
	Out.write(reinterpret_cast<const char*>(GeometryData.Indices.data()), IndexCount * sizeof(uint32));

	//Material File Name
	WriteFString(Out, GeometryData.MaterialFileName);

	//Material Names
	uint32 MaterialNamesCount = static_cast<uint32>(GeometryData.MaterialNames.size());
	Out.write(reinterpret_cast<const char*>(&MaterialNamesCount), sizeof(MaterialNamesCount));
	for (int i = 0; i < MaterialNamesCount; i++)
	{
		WriteFString(Out, GeometryData.MaterialNames[i]);
	}
	//Out.write(reinterpret_cast<const char*>(GeometryData.MaterialNames.data()), MaterialNamesCount * sizeof(FString));

	//SubMeshIndexCounts
	uint32 SubMeshIndexCount = static_cast<uint32>(GeometryData.SubMeshIndexCounts.size());
	Out.write(reinterpret_cast<const char*>(&SubMeshIndexCount), sizeof(SubMeshIndexCount));
	Out.write(reinterpret_cast<const char*>(GeometryData.SubMeshIndexCounts.data()), SubMeshIndexCount * sizeof(uint32));

	return static_cast<bool>(Out);
}

bool FObjSerializer::LoadBinary(const FString& FilePath, FGeometry& OutGeoData)
{
	std::ifstream In(FilePath.c_str(), std::ios::binary);
	if (!In.is_open())
	{
		return false;
	}

	uint32 Magic = 0;
	uint32 Version = 0;

	In.read(reinterpret_cast<char*>(&Magic), sizeof(Magic));
	In.read(reinterpret_cast<char*>(&Version), sizeof(Version));

	//버전이 안 맞으면 바이너리를 읽어오지 않는다.
	if (Magic != MagicNumber || Version != CurrentVersion)
	{
		return false;
	}

	uint32 PositionCount = 0;
	In.read(reinterpret_cast<char*>(&PositionCount), sizeof(PositionCount));
	OutGeoData.Positions.resize(PositionCount);
	In.read(reinterpret_cast<char*>(OutGeoData.Positions.data()), sizeof(FVector) * PositionCount);

	uint32 NormalCount = 0;
	In.read(reinterpret_cast<char*>(&NormalCount), sizeof(NormalCount));
	OutGeoData.Normals.resize(NormalCount);
	In.read(reinterpret_cast<char*>(OutGeoData.Normals.data()), sizeof(FVector) * NormalCount);

	uint32 UVCount = 0;
	In.read(reinterpret_cast<char*>(&UVCount), sizeof(UVCount));
	OutGeoData.TexCoords.resize(UVCount);
	In.read(reinterpret_cast<char*>(OutGeoData.TexCoords.data()), sizeof(FVector2) * UVCount);

	uint32 IndexCount = 0;
	In.read(reinterpret_cast<char*>(&IndexCount), sizeof(IndexCount));
	OutGeoData.Indices.resize(IndexCount);
	In.read(reinterpret_cast<char*>(OutGeoData.Indices.data()), sizeof(uint32) * IndexCount);

	//Material File Name
	ReadFString(In, OutGeoData.MaterialFileName);

	//Material Names
	uint32 MaterialNamesCount = 0;
	In.read(reinterpret_cast<char*>(&MaterialNamesCount), sizeof(MaterialNamesCount));
	OutGeoData.MaterialNames.resize(MaterialNamesCount);
	for (int i = 0; i < MaterialNamesCount; i++)
	{
		ReadFString(In, OutGeoData.MaterialNames[i]);
	}
	//In.read(reinterpret_cast<char*>(OutGeoData.MaterialNames.data()), sizeof(FString) * MaterialNamesCount);;

	//SubMeshIndexCounts
	uint32 SubMeshIndexCount = 0;
	In.read(reinterpret_cast<char*>(&SubMeshIndexCount), sizeof(SubMeshIndexCount));
	OutGeoData.SubMeshIndexCounts.resize(SubMeshIndexCount);
	In.read(reinterpret_cast<char*>(OutGeoData.SubMeshIndexCounts.data()), sizeof(uint32) * SubMeshIndexCount);;

	return static_cast<bool>(In);
}

bool FObjSerializer::WriteFString(std::ofstream& Out, const FString& Str)
{
	uint32 Len = static_cast<uint32>(Str.size());
	//크기
	Out.write(reinterpret_cast<const char*>(&Len), sizeof(Len));
	//데이터
	Out.write(Str.data(), Len);
	return static_cast<bool>(Out);
}

bool FObjSerializer::ReadFString(std::ifstream& In, FString& OutStr)
{
	uint32 Len = 0;
	In.read(reinterpret_cast<char*>(&Len), sizeof(Len));
	if (!In) return false;

	OutStr.resize(Len);
	In.read(OutStr.data(), Len);
	return static_cast<bool>(In);
}
