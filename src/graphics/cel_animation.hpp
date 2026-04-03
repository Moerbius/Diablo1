#pragma once

#include <cstdint>
#include <vector>

class CELAnimation {
public:
	void SetFrames(std::vector<std::vector<std::uint32_t>> frames, int width, int height,
		double frameDurationSeconds, bool loop = true);
	void Clear();
	void Reset();
	void Update(double dt);

	bool HasFrame() const;
	const std::vector<std::uint32_t>& GetCurrentFrame() const;
	int GetWidth() const;
	int GetHeight() const;

private:
	std::vector<std::vector<std::uint32_t>> frames_;
	double frameDurationSeconds_ = 0.0;
	double frameTimeSeconds_ = 0.0;
	int width_ = 0;
	int height_ = 0;
	std::size_t currentFrameIndex_ = 0;
	bool loop_ = true;
};