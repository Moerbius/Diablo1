#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class StormLib;

struct CELFrame {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::vector<std::uint8_t> pixels;  // 8-bit palette indices, top-to-bottom rows.
};

struct CELImage {
	std::vector<CELFrame> frames;
};

struct CELOptions {
	// Used for headerless regular CEL frames when width cannot be derived from a
	// frame header.
	std::uint32_t fallbackWidth = 0;
	std::uint32_t fallbackHeight = 0;
};

class CEL {
public:
	static CELImage LoadFromMPQ(StormLib& mpq, const std::string& filename, const CELOptions& options = {});
	static CELImage ParseCELData(const std::vector<std::byte>& data, const CELOptions& options = {});
	static std::vector<std::uint32_t> ConvertFrameToRGBA32(const CELFrame& frame,
		const std::vector<std::uint8_t>& palette,
		std::uint8_t transparentIndex = 0);
};
