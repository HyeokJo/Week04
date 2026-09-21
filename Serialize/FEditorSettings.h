#pragma once

#include "FArchive.h"
#include "../Common.h"
#include "../FMath.h"
struct FEditorSettings {
	float32 MoveSensitivity = 5.0f;
	float32 RotationSensitivity = 1.0f;
	FVector CameraStartPosition = FVector(0.0f, 0.0f, 0.0f);
	float32 GridSize = 1.f;
	FString LastLoadedScenePath = "";
	uint8 ViewportLayoutPreset = 7U;
	uint32 ViewportSplitterCount = 3U;
	float32 ViewportSplitterRatio0 = 0.5f;
	float32 ViewportSplitterRatio1 = 0.5f;
	float32 ViewportSplitterRatio2 = 0.5f;



	void Serialize(FArchive& Ar) {
		Ar.Serialize("MoveSensitivity", MoveSensitivity);
		Ar.Serialize("RotationSensitivity", RotationSensitivity);
		Ar.Serialize("CameraStartPosition", CameraStartPosition);
		Ar.Serialize("GridSize", GridSize);
		Ar.Serialize("LastLoadedScenePath", LastLoadedScenePath);
		Ar.Serialize("ViewportLayoutPreset", ViewportLayoutPreset);
		Ar.Serialize("ViewportSplitterCount", ViewportSplitterCount);
		Ar.Serialize("ViewportSplitterRatio0", ViewportSplitterRatio0);
		Ar.Serialize("ViewportSplitterRatio1", ViewportSplitterRatio1);
		Ar.Serialize("ViewportSplitterRatio2", ViewportSplitterRatio2);
	}
};