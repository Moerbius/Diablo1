#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class StormLib;

struct TILTile {
	std::array<std::uint16_t, 4> rawSubTileIndices{};
	std::array<std::uint16_t, 4> resolvedSubTileIndices{};
};

struct TILFile {
	std::vector<TILTile> tiles;

	[[nodiscard]] bool IsValid() const
	{
		return !tiles.empty();
	}
};

class TIL {
public:
	static TILFile LoadFromMPQ(StormLib& mpq, const std::string& filename);
	static TILFile ParseTILData(const std::vector<std::byte>& data);
};
