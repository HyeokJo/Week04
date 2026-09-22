#pragma once

#include "FArchive.h"
#include "../Common.h"
#include "../FMath.h"
struct FEditorSettings {
	float32 MoveSensitivity{ 5.0f };
	float32 RotationSensitivity{ 1.0f };
	FVector CameraStartPosition{ 0.0f, 0.0f, 0.0f };
	float32 GridSize{ 1.0f };
	bool mGridVisible{ true };
	bool mGridSnapEnabled{ true };
	bool mAxisVisible{ true };
	FString LastLoadedScenePath{};
	uint8 ViewportLayoutPreset{ 7U };
	uint32 ViewportSplitterCount{ 3U };
	float32 ViewportSplitterRatio0{ 0.5f };
	float32 ViewportSplitterRatio1{ 0.5f };
	float32 ViewportSplitterRatio2{ 0.5f };
	void Serialize(FArchive& Ar);
};
