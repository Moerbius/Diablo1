#include "portrait_renderer.hpp"

#include "video.hpp"

#include <algorithm>

bool PortraitRenderer::ExtractFrame(
	const std::vector<std::uint32_t>& atlas,
	int atlasWidth,
	int atlasHeight,
	int frameCount,
	int frameIndex,
	bool preferVertical,
	std::vector<std::uint32_t>& framePixels,
	int& frameWidth,
	int& frameHeight) const
{
	if (atlas.empty() || atlasWidth <= 0 || atlasHeight <= 0 || frameCount <= 0) {
		return false;
	}

	frameIndex = std::clamp(frameIndex, 0, frameCount - 1);
	int frameX = 0;
	int frameY = 0;

	const bool canVertical = (atlasHeight % frameCount) == 0;
	const bool canHorizontal = (atlasWidth % frameCount) == 0;
	if (!canVertical && !canHorizontal) {
		return false;
	}

	const bool useVertical = preferVertical ? canVertical : !canHorizontal;
	if (useVertical) {
		frameWidth = atlasWidth;
		frameHeight = atlasHeight / frameCount;
		frameY = frameIndex * frameHeight;
	} else {
		frameWidth = atlasWidth / frameCount;
		frameHeight = atlasHeight;
		frameX = frameIndex * frameWidth;
	}

	framePixels.clear();
	framePixels.reserve(frameWidth * frameHeight);
	for (int y = 0; y < frameHeight; ++y) {
		for (int x = 0; x < frameWidth; ++x) {
			const int srcX = frameX + x;
			const int srcY = frameY + y;
			framePixels.push_back(atlas[srcY * atlasWidth + srcX]);
		}
	}

	return true;
}

bool PortraitRenderer::RenderFrameFitted(
	Video& video,
	const std::vector<std::uint32_t>& atlas,
	int atlasWidth,
	int atlasHeight,
	int frameCount,
	int frameIndex,
	bool preferVertical,
	int boxX,
	int boxY,
	int boxWidth,
	int boxHeight) const
{
	std::vector<std::uint32_t> framePixels;
	int frameWidth = 0;
	int frameHeight = 0;
	if (!ExtractFrame(
			atlas,
			atlasWidth,
			atlasHeight,
			frameCount,
			frameIndex,
			preferVertical,
			framePixels,
			frameWidth,
			frameHeight)) {
		return true;
	}

	const double fitScale = std::min(
		static_cast<double>(boxWidth) / frameWidth,
		static_cast<double>(boxHeight) / frameHeight);
	const int renderWidth = std::max(1, static_cast<int>(frameWidth * fitScale));
	const int renderHeight = std::max(1, static_cast<int>(frameHeight * fitScale));
	const int renderX = boxX + ((boxWidth - renderWidth) / 2);
	const int renderY = boxY + ((boxHeight - renderHeight) / 2);

	return video.RenderLogoScaled(
		framePixels.data(),
		frameWidth,
		frameHeight,
		renderWidth,
		renderHeight,
		renderX,
		renderY);
}
