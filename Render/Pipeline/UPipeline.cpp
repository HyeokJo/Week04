#include "PCH.h"
#include "UPipeline.h"

#include "../../ErrorHandler.h"

#include <memory>

#include "../../Core/Asset/FAssetMetadataParser.h"
#include "../../Externals/Include/range/v3/view/zip.hpp"

void UPipeline::Initialize(ID3D11Device* Device, const std::filesystem::path& metaData) {
	UAsset::Initialize(Device, metaData);
	Reset();

	FAssetMetadataParser MetadataParser{};
	ErrorHandler::Report(not MetadataParser.Load(AssetMetaDataPath), " [ UPipeline ]", "Failed to load metadata", ErrorHandler::EErrorLevel::Critical);

    const TFixedArray<std::filesystem::path, static_cast<size_t>(ERenderMode::Max)> ParsePath{
        MetadataParser.ResolvePath("LitFilePath"),
        MetadataParser.ResolvePath("OutlineFilePath"),
        MetadataParser.ResolvePath("UnlitFilePath"),
        MetadataParser.ResolvePath("WireframeFilePath"),
        MetadataParser.ResolvePath("LitWireframeFilePath")
    };

    Pipelines.resize(static_cast<size_t>(ERenderMode::Max));

    for (auto&& [path, pipeline] : ranges::views::zip(ParsePath, Pipelines)) {
        if (path == "")  continue;
        FPipelineDescription Description{};
        
        ErrorHandler::Report(not UPipeline::LoadPipelineDescription(path, Description), " [ UPipeline ]", "Failed to load pipeline description", ErrorHandler::EErrorLevel::Critical);

        ErrorHandler::Report(not UPipeline::Make(Device, Description, pipeline), " [ UPipeline ]", "Failed to create pipeline", ErrorHandler::EErrorLevel::Critical);

        //ErrorHandler::Report(not MetadataParser.TryGet<UINT>("StencilRef", pipeline.StencilRef), "[ UPipeline ]", "Failed to load StencilRef", ErrorHandler::EErrorLevel::Critical);
    }
    ErrorHandler::Report(not MetadataParser.TryGet<size_t>("Primary", PrimaryIndex), "[ UPipeline ]", "Failed to load primary Index", ErrorHandler::EErrorLevel::Critical);

	ModeIndex = PrimaryIndex;
}

bool UPipeline::InitializeFromFile(ID3D11Device* Device, const std::filesystem::path& PipelinePath) {
	if (Device == nullptr || PipelinePath.empty()) {
		return false;
	}

	UAsset::Initialize(Device, PipelinePath);
	Reset();

	FPipelineDescription Description{};
	if (!LoadPipelineDescription(PipelinePath, Description)) {
		return false;
	}

	Pipelines.emplace_back();
	if (!Make(Device, Description, Pipelines.back())) {
		Reset();
		return false;
	}

	OptionFilePath = PipelinePath;
	PrimaryIndex = 0;
	ModeIndex = 0;

	return true;
}

bool UPipeline::InitializeFromFamilyDirectory(ID3D11Device* Device, const std::filesystem::path& FamilyDirectory) {
	if (Device == nullptr || FamilyDirectory.empty() || !std::filesystem::is_directory(FamilyDirectory)) {
		return false;
	}

	std::vector<std::filesystem::path> UnitPaths{};
	const FString FamilyName = FamilyDirectory.filename().generic_string().c_str();
	const FString UnitPrefix = FamilyName + "_";
	std::error_code ErrorCode{};
	for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(FamilyDirectory, ErrorCode)) {
		if (ErrorCode) {
			return false;
		}

		const std::filesystem::path& Path = Entry.path();
		if (Entry.is_regular_file(ErrorCode) && Path.extension() == ".json" && Path.stem().generic_string().starts_with(UnitPrefix)) {
			UnitPaths.emplace_back(Path);
		}
	}

	if (ErrorCode || UnitPaths.empty()) {
		return false;
	}

	std::ranges::sort(UnitPaths, {}, [](const std::filesystem::path& Path) {
		return Path.filename().generic_string();
	});

	UAsset::Initialize(Device, FamilyDirectory);
	Reset();

	for (const std::filesystem::path& UnitPath : UnitPaths) {
		FPipelineDescription Description{};
		Pipelines.emplace_back();
		if (!LoadPipelineDescription(UnitPath, Description) || !Make(Device, Description, Pipelines.back())) {
			Reset();
			return false;
		}
	}

	OptionFilePath = FamilyDirectory;
	PrimaryIndex = 0;
	ModeIndex = 0;

	return true;
}


