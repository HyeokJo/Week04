#include "PCH.h"

#include "FLineRenderer.h"

#include "../../ErrorHandler.h"

#include <array>
#include <limits>

void FLineRenderer::Initialize(ID3D11Device* InDevice, uint32 InitialLineCapacity) {

	ErrorHandler::Report(InDevice == nullptr, "[ FLineRenderer ]", "Invalid device pointer.", ErrorHandler::EErrorLevel::Critical);
	ErrorHandler::Report(InitialLineCapacity == 0, "[ FLineRenderer ]", "Initial line capacity must be greater than zero.", ErrorHandler::EErrorLevel::Critical);

	Reset();

	Device = InDevice;
	DepthTestedPipeline = std::make_unique<UPipeline>();
	OverlayPipeline = std::make_unique<UPipeline>();

	ErrorHandler::Report(!DepthTestedPipeline->InitializeFromFile(Device, "./Content/Pipeline/LineDepthTested.json"), "[ FLineRenderer ]", "Failed to initialize the depth-tested line pipeline.", ErrorHandler::EErrorLevel::Critical);
	ErrorHandler::Report(!OverlayPipeline->InitializeFromFile(Device, "./Content/Pipeline/LineOverlay.json"), "[ FLineRenderer ]", "Failed to initialize the overlay line pipeline.", ErrorHandler::EErrorLevel::Critical);

	InitialLineCapacity = std::max(InitialLineCapacity, 1u);

	ErrorHandler::Report(not CreateQuadGeometry(Device), "[ FLineRenderer ]", "Failed to create quad geometry.", ErrorHandler::EErrorLevel::Critical);
	ErrorHandler::Report(not CreateInstanceBuffer(Device, DepthTestedBatch, InitialLineCapacity), "[ FLineRenderer ]", "Failed to create instance buffer for depth-tested lines.", ErrorHandler::EErrorLevel::Critical);
	ErrorHandler::Report(not CreateInstanceBuffer(Device, OverlayBatch, InitialLineCapacity), "[ FLineRenderer ]", "Failed to create instance buffer for overlay lines.", ErrorHandler::EErrorLevel::Critical);
	ErrorHandler::Report(not FrameConstants.Initialize(Device), "[ FLineRenderer ]", "Failed to initialize frame constants.", ErrorHandler::EErrorLevel::Critical);


	DepthTestedBatch.Instances.reserve(InitialLineCapacity);
	OverlayBatch.Instances.reserve(InitialLineCapacity);
}

void FLineRenderer::Reset() {
	QuadVertexBuffer.Reset();
	QuadIndexBuffer.Reset();

	DepthTestedBatch.InstanceBuffer.Reset();
	DepthTestedBatch.Instances.clear();
	DepthTestedBatch.Capacity = 0;

	OverlayBatch.InstanceBuffer.Reset();
	OverlayBatch.Instances.clear();
	OverlayBatch.Capacity = 0;

	FrameConstants.Reset();

	DepthTestedPipeline = nullptr;
	OverlayPipeline = nullptr;
	Device = nullptr;
}

void FLineRenderer::AddLine(const FVector3& Start, const FVector3& End, const FVector4& Color, float WidthPixels, ELineDepthMode DepthMode) {
	if (WidthPixels <= 0.0f || (End - Start).LengthSquared() <= 0.0f) {
		return;
	}

	FLineBatch& Batch = DepthMode == ELineDepthMode::DepthTested ? DepthTestedBatch : OverlayBatch;

	Batch.Instances.emplace_back(FLineInstance{
		.StartAndWidth = FVector4{ Start.x, Start.y, Start.z, WidthPixels },
		.EndAndPadding = FVector4{ End.x, End.y, End.z, 0.0f },
		.Color = Color
	});
}

void FLineRenderer::AddRay(const FVector3& Origin, const FVector3& Direction, float Length, const FVector4& Color, float WidthPixels, ELineDepthMode DepthMode) {
	if (Length <= 0.0f || Direction.LengthSquared() <= 0.0f) {
		return;
	}

	FVector3 NormalizedDirection = Direction;
	NormalizedDirection.Normalize();

	AddLine(Origin, Origin + NormalizedDirection * Length, Color, WidthPixels, DepthMode);
}

void FLineRenderer::Render(ID3D11DeviceContext* Context, const FLineViewData& ViewData) {
	ErrorHandler::Report(Device == nullptr, "[ FLineRenderer ]", "Invalid device pointer.", ErrorHandler::EErrorLevel::Critical);
	ErrorHandler::Report(Context == nullptr, "[ FLineRenderer ]", "Invalid device context pointer.", ErrorHandler::EErrorLevel::Critical);
	ErrorHandler::Report(ViewData.ViewportSize.x <= 0.0f || ViewData.ViewportSize.y <= 0.0f, "[ FLineRenderer ]", "Invalid viewport size.", ErrorHandler::EErrorLevel::Critical);
	ErrorHandler::Report(DepthTestedPipeline == nullptr, "[ FLineRenderer ]", "Depth-tested pipeline is not initialized.", ErrorHandler::EErrorLevel::Critical);
	
	const FLineFrameConstants Constants{
		.ViewProjection = ViewData.ViewProjection,
		.Viewport = FVector4{
			ViewData.ViewportSize.x,
			ViewData.ViewportSize.y,
			1.0f / ViewData.ViewportSize.x,
			1.0f / ViewData.ViewportSize.y
		}
	};

	ErrorHandler::Report(not FrameConstants.SetGraphicsRoot32BitConstants(Constants), "[ FLineRenderer ]", "Failed to set frame constants.", ErrorHandler::EErrorLevel::Critical);
	ErrorHandler::Report(not FrameConstants.Bind(Context, 0, EGraphicsShaderStage::Vertex), "[ FLineRenderer ]", "Failed to bind frame constants.", ErrorHandler::EErrorLevel::Critical);

	ErrorHandler::Report(not RenderBatch(Device, Context, DepthTestedBatch, DepthTestedPipeline.get()), "[ FLineRenderer ]", "Failed to render depth-tested lines.", ErrorHandler::EErrorLevel::Critical);
	ErrorHandler::Report(not RenderBatch(Device, Context, OverlayBatch, OverlayPipeline.get()), "[ FLineRenderer ]", "Failed to render overlay lines.", ErrorHandler::EErrorLevel::Critical);

	Clear();
}

