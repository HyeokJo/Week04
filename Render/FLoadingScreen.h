#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#include <Windows.h>

class FRenderer;

class FLoadingProgress {
public:
	FLoadingProgress();
	~FLoadingProgress();

public:
	void SetProgress(float Progress, const std::string& Status);
	float GetProgress() const;
	std::string GetStatus() const;

private:
	std::atomic<float> mProgress{};
	mutable std::mutex mStatusMutex{};
	std::string mStatus{};
};

class FLoadingScreen {
public:
	using FLoadingTask = std::function<bool(FLoadingProgress&)>;

public:
	FLoadingScreen();
	~FLoadingScreen();

public:
	bool Run(FRenderer& Renderer, HACCEL AcceleratorTable, const FLoadingTask& LoadingTask);

private:
	void Render(FRenderer& Renderer, const FLoadingProgress& Progress);
};
