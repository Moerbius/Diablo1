#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class StormLib;

struct MINFrameReference {
	std::uint16_t rawValue = 0;
	std::uint8_t celFrameType = 0;
	std::uint16_t incrementedCelFrameIndex = 0;
	int celFrameIndex = -1;
	bool isTransparent = true;
};

struct MINSubTile {
	std::vector<MINFrameReference> frameRefs;
};

struct MINFile {
	std::uint16_t refsPerSubTile = 0;
	std::vector<MINSubTile> subTiles;

	[[nodiscard]] bool IsValid() const
	{
		return refsPerSubTile > 0 && !subTiles.empty();
	}
};

struct MINParseOptions {
	std::uint16_t refsPerSubTile = 0; // 0 = auto-detect.
};

class MIN {
public:
	static MINFile LoadFromMPQ(StormLib& mpq, const std::string& filename, const MINParseOptions& options = {});
	static MINFile ParseMINData(const std::vector<std::byte>& data, const MINParseOptions& options = {});
};
