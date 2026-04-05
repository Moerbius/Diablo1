#include "graphics/min.hpp"

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

std::uint16_t DetectRefsPerSubTile(std::size_t bytes)
{
	if ((bytes % 32u) == 0u && (bytes % 20u) != 0u) {
		return 16;
	}
	if ((bytes % 20u) == 0u && (bytes % 32u) != 0u) {
		return 10;
	}
	if ((bytes % 32u) == 0u) {
		return 16;
	}
	if ((bytes % 20u) == 0u) {
		return 10;
	}
	return 0;
}

} // namespace

MINFile MIN::LoadFromMPQ(StormLib& mpq, const std::string& filename, const MINParseOptions& options)
{
	std::vector<std::byte> data;
	if (!mpq.ReadFile(filename, data)) {
		std::fprintf(stderr, "MIN: Failed to read file from MPQ: %s\n", filename.c_str());
		return MINFile{};
	}
	return ParseMINData(data, options);
}

MINFile MIN::ParseMINData(const std::vector<std::byte>& data, const MINParseOptions& options)
{
	MINFile file;
	if (data.empty() || (data.size() % 2u) != 0u) {
		std::fprintf(stderr, "MIN: Invalid data size\n");
		return file;
	}

	file.refsPerSubTile = options.refsPerSubTile;
	if (file.refsPerSubTile == 0) {
		file.refsPerSubTile = DetectRefsPerSubTile(data.size());
	}
	if (file.refsPerSubTile != 10 && file.refsPerSubTile != 16) {
		std::fprintf(stderr, "MIN: Could not determine refs per subtile\n");
		return MINFile{};
	}

	const std::size_t bytesPerSubTile = static_cast<std::size_t>(file.refsPerSubTile) * 2u;
	if ((data.size() % bytesPerSubTile) != 0u) {
		std::fprintf(stderr, "MIN: Data size does not match refs per subtile\n");
		return MINFile{};
	}

	const std::size_t subTileCount = data.size() / bytesPerSubTile;
	file.subTiles.reserve(subTileCount);
	for (std::size_t i = 0; i < subTileCount; ++i) {
		MINSubTile subTile;
		subTile.frameRefs.reserve(file.refsPerSubTile);
		const std::size_t baseOffset = i * bytesPerSubTile;
		for (std::size_t j = 0; j < file.refsPerSubTile; ++j) {
			const std::uint16_t raw = ReadLE16(data, baseOffset + j * 2u);
			const std::uint8_t type = static_cast<std::uint8_t>((raw >> 12) & 0x0Fu);
			const std::uint16_t incrementedIndex = static_cast<std::uint16_t>(raw & 0x0FFFu);

			MINFrameReference ref;
			ref.rawValue = raw;
			ref.celFrameType = type;
			ref.incrementedCelFrameIndex = incrementedIndex;
			ref.isTransparent = incrementedIndex == 0;
			ref.celFrameIndex = ref.isTransparent ? -1 : static_cast<int>(incrementedIndex - 1);
			subTile.frameRefs.push_back(ref);
		}
		file.subTiles.push_back(std::move(subTile));
	}

	return file;
}
