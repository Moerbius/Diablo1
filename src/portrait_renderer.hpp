#pragma once

#include <cstdint>
#include <vector>

class Video;

class PortraitRenderer {
public:
	bool ExtractFrame(
		const std::vector<std::uint32_t>& atlas,
		int atlasWidth,
		int atlasHeight,
		int frameCount,
		int frameIndex,
		bool preferVertical,
		std::vector<std::uint32_t>& framePixels,
		int& frameWidth,
		int& frameHeight) const;

	bool RenderFrameFitted(
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
		int boxHeight) const;
};
