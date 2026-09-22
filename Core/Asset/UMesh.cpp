#include "PCH.h"
#include "UMesh.h"

#include "../Console/Console.h"

#include "FObjImporter.h"
#include "../../Serialize/FObjSerializer.h"

bool UMesh::Initialize(ID3D11Device* Device, const std::filesystem::path& SourceObjPath, const std::filesystem::path& BinaryPath, const FMaterialResolver& MaterialResolver, const FMaterialGroupResolver& MaterialGroupResolver) {
	const std::filesystem::path& AssetPath = SourceObjPath.empty() ? BinaryPath : SourceObjPath;
	if (!UAsset::Initialize(Device, AssetPath) || BinaryPath.empty()) {
		Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Model load rejected: device or asset path is invalid.");
		return false;
	}

	FObjImporter ObjImporter{};
	FGeometry Geometry{};

	std::error_code FileSystemError{};
	const bool bHasBinary = std::filesystem::is_regular_file(BinaryPath, FileSystemError);
	const bool bLoadedFromBinary = bHasBinary && FObjSerializer::LoadBinary(BinaryPath.string().c_str(), Geometry);

	if (bLoadedFromBinary) {
		Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Loaded model binary: %s", BinaryPath.generic_string().c_str());
	}
	else {
		if (SourceObjPath.empty()) {
			Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Failed to load standalone model binary: %s", BinaryPath.generic_string().c_str());
			return false;
		}  

		if (bHasBinary) {
			Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "[UMesh] Failed to load model binary; Maybe Different Version. falling back to OBJ: %s", BinaryPath.generic_string().c_str());
		}

		if (!ObjImporter.LoadObjFile(SourceObjPath.string().c_str(), Geometry)) {
			Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "[UMesh] Failed to import OBJ geometry: %s", SourceObjPath.generic_string().c_str());
			Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "[UMesh] Import Failed. Check Obj File Path : %s", SourceObjPath.generic_string().c_str());
			return false;
		}

		if (!FObjSerializer::SaveBinary(Geometry, BinaryPath.string().c_str())) {
			Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "[UMesh] Failed to create model binary: %s", BinaryPath.generic_string().c_str());
		}
		else {
			Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "[UMesh] Created model binary: %s", BinaryPath.generic_string().c_str());
		}
	}


	if (bLoadedFromBinary && Geometry.SubMeshIndexCounts.empty() && !Geometry.Indices.empty()) {
		Geometry.SubMeshIndexCounts.push_back(static_cast<uint32>(Geometry.Indices.size()));
	}

	if (Geometry.MaterialNames.empty() && Geometry.SubMeshIndexCounts.size() == 1) {
		Geometry.MaterialNames.push_back({});
	}

	FAssetHandle ImportedMaterial{};

	if (!Geometry.MaterialFileName.empty()) {
		const std::filesystem::path MaterialPath = (AssetPath.parent_path() / std::filesystem::path(Geometry.MaterialFileName.c_str())).lexically_normal();
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
		Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Model material group count does not match submesh count: %s", AssetPath.generic_string().c_str());
		return false;
	}

	uint32 FirstIndex = 0;

	for (uint32 SubMeshIndex = 0; SubMeshIndex < Geometry.SubMeshIndexCounts.size(); ++SubMeshIndex) {
		FSubMesh SubMesh{};
		SubMesh.FirstIndex = FirstIndex;
		SubMesh.IndexCount = Geometry.SubMeshIndexCounts[SubMeshIndex];

		if (SubMesh.FirstIndex > Geometry.Indices.size() || SubMesh.IndexCount > Geometry.Indices.size() - SubMesh.FirstIndex) {
			Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Model submesh index range is invalid: %s", AssetPath.generic_string().c_str());
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
					Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Model MTL group was not found; using material group 0: %s in %s", MaterialName.c_str(), AssetPath.generic_string().c_str());
				}
			}
			else {
				Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Model has no usable MTL; using material group 0: %s", AssetPath.generic_string().c_str());
			}
		}

		ImportedSubMeshes.push_back(SubMesh);
	}

	if (FirstIndex != Geometry.Indices.size() || !Make(Device, Geometry.Indices,
		MakeVertexAttribute<EVertexAttribute::Position>(Geometry.Positions),
		MakeVertexAttribute<EVertexAttribute::Normal>(Geometry.Normals),
		MakeVertexAttribute<EVertexAttribute::UV>(Geometry.TexCoords),
		MakeVertexAttribute<EVertexAttribute::Color>(Geometry.Colors))) {
		Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Failed to create GPU buffers for model: %s", AssetPath.generic_string().c_str());
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
