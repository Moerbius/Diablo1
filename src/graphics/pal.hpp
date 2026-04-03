#pragma once

#include <cstdint>
#include <string>
#include <vector>

class StormLib;

class PAL {
public:
	static std::vector<std::uint8_t> LoadFromMPQ(StormLib& mpq, const std::string& filename);
	static std::vector<std::uint8_t> ParsePALData(const std::vector<std::byte>& data);
};
