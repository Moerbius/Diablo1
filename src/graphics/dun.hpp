#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class StormLib;

struct DUNMap {
	std::uint16_t width = 0;
	std::uint16_t height = 0;
	std::vector<std::uint16_t> baseLayer;
	std::vector<std::uint16_t> itemsLayer;
	std::vector<std::uint16_t> monstersLayer;
	std::vector<std::uint16_t> objectsLayer;
	std::vector<std::uint16_t> unusedLayer;

	[[nodiscard]] bool IsValid() const
	{
		return width > 0 && height > 0 && baseLayer.size() == static_cast<std::size_t>(width) * height;
	}

	[[nodiscard]] bool HasExtendedLayers() const
	{
		return !itemsLayer.empty() && !monstersLayer.empty() && !objectsLayer.empty() && !unusedLayer.empty();
	}

	[[nodiscard]] int BaseTileIndexAt(std::size_t x, std::size_t y) const
	{
		if (x >= width || y >= height) {
			return -1;
		}
		const std::uint16_t value = baseLayer[y * width + x];
		if (value == 0) {
			return -1;
		}
		return static_cast<int>(value - 1);
	}
};

class DUN {
public:
	static DUNMap LoadFromMPQ(StormLib& mpq, const std::string& filename);
	static DUNMap ParseDUNData(const std::vector<std::byte>& data);
};