void FLineRenderer::Clear() {
	DepthTestedBatch.Instances.clear();
	OverlayBatch.Instances.clear();
}

uint32 FLineRenderer::GetLineCount() const {
	return static_cast<uint32>(DepthTestedBatch.Instances.size() + OverlayBatch.Instances.size());
}

bool FLineRenderer::IsEmpty() const {
	return DepthTestedBatch.Instances.empty() && OverlayBatch.Instances.empty();
}

bool FLineRenderer::CreateQuadGeometry(ID3D11Device* InDevice) {
	constexpr std::array<FQuadVertex, 4> Vertices{
		FQuadVertex{ FVector2D{ 0.0f, -1.0f } },
		FQuadVertex{ FVector2D{ 0.0f, 1.0f } },
		FQuadVertex{ FVector2D{ 1.0f, -1.0f } },
		FQuadVertex{ FVector2D{ 1.0f, 1.0f } }
	};

	constexpr std::array<uint16, 6> Indices{ 0, 1, 2, 2, 1, 3 };

	FGraphicsBufferDescription VertexBufferDescription{};
	VertexBufferDescription.ByteSize = static_cast<uint32>(sizeof(Vertices));
	VertexBufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
	VertexBufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	if (!QuadVertexBuffer.Initialize(InDevice, VertexBufferDescription, Vertices.data())) {
		return false;
	}

	FGraphicsBufferDescription IndexBufferDescription{};
	IndexBufferDescription.ByteSize = static_cast<uint32>(sizeof(Indices));
	IndexBufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
	IndexBufferDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;

	return QuadIndexBuffer.Initialize(InDevice, IndexBufferDescription, Indices.data());
}

bool FLineRenderer::CreateInstanceBuffer(ID3D11Device* InDevice, FLineBatch& Batch, uint32 Capacity) {
	if (InDevice == nullptr || Capacity == 0 || Capacity > std::numeric_limits<uint32>::max() / sizeof(FLineInstance)) {
		return false;
	}

	FGraphicsBufferDescription Description{};
	Description.ByteSize = static_cast<uint32>(Capacity * sizeof(FLineInstance));
	Description.Usage = D3D11_USAGE_DYNAMIC;
	Description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	Description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	FGraphicsBuffer NewBuffer{};

	if (!NewBuffer.Initialize(InDevice, Description)) {
		return false;
	}

	Batch.InstanceBuffer = std::move(NewBuffer);
	Batch.Capacity = Capacity;

	return true;
}

bool FLineRenderer::EnsureCapacity(ID3D11Device* InDevice, FLineBatch& Batch, uint32 RequiredCapacity) {
	if (RequiredCapacity <= Batch.Capacity) {
		return true;
	}

	uint32 NewCapacity = std::max(Batch.Capacity, 1u);

	while (NewCapacity < RequiredCapacity) {
		if (NewCapacity > std::numeric_limits<uint32>::max() / 2) {
			NewCapacity = RequiredCapacity;
			break;
		}

		NewCapacity *= 2;
	}

	return CreateInstanceBuffer(InDevice, Batch, NewCapacity);
}

bool FLineRenderer::RenderBatch(ID3D11Device* InDevice, ID3D11DeviceContext* Context, FLineBatch& Batch, const UPipeline* Pipeline) {
	if (Batch.Instances.empty()) {
		return true;
	}

	if (InDevice == nullptr or Context == nullptr or Pipeline == nullptr) {
		return false;
	}

	const uint32 InstanceCount = static_cast<uint32>(Batch.Instances.size());
	const uint32 InstanceByteSize = static_cast<uint32>(InstanceCount * sizeof(FLineInstance));

	if (not EnsureCapacity(InDevice, Batch, InstanceCount) or /* -> */ not Batch.InstanceBuffer.WriteDiscard(Context, Batch.Instances.data(), InstanceByteSize)) {
		return false;
	}

	Pipeline->Bind(Context);

	ID3D11Buffer* VertexBuffers[]{
		QuadVertexBuffer.GetBuffer(),
		Batch.InstanceBuffer.GetBuffer()
	};

	const uint32 Strides[]{
		static_cast<uint32>(sizeof(FQuadVertex)),
		static_cast<uint32>(sizeof(FLineInstance))
	};

	constexpr uint32 Offsets[]{ 0, 0 };

	Context->IASetVertexBuffers(0, _countof(VertexBuffers), VertexBuffers, Strides, Offsets);
	Context->IASetIndexBuffer(QuadIndexBuffer.GetBuffer(), DXGI_FORMAT_R16_UINT, 0);
	Context->DrawIndexedInstanced(6, InstanceCount, 0, 0, 0);

	return true;
}
