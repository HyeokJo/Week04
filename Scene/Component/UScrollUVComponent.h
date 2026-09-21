#pragma once
#include "UBillboardComponent.h"

class FPropertyEditorContext;
class FArchive;

class UScrollUVComponent final : public UBillboardComponent
{
	

public:
	UScrollUVComponent() = default;
	~UScrollUVComponent() = default;

	JG_DECLARE_DERIVED_TYPEINFO(UScrollUVComponent, UBillboardComponent);

	UScrollUVComponent(const UScrollUVComponent&) = delete;
	UScrollUVComponent& operator=(const UScrollUVComponent&) = delete;

	UScrollUVComponent(const UScrollUVComponent&&) = delete;
	UScrollUVComponent& operator=(const UScrollUVComponent&&) = delete;


	 bool IsPlaying() const { return bPlaying; }
	 bool IsLooping() const { return bLooping; }
	 void PlayScrollUV() { bPlaying = true; }
	 void PauseScrollUV() { bPlaying = false; }
	 void SetScrollSpeed(FVector2 InScrollSpeed) { ScrollSpeed.x = InScrollSpeed.x , ScrollSpeed.y = InScrollSpeed.y;}

	void Tick(float DeltaTime) override;
	void DrawPanels(FPropertyEditorContext& Context) override;
	void UpdateUVFromCurrentFrame();

protected:
	void Serialize(FArchive& Archive) override;
private:
	FVector2 ScrollSpeed{ 0.1f, 0.1f };   // 초당 UV 이동량
	FVector2 CurrentOffset{ 0.0f, 0.0f }; // 누적값
	bool bPlaying = true;
	bool bLooping = true;
};