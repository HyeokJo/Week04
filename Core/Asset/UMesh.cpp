#include "PCH.h"
#include "UMesh.h"

#include "../../ErrorHandler.h"
#include "FAssetMetadataParser.h"

#include "../Console/Console.h"

#include "BasicGeometry/Capsule.h"
#include "BasicGeometry/Corn.h"
#include "BasicGeometry/Cube.h"
#include "BasicGeometry/Cylinder.h"
#include "BasicGeometry/Pyramid.h"
#include "BasicGeometry/Plane.h"
#include "BasicGeometry/Sphere.h"
#include "BasicGeometry/Torus.h"
#include "BasicGeometry/InverseSphere.h"
#include "FObjInporter.h"

void UMesh::Initialize(ID3D11Device* Device, const std::filesystem::path& metaData) {
	UAsset::Initialize(Device, metaData);

	FAssetMetadataParser MetadataParser{};

	ErrorHandler::Report(not MetadataParser.Load(AssetMetaDataPath), " [ UMesh ]", "Failed to load metadata", ErrorHandler::EErrorLevel::Critical);

	if (MetadataParser.GetOr("BasicMesh", false)) {
		const FString MeshType = MetadataParser.GetOr("MeshType", FString{});
		
		if (MeshType == "Plane") {
			UMesh::Make(Device, BasicGeometry::Plane::Indices,
				MakeVertexAttribute<EVertexAttribute::Position>(BasicGeometry::Plane::Positions),
				MakeVertexAttribute<EVertexAttribute::Normal>(BasicGeometry::Plane::Normals),
				MakeVertexAttribute<EVertexAttribute::UV>(BasicGeometry::Plane::TexCoords)
			);	
		} 
		else if (MeshType == "Cube") {
			UMesh::Make(Device, BasicGeometry::Cube::Indices,
				MakeVertexAttribute<EVertexAttribute::Position>(BasicGeometry::Cube::Positions),
				MakeVertexAttribute<EVertexAttribute::Normal>(BasicGeometry::Cube::Normals),
				MakeVertexAttribute<EVertexAttribute::UV>(BasicGeometry::Cube::TexCoords)
			);
		} 
		else if (MeshType == "Sphere") {
			UMesh::Make(Device, BasicGeometry::Sphere::Indices,
				MakeVertexAttribute<EVertexAttribute::Position>(BasicGeometry::Sphere::Positions),
				MakeVertexAttribute<EVertexAttribute::Normal>(BasicGeometry::Sphere::Normals),
				MakeVertexAttribute<EVertexAttribute::UV>(BasicGeometry::Sphere::TexCoords)
			);
		} 
		else if (MeshType == "Capsule") {
			UMesh::Make(Device, BasicGeometry::Capsule::Indices,
				MakeVertexAttribute<EVertexAttribute::Position>(BasicGeometry::Capsule::Positions),
				MakeVertexAttribute<EVertexAttribute::Normal>(BasicGeometry::Capsule::Normals),
				MakeVertexAttribute<EVertexAttribute::UV>(BasicGeometry::Capsule::TexCoords)
			);
		}
		else if (MeshType == "Cone") {
			UMesh::Make(Device, BasicGeometry::Cone::Indices,
				MakeVertexAttribute<EVertexAttribute::Position>(BasicGeometry::Cone::Positions),
				MakeVertexAttribute<EVertexAttribute::Normal>(BasicGeometry::Cone::Normals),
				MakeVertexAttribute<EVertexAttribute::UV>(BasicGeometry::Cone::TexCoords)
			);
		}
		else if (MeshType == "Cylinder") {
			UMesh::Make(Device, BasicGeometry::Cylinder::Indices,
				MakeVertexAttribute<EVertexAttribute::Position>(BasicGeometry::Cylinder::Positions),
				MakeVertexAttribute<EVertexAttribute::Normal>(BasicGeometry::Cylinder::Normals),
				MakeVertexAttribute<EVertexAttribute::UV>(BasicGeometry::Cylinder::TexCoords)
			);
		}
		else if (MeshType == "Pyramid") {
			UMesh::Make(Device, BasicGeometry::Pyramid::Indices,
				MakeVertexAttribute<EVertexAttribute::Position>(BasicGeometry::Pyramid::Positions),
				MakeVertexAttribute<EVertexAttribute::Normal>(BasicGeometry::Pyramid::Normals),
				MakeVertexAttribute<EVertexAttribute::UV>(BasicGeometry::Pyramid::TexCoords)
			);
		}
		else if (MeshType == "Torus") {
			float MajorRadius = MetadataParser.GetOr("MajorRadius",BasicGeometry::Torus::MajorRadius);
			float MinorRadius = MetadataParser.GetOr("MinorRadius",BasicGeometry::Torus::MinorRadius);

			if (MajorRadius <= 0.0f) 
			{
				MajorRadius = BasicGeometry::Torus::MajorRadius;
			}

			if (MinorRadius <= 0.0f)
			{
				MinorRadius = BasicGeometry::Torus::MinorRadius;
			}

			const BasicGeometry::Torus::FGeometry Geometry =BasicGeometry::Torus::GenerateGeometry(MajorRadius,MinorRadius);
			UMesh::Make(Device, BasicGeometry::Torus::Indices,
				MakeVertexAttribute<EVertexAttribute::Position>(Geometry.Positions),
				MakeVertexAttribute<EVertexAttribute::Normal>(Geometry.Normals),
				MakeVertexAttribute<EVertexAttribute::UV>(Geometry.TexCoords)
			);
		}
		else if (MeshType == "SkyDome") {
			UMesh::Make(Device, BasicGeometry::SkyDome::Indices,
				MakeVertexAttribute<EVertexAttribute::Position>(BasicGeometry::SkyDome::Positions),
				MakeVertexAttribute<EVertexAttribute::Normal>(BasicGeometry::SkyDome::Normals),
				MakeVertexAttribute<EVertexAttribute::UV>(BasicGeometry::SkyDome::TexCoords)
			);
		}
		else {
			ErrorHandler::Report(false, " [ UMesh ]", "Unsupported BasicMesh type: " + MeshType, ErrorHandler::EErrorLevel::Critical);
		}
	}	
	else
	{
		FString FilePath = MetadataParser.GetOr("FilePath", FString(""));
		
		FObjInporter ObjImporter;
		FGeometry Geometry;

		//meta 파일 안의 obj 경로로 로드
		if (ObjImporter.LoadObjFile(FilePath.c_str(), Geometry))
		{
			UMesh::Make(Device, Geometry.Indices,
						MakeVertexAttribute<EVertexAttribute::Position>(Geometry.Positions),
						MakeVertexAttribute<EVertexAttribute::Normal>(Geometry.Normals),
						MakeVertexAttribute<EVertexAttribute::UV>(Geometry.TexCoords));
		}
		else
		{
			ErrorHandler::Report(false, " [ UMesh ]", "Failed to Import OBJ File: " + FilePath, ErrorHandler::EErrorLevel::Critical);
		}		
	}
	
}

