#include "logo_renderer.hpp"

#include "video.hpp"

#include <algorithm>

namespace {
constexpr int kLogoFrameCount = 15;
constexpr double kLogoFrameTime = 0.05;
}

void LogoRenderer::Reset()
{
	animationTime_ = 0.0;
	currentFrame_ = 0;
}

void LogoRenderer::Update(double dt)
{
	animationTime_ += dt;
	currentFrame_ = static_cast<int>(animationTime_ / kLogoFrameTime) % kLogoFrameCount;
}

double LogoRenderer::GetAnimationTime() const
{
	return animationTime_;
}

bool LogoRenderer::BuildCurrentFrame(
	const std::vector<std::uint32_t>& logoImage,
	int logoWidth,
	int logoHeight,
	std::vector<std::uint32_t>& framePixels,
	int& frameHeight) const
{
	if (logoImage.empty() || logoWidth <= 0 || logoHeight <= 0) {
		return false;
	}

	frameHeight = logoHeight / kLogoFrameCount;
	if (frameHeight <= 0) {
		return false;
	}

	const int frameY = currentFrame_ * frameHeight;
	framePixels.clear();
	framePixels.reserve(logoWidth * frameHeight);

	for (int y = 0; y < frameHeight; ++y) {
		for (int x = 0; x < logoWidth; ++x) {
			framePixels.push_back(logoImage[(frameY + y) * logoWidth + x]);
		}
	}

	return true;
}

bool LogoRenderer::RenderTopCentered(
	Video& video,
	const std::vector<std::uint32_t>& logoImage,
	int logoWidth,
	int logoHeight,
	double uniformScale,
	double logoScale,
	int renderX,
	int renderY,
	int renderWidth,
	int* outScaledLogoHeight) const
{
	std::vector<std::uint32_t> framePixels;
	int frameHeight = 0;
	if (!BuildCurrentFrame(logoImage, logoWidth, logoHeight, framePixels, frameHeight)) {
		return true;
	}

	const int scaledLogoWidth = static_cast<int>(logoWidth * uniformScale * logoScale);
	const int scaledLogoHeight = static_cast<int>(frameHeight * uniformScale * logoScale);
	const int logoX = renderX + ((renderWidth - scaledLogoWidth) / 2);
	const int logoY = renderY;

	if (outScaledLogoHeight != nullptr) {
		*outScaledLogoHeight = scaledLogoHeight;
	}

	return video.RenderLogoScaled(
		framePixels.data(),
		logoWidth,
		frameHeight,
		scaledLogoWidth,
		scaledLogoHeight,
		logoX,
		logoY);
}

bool LogoRenderer::RenderCenteredWithNativeOffset(
	Video& video,
	const std::vector<std::uint32_t>& logoImage,
	int logoWidth,
	int logoHeight,
	double scaleX,
	double scaleY,
	int renderX,
	int renderY,
	int renderWidth,
	int renderHeight,
	int nativeOffsetY) const
{
	std::vector<std::uint32_t> framePixels;
	int frameHeight = 0;
	if (!BuildCurrentFrame(logoImage, logoWidth, logoHeight, framePixels, frameHeight)) {
		return true;
	}

	const int scaledLogoWidth = static_cast<int>(logoWidth * scaleX);
	const int scaledLogoHeight = static_cast<int>(frameHeight * scaleY);
	const int logoX = renderX + ((renderWidth - scaledLogoWidth) / 2);
	const int logoOffsetY = static_cast<int>(nativeOffsetY * scaleY);
	const int logoY = renderY + ((renderHeight - scaledLogoHeight) / 2) + logoOffsetY;

	return video.RenderLogoScaled(
		framePixels.data(),
		logoWidth,
		frameHeight,
		scaledLogoWidth,
		scaledLogoHeight,
		logoX,
		logoY);
}
