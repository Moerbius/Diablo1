#pragma once

#include <cstdint>
#include <vector>

class Video;

class FocusRenderer {
public:
	void Reset();
	void Update(double dt);
	double GetAnimationTime() const;

	bool RenderPairAroundText(
		Video& video,
		const std::vector<std::uint32_t>& focusImage,
		int focusWidth,
		int focusHeight,
		int frameCount,
		double uniformScale,
		int textX,
		int textY,
		int textWidth,
		int lineHeight,
		int markerPadding) const;

private:
	bool ExtractCurrentFrame(
		const std::vector<std::uint32_t>& focusImage,
		int focusWidth,
		int focusHeight,
		int frameCount,
		std::vector<std::uint32_t>& framePixels,
		int& frameWidth,
		int& frameHeight) const;

	double animationTime_ = 0.0;
};
