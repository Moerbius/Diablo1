#include "focus_renderer.hpp"

#include "video.hpp"

#include <algorithm>

namespace {
constexpr double kFocusFrameTime = 0.05;
}

void FocusRenderer::Reset()
{
	animationTime_ = 0.0;
}

void FocusRenderer::Update(double dt)
{
	animationTime_ += dt;
}

double FocusRenderer::GetAnimationTime() const
{
	return animationTime_;
}

bool FocusRenderer::ExtractCurrentFrame(
	const std::vector<std::uint32_t>& focusImage,
	int focusWidth,
	int focusHeight,
	int frameCount,
	std::vector<std::uint32_t>& framePixels,
	int& frameWidth,
	int& frameHeight) const
{
	if (focusImage.empty() || focusWidth <= 0 || focusHeight <= 0 || frameCount <= 0) {
		return false;
	}

	const int frame = static_cast<int>(animationTime_ / kFocusFrameTime) % frameCount;
	int frameX = 0;
	int frameY = 0;

	if ((focusHeight % frameCount) == 0) {
		frameWidth = focusWidth;
		frameHeight = focusHeight / frameCount;
		frameY = frame * frameHeight;
	} else if ((focusWidth % frameCount) == 0) {
		frameWidth = focusWidth / frameCount;
		frameHeight = focusHeight;
		frameX = frame * frameWidth;
	} else {
		return false;
	}

	framePixels.clear();
	framePixels.reserve(frameWidth * frameHeight);
	for (int y = 0; y < frameHeight; ++y) {
		for (int x = 0; x < frameWidth; ++x) {
			const int srcX = frameX + x;
			const int srcY = frameY + y;
			framePixels.push_back(focusImage[srcY * focusWidth + srcX]);
		}
	}

	return true;
}

bool FocusRenderer::RenderPairAroundText(
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
	int markerPadding) const
{
	std::vector<std::uint32_t> framePixels;
	int frameWidth = 0;
	int frameHeight = 0;
	if (!ExtractCurrentFrame(focusImage, focusWidth, focusHeight, frameCount, framePixels, frameWidth, frameHeight)) {
		return true;
	}

	const int scaledWidth = static_cast<int>(frameWidth * uniformScale);
	const int scaledHeight = static_cast<int>(frameHeight * uniformScale);
	const int markerY = textY + ((lineHeight - scaledHeight) / 2);
	const int leftX = textX - scaledWidth - markerPadding;
	const int rightX = textX + textWidth + markerPadding;

	if (!video.RenderLogoScaled(
			framePixels.data(),
			frameWidth,
			frameHeight,
			scaledWidth,
			scaledHeight,
			leftX,
			markerY)) {
		return false;
	}

	return video.RenderLogoScaled(
		framePixels.data(),
		frameWidth,
		frameHeight,
		scaledWidth,
		scaledHeight,
		rightX,
		markerY);
}
