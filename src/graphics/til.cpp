#include "graphics/til.hpp"

#include "storm/stormlib.hpp"

#include <cstdio>

namespace {

std::uint16_t ReadLE16(const std::vector<std::byte>& data, std::size_t offset)
{
	if (offset + 1 >= data.size()) {
		return 0;
	}
	const std::uint16_t b0 = static_cast<std::uint8_t>(data[offset + 0]);
	const std::uint16_t b1 = static_cast<std::uint8_t>(data[offset + 1]);
	return static_cast<std::uint16_t>(b0 | (b1 << 8));
}

} // namespace

TILFile TIL::LoadFromMPQ(StormLib& mpq, const std::string& filename)
{
	std::vector<std::byte> data;
	if (!mpq.ReadFile(filename, data)) {
		std::fprintf(stderr, "TIL: Failed to read file from MPQ: %s\n", filename.c_str());
		return TILFile{};
	}
	return ParseTILData(data);
}

TILFile TIL::ParseTILData(const std::vector<std::byte>& data)
{
	TILFile file;
	if (data.empty() || (data.size() % 8u) != 0u) {
		std::fprintf(stderr, "TIL: Invalid data size\n");
		return file;
	}

	const std::size_t tileCount = data.size() / 8u;
	file.tiles.reserve(tileCount);
	for (std::size_t i = 0; i < tileCount; ++i) {
		TILTile tile;
		for (std::size_t j = 0; j < 4; ++j) {
			const std::uint16_t raw = ReadLE16(data, i * 8u + j * 2u);
			tile.rawSubTileIndices[j] = raw;
			tile.resolvedSubTileIndices[j] = static_cast<std::uint16_t>(raw + 1u);
		}
		file.tiles.push_back(tile);
	}

	return file;
}
