#include "graphics/pal.hpp"

#include "storm/stormlib.hpp"

#include <cstdio>

std::vector<std::uint8_t> PAL::LoadFromMPQ(StormLib& mpq, const std::string& filename)
{
	std::vector<std::byte> data;
	if (!mpq.ReadFile(filename, data)) {
		std::fprintf(stderr, "PAL: Failed to read file from MPQ: %s\n", filename.c_str());
		return std::vector<std::uint8_t>();
	}

	return ParsePALData(data);
}

std::vector<std::uint8_t> PAL::ParsePALData(const std::vector<std::byte>& data)
{
	// PAL files are exactly 768 bytes: 256 colors * 3 bytes (RGB)
	constexpr std::size_t expectedSize = 256 * 3;

	if (data.size() != expectedSize) {
		std::fprintf(stderr, "PAL: Invalid file size. Expected %zu bytes, got %zu bytes\n", expectedSize, data.size());
		return std::vector<std::uint8_t>();
	}

	std::vector<std::uint8_t> palette;
	palette.reserve(expectedSize);

	for (std::size_t i = 0; i < data.size(); ++i) {
		palette.push_back(static_cast<std::uint8_t>(data[i]));
	}

	return palette;
}
