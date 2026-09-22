#pragma once 
#include "../Core/Asset/FObjInfo.h"

class FObjSerializer
{
public:
	static constexpr uint32 MagicNumber = 0x4d45534d; // MESM : Macaw Engine Static Mesh
	static constexpr uint32 CurrentVersion = 2;

	static bool SaveBinary(const FGeometry& GeometryData, const FString& FilePath);
	static bool LoadBinary(const FString& FilePath, FGeometry& OutGeoData);

private:
	static bool WriteFString(std::ofstream& Out, const FString& Str);
	static bool ReadFString(std::ifstream& In, FString& OutStr);
};