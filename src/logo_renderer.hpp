#pragma once

#include <cstdint>
#include <vector>

class Video;

class LogoRenderer {
public:
	void Reset();
	void Update(double dt);
	double GetAnimationTime() const;

	bool RenderTopCentered(
		Video& video,
		const std::vector<std::uint32_t>& logoImage,
		int logoWidth,
		int logoHeight,
		double uniformScale,
		double logoScale,
		int renderX,
		int renderY,
		int renderWidth,
		int* outScaledLogoHeight = nullptr) const;

	bool RenderCenteredWithNativeOffset(
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
		int nativeOffsetY) const;

private:
	bool BuildCurrentFrame(
		const std::vector<std::uint32_t>& logoImage,
		int logoWidth,
		int logoHeight,
		std::vector<std::uint32_t>& framePixels,
		int& frameHeight) const;

	double animationTime_ = 0.0;
	int currentFrame_ = 0;
};
