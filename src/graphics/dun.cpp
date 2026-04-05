#include "graphics/dun.hpp"

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

bool ReadWordLayer(const std::vector<std::byte>& data, std::size_t& cursor, std::size_t entries, std::vector<std::uint16_t>& out)
{
	const std::size_t bytes = entries * sizeof(std::uint16_t);
	if (cursor + bytes > data.size()) {
		return false;
	}

	out.clear();
	out.reserve(entries);
	for (std::size_t i = 0; i < entries; ++i) {
		out.push_back(ReadLE16(data, cursor + i * 2));
	}
	cursor += bytes;
	return true;
}

} // namespace

DUNMap DUN::LoadFromMPQ(StormLib& mpq, const std::string& filename)
{
	std::vector<std::byte> data;
	if (!mpq.ReadFile(filename, data)) {
		std::fprintf(stderr, "DUN: Failed to read file from MPQ: %s\n", filename.c_str());
		return DUNMap{};
	}
	return ParseDUNData(data);
}

DUNMap DUN::ParseDUNData(const std::vector<std::byte>& data)
{
	DUNMap map;
	if (data.size() < 4) {
		std::fprintf(stderr, "DUN: Data too small\n");
		return map;
	}

	map.width = ReadLE16(data, 0);
	map.height = ReadLE16(data, 2);
	if (map.width == 0 || map.height == 0) {
		std::fprintf(stderr, "DUN: Invalid map dimensions\n");
		return DUNMap{};
	}

	const std::size_t tileCount = static_cast<std::size_t>(map.width) * map.height;
	const std::size_t subtileCount = tileCount * 4;

	std::size_t cursor = 4;
	if (!ReadWordLayer(data, cursor, tileCount, map.baseLayer)) {
		std::fprintf(stderr, "DUN: Missing base layer\n");
		return DUNMap{};
	}

	if (cursor == data.size()) {
		return map;
	}

	if (!ReadWordLayer(data, cursor, subtileCount, map.itemsLayer)
		|| !ReadWordLayer(data, cursor, subtileCount, map.monstersLayer)
		|| !ReadWordLayer(data, cursor, subtileCount, map.objectsLayer)
		|| !ReadWordLayer(data, cursor, subtileCount, map.unusedLayer)) {
		std::fprintf(stderr, "DUN: Incomplete extended layers\n");
		return DUNMap{};
	}

	if (cursor != data.size()) {
		std::fprintf(stderr, "DUN: Trailing bytes after layers\n");
		return DUNMap{};
	}

	return map;
}
