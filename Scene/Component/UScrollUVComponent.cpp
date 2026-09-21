#include "PCH.h"
#include "UScrollUVComponent.h"
#include "Render/Panel/FPropertyEditorContext.h"

#include "../../Serialize/FArchive.h"

void UScrollUVComponent::Tick(float DeltaTime)
{
	UBillboardComponent::Tick(DeltaTime);

	if (!bPlaying)
	{
		return;
	}

	CurrentOffset += ScrollSpeed * DeltaTime;

	if (bLooping)
	{
		// 누적값이 커지면 float 정밀도가 깨져 UV가 계단진다. 소수부만 남긴다.
		CurrentOffset.x -= std::floor(CurrentOffset.x);
		CurrentOffset.y -= std::floor(CurrentOffset.y);
	}

	UpdateUVFromCurrentFrame();
}

void UScrollUVComponent::UpdateUVFromCurrentFrame()
{
	// UV 창 전체를 오프셋만큼 민다. 0~1 을 벗어나는 부분은 Wrap 샘플러가 처리한다.
	UBillboardComponent::SetUV(CurrentOffset, CurrentOffset + FVector2{ 1.0f, 1.0f });
}

void UScrollUVComponent::DrawPanels(FPropertyEditorContext& Context)
{
	UBillboardComponent::DrawPanels(Context);


	Context.DrawVector2("ScrollSpeed", ScrollSpeed, 0.01f, -5.0f, 5.0f, [this](FVector2 NewSpeed) { SetScrollSpeed(NewSpeed);});

}

void UScrollUVComponent::Serialize(FArchive& Archive)
{
	UBillboardComponent::Serialize(Archive);

	Archive.Serialize("ScrollSpeed", ScrollSpeed);
	Archive.Serialize("bLooping", bLooping);
	Archive.Serialize("bPlaying", bPlaying);

	if (Archive.IsLoading())
	{
		CurrentOffset = FVector2{ 0.0f, 0.0f };
		UpdateUVFromCurrentFrame();
	}
}
