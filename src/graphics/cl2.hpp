#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class StormLib;

struct CL2Frame {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::vector<std::uint8_t> pixels;
};

struct CL2Image {
	std::vector<CL2Frame> frames;
	std::vector<std::pair<std::size_t, std::size_t>> groupFrameRanges;
};

class CL2 {
public:
	static CL2Image LoadFromMPQ(StormLib& mpq, const std::string& filename);
	static CL2Image ParseCL2Data(const std::vector<std::byte>& data);
	static std::vector<std::uint32_t> ConvertFrameToRGBA32(
		const CL2Frame& frame,
		const std::vector<std::uint8_t>& palette,
		std::uint8_t transparentIndex = 0);
};