bool UPipeline::Make(ID3D11Device* Device, const FPipelineDescription& Description, PipelineUnit& Pipeline) {
    if (Device == nullptr) {
        ErrorHandler::Report("Pipeline::Initialize", "A valid Direct3D device is required to initialize a pipeline.", ErrorHandler::EErrorLevel::Error);
        return false;
    }

    // Reset();

    if (!Pipeline.VertexShader.Initialize(Device, Description.VertexShader)) {
        return false;
    }

    if (!Pipeline.PixelShader.Initialize(Device, Description.PixelShader)) {
        return false;
    }

    if (Description.bHasGeometryShader)
    {
        if (!Pipeline.GeometryShader.Initialize(Device,Description.GeometryShader))
        {
            return false;
        }
    }

    std::vector<D3D11_INPUT_ELEMENT_DESC> NativeInputLayout;
    NativeInputLayout.reserve(Description.InputLayout.size());

    for (const FInputElementDescription& Source : Description.InputLayout) {
        D3D11_INPUT_ELEMENT_DESC Element{};
        Element.SemanticName = Source.SemanticName.c_str();
        Element.SemanticIndex = Source.SemanticIndex;
        Element.Format = ConvertVertexFormat(Source.Format);
        Element.InputSlot = Source.InputSlot;
        Element.AlignedByteOffset = Source.AlignedByteOffset;
        Element.InputSlotClass = Source.InputClassification == EInputClassification::PerInstance ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
        Element.InstanceDataStepRate = Source.InstanceDataStepRate;

        NativeInputLayout.emplace_back(Element);
    }

    HRESULT Result = S_OK;
    if (!NativeInputLayout.empty()) {
        Result = Device->CreateInputLayout(NativeInputLayout.data(), static_cast<UINT>(NativeInputLayout.size()), Pipeline.VertexShader.GetByteCodeData(), Pipeline.VertexShader.GetByteCodeSize(), Pipeline.InputLayout.GetAddressOf());

        if (FAILED(Result)) {
            ErrorHandler::ReportHRESULT(Result, "Pipeline::Initialize", "Failed to create the input layout.", ErrorHandler::EErrorLevel::Error);
            Reset();
            return false;
        }
    } else {
        Pipeline.InputLayout.Reset();
    }

    D3D11_RASTERIZER_DESC RasterizerDesc{};
    RasterizerDesc.FillMode = ConvertFillMode(Description.Rasterizer.FillMode);
    RasterizerDesc.CullMode = ConvertCullMode(Description.Rasterizer.CullMode);
    RasterizerDesc.FrontCounterClockwise = Description.Rasterizer.FrontCounterClockwise;
    RasterizerDesc.DepthClipEnable = Description.Rasterizer.DepthClipEnable;
    RasterizerDesc.ScissorEnable = Description.Rasterizer.ScissorEnable;

    Result = Device->CreateRasterizerState(&RasterizerDesc, Pipeline.RasterizerState.GetAddressOf());

    if (FAILED(Result)) {
        ErrorHandler::ReportHRESULT(Result, "Pipeline::Initialize", "Failed to create the rasterizer state.", ErrorHandler::EErrorLevel::Error);
        Reset();
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC DepthStencilDesc{};
    DepthStencilDesc.DepthEnable = Description.DepthStencil.DepthEnable;
    DepthStencilDesc.DepthWriteMask = Description.DepthStencil.DepthWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    DepthStencilDesc.DepthFunc = ConvertCompareFunc(Description.DepthStencil.DepthFunc);
    DepthStencilDesc.StencilEnable = Description.DepthStencil.StencilEnable;;
    DepthStencilDesc.FrontFace.StencilFunc = ConvertCompareFunc(Description.DepthStencil.StencilFunc);
    DepthStencilDesc.FrontFace.StencilPassOp = ConvertStencillOp(Description.DepthStencil.StencilPassOp);
    DepthStencilDesc.FrontFace.StencilFailOp = ConvertStencillOp(Description.DepthStencil.StencilFailOp);
    DepthStencilDesc.FrontFace.StencilDepthFailOp = ConvertStencillOp(Description.DepthStencil.StencilDepthFailOp);

    DepthStencilDesc.BackFace = DepthStencilDesc.FrontFace;

    Result = Device->CreateDepthStencilState(&DepthStencilDesc, Pipeline.DepthStencilState.GetAddressOf());

    if (FAILED(Result)) {
        ErrorHandler::ReportHRESULT(Result, "Pipeline::Initialize", "Failed to create the depth-stencil state.", ErrorHandler::EErrorLevel::Error);
        Reset();
        return false;
    }

    D3D11_BLEND_DESC BlendDesc{};
    BlendDesc.AlphaToCoverageEnable = false;
    BlendDesc.IndependentBlendEnable = false;

    D3D11_RENDER_TARGET_BLEND_DESC& RenderTarget = BlendDesc.RenderTarget[0];
    RenderTarget.BlendEnable = Description.Blend.BlendEnable;
    RenderTarget.SrcBlend = ConvertBlend(Description.Blend.SrcBlend);
    RenderTarget.DestBlend = ConvertBlend(Description.Blend.DestBlend);
    RenderTarget.BlendOp = ConvertBlendOp(Description.Blend.BlendOp);
    RenderTarget.SrcBlendAlpha = ConvertBlend(Description.Blend.SrcBlendAlpha);
    RenderTarget.DestBlendAlpha = ConvertBlend(Description.Blend.DestBlendAlpha);
    RenderTarget.BlendOpAlpha = ConvertBlendOp(Description.Blend.BlendOpAlpha);
    RenderTarget.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    Result = Device->CreateBlendState(&BlendDesc, Pipeline.BlendState.GetAddressOf());

    if (FAILED(Result)) {
        ErrorHandler::ReportHRESULT(Result, "Pipeline::Initialize", "Failed to create the blend state.", ErrorHandler::EErrorLevel::Error);
        Reset();
        return false;
    }

    Pipeline.PrimitiveTopology = ConvertPrimitiveTopology(Description.PrimitiveTopology);

    Pipeline.Initialized = true;
    return true;
}

void UPipeline::Bind(ID3D11DeviceContext* Context) const {
    if (Context == nullptr) {
        ErrorHandler::Report("Pipeline::Bind", "A valid Direct3D device context is required to bind a pipeline.", ErrorHandler::EErrorLevel::Error);
        return;
    }

    if (ModeIndex >= Pipelines.size() || !Pipelines[ModeIndex].Initialized) {
        return;
    }

    Context->IASetInputLayout(Pipelines[ModeIndex].InputLayout.Get());
    Context->IASetPrimitiveTopology(Pipelines[ModeIndex].PrimitiveTopology);

    Context->VSSetShader(Pipelines[ModeIndex].VertexShader.GetVertexShader(), nullptr, 0);
    Context->PSSetShader(Pipelines[ModeIndex].PixelShader.GetPixelShader(), nullptr, 0);

    Context->GSSetShader(Pipelines[ModeIndex].GeometryShader.GetGeometryShader(), nullptr, 0);
    Context->HSSetShader(nullptr, nullptr, 0);
    Context->DSSetShader(nullptr, nullptr, 0);

    Context->RSSetState(Pipelines[ModeIndex].RasterizerState.Get());
    Context->OMSetBlendState(Pipelines[ModeIndex].BlendState.Get(), nullptr, 0xffffffff);
    Context->OMSetDepthStencilState(Pipelines[ModeIndex].DepthStencilState.Get(), 1);
}

void UPipeline::Reset() {
    for (auto& pipelines : Pipelines) {
        pipelines.VertexShader.Reset();
        pipelines.PixelShader.Reset();
        pipelines.GeometryShader.Reset();

        pipelines.InputLayout.Reset();
        pipelines.RasterizerState.Reset();
        pipelines.BlendState.Reset();
		pipelines.DepthStencilState.Reset();

		pipelines.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		pipelines.Initialized = false;
	}
	Pipelines.clear();
	PrimaryIndex = 0;
	ModeIndex = 0;
}

void UPipeline::SetRenderMode(ERenderMode mode)
{
    const size_t RequestedIndex = static_cast<size_t>(mode);
    if (RequestedIndex < Pipelines.size() && Pipelines[RequestedIndex].Initialized) {
        ModeIndex = RequestedIndex;
    }
    else {
        ModeIndex = PrimaryIndex;
    }
}

bool UPipeline::RenderModeSettable(ERenderMode mode)
{
    const size_t RequestedIndex = static_cast<size_t>(mode);
    return RequestedIndex < Pipelines.size() && Pipelines[RequestedIndex].Initialized;
}

bool UPipeline::LoadPipelineDescription(const std::filesystem::path& Path, FPipelineDescription& OutDescription) {
    FILE* File = nullptr;

#ifdef _WIN32
    _wfopen_s(&File, Path.c_str(), L"rb");
#else
    File = std::fopen(Path.string().c_str(), "rb");
#endif

    if (File == nullptr) {
        ErrorHandler::Report("Pipeline::LoadPipelineDescription", "Failed to open the pipeline option file.", ErrorHandler::EErrorLevel::Critical);
        return false;
    }

	std::unique_ptr<char[]> ReadBufferPtr(new char[65536]);
    rapidjson::FileReadStream Stream(File, ReadBufferPtr.get(), sizeof(ReadBufferPtr.get()));

    rapidjson::Document Root;
    Root.ParseStream(Stream);

    std::fclose(File);

    if (Root.HasParseError() || !Root.IsObject()) {
        ErrorHandler::Report("Pipeline::LoadPipelineDescription", "Failed to parse the pipeline option file as a JSON object.", ErrorHandler::EErrorLevel::Error);
        return false;
    }

    FPipelineDescription Description;

    const rapidjson::Value* VS = GetObject(Root, "VertexShader");

    if (VS == nullptr) {
        return false;
    }

    const char* VSSource = GetString(*VS, "Source");
    const char* VSEntryPoint = GetString(*VS, "EntryPoint");
    const char* VSProfile = GetString(*VS, "Profile");

    if (VSSource == nullptr || VSEntryPoint == nullptr || VSProfile == nullptr) {
        return false;
    }

    Description.VertexShader.Source = VSSource;
    Description.VertexShader.EntryPoint = VSEntryPoint;
    Description.VertexShader.Profile = VSProfile;
    Description.VertexShader.Stage = EShaderStage::Vertex;

    const rapidjson::Value* PS = GetObject(Root, "PixelShader");

    if (PS == nullptr) {
        return false;
    }

    const char* PSSource = GetString(*PS, "Source");
    const char* PSEntryPoint = GetString(*PS, "EntryPoint");
    const char* PSProfile = GetString(*PS, "Profile");

    if (PSSource == nullptr || PSEntryPoint == nullptr || PSProfile == nullptr) {
        return false;
    }

    Description.PixelShader.Source = PSSource;
    Description.PixelShader.EntryPoint = PSEntryPoint;
    Description.PixelShader.Profile = PSProfile;
    Description.PixelShader.Stage = EShaderStage::Pixel;

    if (Root.HasMember("GeometryShader"))
    {
        const rapidjson::Value& GS = Root["GeometryShader"];

        if (!GS.IsObject())
        {
            return false;
        }

        const char* GSSource = GetString(GS, "Source");
        const char* GSEntryPoint = GetString(GS, "EntryPoint");
        const char* GSProfile = GetString(GS, "Profile");

        if (GSSource == nullptr || GSEntryPoint == nullptr || GSProfile == nullptr)
        {
            return false;
        }

        Description.GeometryShader.Source = GSSource;
        Description.GeometryShader.EntryPoint = GSEntryPoint;
        Description.GeometryShader.Profile = GSProfile;
        Description.GeometryShader.Stage = EShaderStage::Geometry;
        Description.bHasGeometryShader = true;
    }

    const rapidjson::Value* InputLayout = GetArray(Root, "InputLayout");

    if (InputLayout == nullptr) {
        return false;
    }

    Description.InputLayout.reserve(InputLayout->Size());

    for (const rapidjson::Value& Element : InputLayout->GetArray()) {
        if (!Element.IsObject()) {
            ErrorHandler::Report("Pipeline::LoadPipelineDescription", "Each input-layout element must be a JSON object.", ErrorHandler::EErrorLevel::Error);
            return false;
        }

        const char* SemanticName = GetString(Element, "SemanticName");
        const char* Format = GetString(Element, "Format");

        if (SemanticName == nullptr || Format == nullptr) {
            return false;
        }

        FInputElementDescription Input;
        Input.SemanticName = SemanticName;
        Input.SemanticIndex = GetUint(Element, "SemanticIndex", 0);
        Input.Format = ParseVertexFormat(Format);
        Input.InputSlot = GetUint(Element, "InputSlot", 0);
        Input.AlignedByteOffset = GetUint(Element, "AlignedByteOffset");
        Input.InputClassification = std::strcmp(GetString(Element, "InputClassification", "PerVertex"), "PerInstance") == 0 ? EInputClassification::PerInstance : EInputClassification::PerVertex;
        Input.InstanceDataStepRate = GetUint(Element, "InstanceDataStepRate", 0);

        Description.InputLayout.emplace_back(std::move(Input));
    }

    const char* PrimitiveTopology = GetString(Root, "PrimitiveTopology");

    if (PrimitiveTopology == nullptr) {
        return false;
    }

    Description.PrimitiveTopology = ParsePrimitiveTopology(PrimitiveTopology);

    const rapidjson::Value* Rasterizer = GetObject(Root, "Rasterizer");

    if (Rasterizer == nullptr) {
        return false;
    }

    const char* FillMode = GetString(*Rasterizer, "FillMode");
    const char* CullMode = GetString(*Rasterizer, "CullMode");

    if (FillMode == nullptr || CullMode == nullptr) {
        return false;
    }

    Description.Rasterizer.FillMode = ParseFillMode(FillMode);
    Description.Rasterizer.CullMode = ParseCullMode(CullMode);
    Description.Rasterizer.FrontCounterClockwise = GetBool(*Rasterizer, "FrontCounterClockwise", false);
    Description.Rasterizer.DepthClipEnable = GetBool(*Rasterizer, "DepthClipEnable", true);
    Description.Rasterizer.ScissorEnable = GetBool(*Rasterizer, "ScissorEnable", false);

    const rapidjson::Value* DepthStencil = GetObject(Root, "DepthStencil");

    if (DepthStencil == nullptr) {
        return false;
    }

    Description.DepthStencil.DepthEnable = GetBool(*DepthStencil, "DepthEnable", true);
    Description.DepthStencil.DepthWriteEnable = GetBool(*DepthStencil, "DepthWriteEnable", true);
    Description.DepthStencil.DepthFunc = ParseCompareFunc(GetString(*DepthStencil, "DepthFunc", "LessEqual"));

    const rapidjson::Value* Blend = GetObject(Root, "Blend");

    if (Blend == nullptr) {
        return false;
    }

    Description.Blend.BlendEnable = GetBool(*Blend, "BlendEnable", false);
    Description.Blend.SrcBlend = ParseBlend(GetString(*Blend, "SrcBlend", "One"));
    Description.Blend.DestBlend = ParseBlend(GetString(*Blend, "DestBlend", "Zero"));
    Description.Blend.BlendOp = ParseBlendOp(GetString(*Blend, "BlendOp", "Add"));
    Description.Blend.SrcBlendAlpha = ParseBlend(GetString(*Blend, "SrcBlendAlpha", "One"));
    Description.Blend.DestBlendAlpha = ParseBlend(GetString(*Blend, "DestBlendAlpha", "Zero"));
    Description.Blend.BlendOpAlpha = ParseBlendOp(GetString(*Blend, "BlendOpAlpha", "Add"));

    OutDescription = std::move(Description);

    return true;
}

void UPipeline::Serialize(FArchive& Ar) {
	UAsset::Serialize(Ar);
}

