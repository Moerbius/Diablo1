#pragma once

#include <cstdint>
#include <string>
#include <vector>

class StormLib;

struct PCXImage {
	std::uint32_t width;
	std::uint32_t height;
	std::vector<std::uint8_t> pixels;      // Indexed 8-bit pixel data
	std::vector<std::uint8_t> palette;     // 768 bytes (256 * 3 RGB)
};

class PCX {
public:
	static PCXImage LoadFromMPQ(StormLib& mpq, const std::string& filename);
	static std::vector<std::uint32_t> ConvertToRGBA32(const PCXImage& image);

private:
	static PCXImage ParsePCXData(const std::vector<std::byte>& data);
};
