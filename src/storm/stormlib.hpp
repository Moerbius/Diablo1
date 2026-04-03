#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

class StormLib {
public:
	StormLib();
	~StormLib();

	bool OpenArchive(const std::string& archivePath);
	void CloseArchive();

	bool IsArchiveOpen() const;
	bool HasFile(const std::string& filePath) const;
	bool ReadFile(const std::string& filePath, std::vector<std::byte>& outData) const;

private:
	struct MpqHeader {
		std::uint32_t id;
		std::uint32_t headerSize;
		std::uint32_t archiveSize;
		std::uint16_t formatVersion;
		std::uint16_t blockSizeShift;
		std::uint32_t hashTablePos;
		std::uint32_t blockTablePos;
		std::uint32_t hashTableSize;
		std::uint32_t blockTableSize;
	};

	struct HashEntry {
		std::uint32_t nameHashA;
		std::uint32_t nameHashB;
		std::uint16_t locale;
		std::uint16_t platform;
		std::uint32_t blockIndex;
	};

	struct BlockEntry {
		std::uint32_t filePos;
		std::uint32_t compressedSize;
		std::uint32_t fileSize;
		std::uint32_t flags;
	};

	bool FindMpqHeader(std::uint64_t fileSize, std::uint64_t& headerOffset, MpqHeader& header) const;
	bool LoadTables();
	bool ReadAt(std::uint64_t offset, void* outBuffer, std::size_t size) const;
	bool ReadFileSingleUnit(const std::string& filePath, const BlockEntry& block, std::vector<std::byte>& outData) const;
	bool ReadFileSectorBased(const std::string& filePath, const BlockEntry& block, std::vector<std::byte>& outData) const;
	bool ReadStoredSector(const std::byte* sectorData, std::size_t sectorSize, std::byte* outData, std::size_t outSize) const;
	bool DecompressImploded(const std::byte* inData, std::size_t inSize, std::byte* outData, std::size_t outSize) const;
	int FindBlockIndex(const std::string& filePath) const;

	static std::string NormalizePath(const std::string& path);
	static void BuildCryptTable(std::uint32_t table[0x500]);
	static std::uint32_t HashString(const std::string& text, std::uint32_t hashType, const std::uint32_t table[0x500]);
	static void DecryptBlock(std::uint32_t* data, std::size_t count, std::uint32_t key, const std::uint32_t table[0x500]);
	static void DecryptBytes(std::byte* data, std::size_t length, std::uint32_t key, const std::uint32_t table[0x500]);
	static std::uint32_t CalcFileKey(const std::string& filePath, const BlockEntry& block, const std::uint32_t table[0x500]);

	static constexpr std::uint32_t kMpqSignature = 0x1A51504Du;
	static constexpr std::uint32_t kHashTypeTableOffset = 0;
	static constexpr std::uint32_t kHashTypeNameA = 1;
	static constexpr std::uint32_t kHashTypeNameB = 2;
	static constexpr std::uint32_t kHashTableKeyType = 3;

	static constexpr std::uint32_t kHashEntryFree = 0xFFFFFFFFu;
	static constexpr std::uint32_t kHashEntryDeleted = 0xFFFFFFFEu;

	static constexpr std::uint32_t kFileFlagImplode = 0x00000100u;
	static constexpr std::uint32_t kFileFlagCompress = 0x00000200u;
	static constexpr std::uint32_t kFileFlagEncrypted = 0x00010000u;
	static constexpr std::uint32_t kFileFlagKeyV2 = 0x00020000u;
	static constexpr std::uint32_t kFileFlagSingleUnit = 0x01000000u;
	static constexpr std::uint32_t kFileFlagSectorCrc = 0x04000000u;
	static constexpr std::uint32_t kFileFlagExists = 0x80000000u;

	mutable std::ifstream archiveStream_;
	std::uint64_t archiveOffset_;
	MpqHeader header_;
	std::vector<HashEntry> hashTable_;
	std::vector<BlockEntry> blockTable_;
	std::uint32_t cryptTable_[0x500];

	std::string archivePath_;
	bool archiveOpen_;
};