bool UMesh::InitializeFromObjFile(ID3D11Device* Device, const std::filesystem::path& ObjPath, const FMaterialResolver& MaterialResolver, const FMaterialGroupResolver& MaterialGroupResolver) {
	if (Device == nullptr || ObjPath.empty()) {
		Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Model load rejected: device or OBJ path is invalid.");
		return false;
	}

	UAsset::Initialize(Device, ObjPath);

	FObjInporter ObjImporter{};
	FGeometry Geometry{};

	if (!ObjImporter.LoadObjFile(ObjPath.string().c_str(), Geometry)) {
		Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Failed to import OBJ geometry: %s", ObjPath.generic_string().c_str());
		return false;
	}

	if (Geometry.MaterialNames.empty() && Geometry.SubMeshIndexCounts.size() == 1) {
		Geometry.MaterialNames.push_back({});
	}

	FAssetHandle ImportedMaterial{};

	if (!Geometry.MaterialFileName.empty()) {
		const std::filesystem::path MaterialPath = (ObjPath.parent_path() / std::filesystem::path(Geometry.MaterialFileName.c_str())).lexically_normal();
		if (MaterialResolver) {
			ImportedMaterial = MaterialResolver(MaterialPath);
		}

		if (!ImportedMaterial) {
			Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Model MTL asset was not found; using material group 0: %s", MaterialPath.generic_string().c_str());
		}
	}

	TArray<FSubMesh> ImportedSubMeshes{};
	ImportedSubMeshes.reserve(Geometry.SubMeshIndexCounts.size());

	if (Geometry.MaterialNames.size() != Geometry.SubMeshIndexCounts.size()) {
		Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Model material group count does not match submesh count: %s", ObjPath.generic_string().c_str());
		return false;
	}

	uint32 FirstIndex = 0;

	for (uint32 SubMeshIndex = 0; SubMeshIndex < Geometry.SubMeshIndexCounts.size(); ++SubMeshIndex) {
		FSubMesh SubMesh{};
		SubMesh.FirstIndex = FirstIndex;
		SubMesh.IndexCount = Geometry.SubMeshIndexCounts[SubMeshIndex];

		if (SubMesh.FirstIndex > Geometry.Indices.size() || SubMesh.IndexCount > Geometry.Indices.size() - SubMesh.FirstIndex) {
			Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Model submesh index range is invalid: %s", ObjPath.generic_string().c_str());
			return false;
		}

		FirstIndex += SubMesh.IndexCount;

		const FString& MaterialName = Geometry.MaterialNames[SubMeshIndex];

		if (!MaterialName.empty()) {
			if (ImportedMaterial && MaterialGroupResolver) {
				const std::optional<uint32> MaterialGroupIndex = MaterialGroupResolver(ImportedMaterial, MaterialName);
				if (MaterialGroupIndex.has_value()) {
					SubMesh.MaterialGroupIndex = *MaterialGroupIndex;
				}
				else {
					Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Model MTL group was not found; using material group 0: %s in %s", MaterialName.c_str(), ObjPath.generic_string().c_str());
				}
			}
			else {
				Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Model has no usable MTL; using material group 0: %s", ObjPath.generic_string().c_str());
			}
		}

		ImportedSubMeshes.push_back(SubMesh);
	}

	if (FirstIndex != Geometry.Indices.size() || !Make(Device, Geometry.Indices,
		MakeVertexAttribute<EVertexAttribute::Position>(Geometry.Positions),
		MakeVertexAttribute<EVertexAttribute::Normal>(Geometry.Normals),
		MakeVertexAttribute<EVertexAttribute::UV>(Geometry.TexCoords))) {
		Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Failed to create GPU buffers for model: %s", ObjPath.generic_string().c_str());
		return false;
	}

	SubMeshes = std::move(ImportedSubMeshes);

	return true;
}

ID3D11Buffer* UMesh::GetVertexBuffer(EVertexAttribute Attribute) const {
	const size_t Index = GetAttributeIndex(Attribute);

	if (Index >= GetAttributeCount()) {
		return nullptr;
	}

	return VertexBuffers[Index].Get();
}

ID3D11Buffer* UMesh::GetIndexBuffer() const {
	return IndexBuffer.Get();
}

bool UMesh::HasVertexAttribute(EVertexAttribute Attribute) const {
	const size_t Index = GetAttributeIndex(Attribute);

	if (Index >= GetAttributeCount()) {
		return false;
	}

	return AttributeStorage[Index] != nullptr;
}

uint32 UMesh::GetVertexStride(EVertexAttribute Attribute) const {
	const size_t Index = GetAttributeIndex(Attribute);

	if (Index >= GetAttributeCount() || !AttributeStorage[Index]) {
		return 0;
	}

	return AttributeStorage[Index]->GetStride();
}

uint32 UMesh::GetVertexAttributeCount(EVertexAttribute Attribute) const {
	const size_t Index = GetAttributeIndex(Attribute);

	if (Index >= GetAttributeCount() || !AttributeStorage[Index]) {
		return 0;
	}

	return AttributeStorage[Index]->GetCount();
}

const void* UMesh::GetVertexData(EVertexAttribute Attribute) const {
	const size_t Index = GetAttributeIndex(Attribute);

	if (Index >= GetAttributeCount() || !AttributeStorage[Index]) {
		return nullptr;
	}

	return AttributeStorage[Index]->GetData();
}

void UMesh::Serialize(FArchive& Ar) {
	UAsset::Serialize(Ar);
}

bool UMesh::CreateIndexBuffer(ID3D11Device* Device, const std::span<const uint32>& InIndices) {
	if (Device == nullptr || InIndices.empty()) {
		return false;
	}

	const size_t ByteSize = InIndices.size_bytes(); 

	if (ByteSize > std::numeric_limits<UINT>::max()) {
		return false;
	}

	D3D11_BUFFER_DESC BufferDesc{};
	BufferDesc.ByteWidth = static_cast<UINT>(ByteSize);
	BufferDesc.Usage = D3D11_USAGE_DEFAULT;
	BufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	BufferDesc.CPUAccessFlags = 0;
	BufferDesc.MiscFlags = 0;
	BufferDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA InitialData{};
	InitialData.pSysMem = InIndices.data();

	Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;

	const HRESULT Result = Device->CreateBuffer(&BufferDesc, &InitialData, Buffer.GetAddressOf());

	if (FAILED(Result)) {
		return false;
	}

	IndexBuffer = std::move(Buffer);

	Indices.assign(InIndices.begin(), InIndices.end());

	return true;
}

void UMesh::Reset() {
	for (Microsoft::WRL::ComPtr<ID3D11Buffer>& Buffer : VertexBuffers) {
		Buffer.Reset();
	}

	for (std::unique_ptr<FVertexAttributeStorageBase>& Storage : AttributeStorage) {
		Storage.reset();
	}

	IndexBuffer.Reset();
	Indices.clear();
	SubMeshes.clear();

}
