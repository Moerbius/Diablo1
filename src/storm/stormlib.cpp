#include "storm/stormlib.hpp"
#include "smacker/pklib/pklib.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <limits>
#include <memory>
#include <vector>

namespace {

struct PkExplodeInfo {
	const unsigned char* in;
	const unsigned char* inEnd;
	unsigned char* out;
	unsigned char* outEnd;
	bool overflow;
};

unsigned int ReadInputData(char* buf, unsigned int* size, void* param)
{
	auto* info = static_cast<PkExplodeInfo*>(param);
	const std::size_t maxRequested = static_cast<std::size_t>(*size);
	const std::size_t available = static_cast<std::size_t>(info->inEnd - info->in);
	const std::size_t toCopy = std::min(maxRequested, available);
	if (toCopy > 0) {
		std::memcpy(buf, info->in, toCopy);
		info->in += toCopy;
	}
	return static_cast<unsigned int>(toCopy);
}

void WriteOutputData(char* buf, unsigned int* size, void* param)
{
	auto* info = static_cast<PkExplodeInfo*>(param);
	const std::size_t requested = static_cast<std::size_t>(*size);
	const std::size_t available = static_cast<std::size_t>(info->outEnd - info->out);
	std::size_t toWrite = requested;
	if (toWrite > available) {
		info->overflow = true;
		toWrite = available;
	}

	if (toWrite > 0) {
		std::memcpy(info->out, buf, toWrite);
		info->out += toWrite;
	}
}

} // namespace

StormLib::StormLib() : archiveOffset_(0), header_{}, archiveOpen_(false)
{
	BuildCryptTable(cryptTable_);
}

StormLib::~StormLib()
{
	CloseArchive();
}

bool StormLib::OpenArchive(const std::string& archivePath)
{
	CloseArchive();

	archiveStream_.open(archivePath, std::ios::binary);
	if (!archiveStream_.is_open()) {
		return false;
	}

	archiveStream_.seekg(0, std::ios::end);
	const std::streampos endPos = archiveStream_.tellg();
	if (endPos < 0) {
		CloseArchive();
		return false;
	}

	const std::uint64_t fileSize = static_cast<std::uint64_t>(endPos);
	MpqHeader foundHeader{};
	std::uint64_t foundHeaderOffset = 0;
	if (!FindMpqHeader(fileSize, foundHeaderOffset, foundHeader)) {
		CloseArchive();
		return false;
	}

	archiveOffset_ = foundHeaderOffset;
	header_ = foundHeader;

	if (!LoadTables()) {
		CloseArchive();
		return false;
	}

	archivePath_ = archivePath;
	archiveOpen_ = true;
	return true;
}

void StormLib::CloseArchive()
{
	if (archiveStream_.is_open()) {
		archiveStream_.close();
	}

	archiveOffset_ = 0;
	header_ = {};
	hashTable_.clear();
	blockTable_.clear();
	archivePath_.clear();
	archiveOpen_ = false;
}

bool StormLib::IsArchiveOpen() const
{
	return archiveOpen_;
}

bool StormLib::HasFile(const std::string& filePath) const
{
	if (!archiveOpen_) {
		return false;
	}

	return FindBlockIndex(filePath) >= 0;
}

bool StormLib::ReadFile(const std::string& filePath, std::vector<std::byte>& outData) const
{
	outData.clear();

	if (!archiveOpen_) {
		return false;
	}

	const int blockIndex = FindBlockIndex(filePath);
	if (blockIndex < 0) {
		std::fprintf(stderr, "MPQ read failed: file not found: %s\n", filePath.c_str());
		return false;
	}

	const BlockEntry& block = blockTable_[static_cast<std::size_t>(blockIndex)];
	if ((block.flags & kFileFlagExists) == 0) {
		std::fprintf(stderr, "MPQ read failed: block does not exist: %s\n", filePath.c_str());
		return false;
	}

	if (block.fileSize == 0) {
		return true;
	}

	if ((block.flags & kFileFlagSingleUnit) != 0) {
		return ReadFileSingleUnit(filePath, block, outData);
	}

	return ReadFileSectorBased(filePath, block, outData);
}

bool StormLib::ReadFileSingleUnit(const std::string& filePath, const BlockEntry& block, std::vector<std::byte>& outData) const
{
	std::vector<std::byte> encoded(static_cast<std::size_t>(block.compressedSize));
	if (!ReadAt(archiveOffset_ + block.filePos, encoded.data(), encoded.size())) {
		std::fprintf(stderr, "MPQ read failed: unable to read single-unit data: %s\n", filePath.c_str());
		outData.clear();
		return false;
	}

	if ((block.flags & kFileFlagEncrypted) != 0) {
		const std::uint32_t fileKey = CalcFileKey(filePath, block, cryptTable_);
		DecryptBytes(encoded.data(), encoded.size(), fileKey, cryptTable_);
	}

	outData.resize(static_cast<std::size_t>(block.fileSize));

	if ((block.flags & kFileFlagImplode) != 0) {
		if (!DecompressImploded(encoded.data(), encoded.size(), outData.data(), outData.size())) {
			std::fprintf(stderr, "MPQ read failed: implode decompress failed for single-unit file: %s\n", filePath.c_str());
			outData.clear();
			return false;
		}
		return true;
	}

	if ((block.flags & kFileFlagCompress) != 0) {
		std::fprintf(stderr, "MPQ read failed: MPQ_FILE_COMPRESS not implemented for single-unit file: %s\n", filePath.c_str());
		outData.clear();
		return false;
	}

	if (encoded.size() != outData.size()) {
		outData.clear();
		return false;
	}

	std::memcpy(outData.data(), encoded.data(), outData.size());

	return true;
}

bool StormLib::ReadFileSectorBased(const std::string& filePath, const BlockEntry& block, std::vector<std::byte>& outData) const
{
	const std::uint32_t sectorSize = 512u << header_.blockSizeShift;
	if (sectorSize == 0) {
		std::fprintf(stderr, "MPQ read failed: invalid sector size for file: %s\n", filePath.c_str());
		return false;
	}

	const std::uint32_t sectorCount = (block.fileSize + sectorSize - 1) / sectorSize;
	if (sectorCount == 0) {
		return true;
	}

	outData.resize(static_cast<std::size_t>(block.fileSize));

	const std::uint64_t blockStart = archiveOffset_ + block.filePos;
	const bool hasCompressedData = (block.flags & (kFileFlagCompress | kFileFlagImplode)) != 0;
	const bool isEncrypted = (block.flags & kFileFlagEncrypted) != 0;
	const std::uint32_t fileKey = isEncrypted ? CalcFileKey(filePath, block, cryptTable_) : 0;

	if (!hasCompressedData) {
		if (block.compressedSize < block.fileSize) {
			outData.clear();
			return false;
		}

		if (!isEncrypted) {
			if (!ReadAt(blockStart, outData.data(), outData.size())) {
				outData.clear();
				return false;
			}
			return true;
		}

		std::size_t outOffset = 0;
		for (std::uint32_t sectorIndex = 0; sectorIndex < sectorCount; ++sectorIndex) {
			const std::size_t remaining = outData.size() - outOffset;
			const std::size_t chunkSize = std::min<std::size_t>(sectorSize, remaining);
			std::vector<std::byte> chunk(chunkSize);

			if (!ReadAt(blockStart + outOffset, chunk.data(), chunk.size())) {
				outData.clear();
				return false;
			}

			DecryptBytes(chunk.data(), chunk.size(), fileKey + sectorIndex, cryptTable_);
			std::memcpy(outData.data() + outOffset, chunk.data(), chunk.size());
			outOffset += chunk.size();
		}

		return true;
	}

	const std::size_t sectorTableCount = static_cast<std::size_t>(sectorCount) + 1u + (((block.flags & kFileFlagSectorCrc) != 0) ? 1u : 0u);
	const std::size_t sectorTableBytes = sectorTableCount * sizeof(std::uint32_t);

	if (block.compressedSize < sectorTableBytes) {
		std::fprintf(stderr, "MPQ read failed: sector table truncated for file: %s\n", filePath.c_str());
		outData.clear();
		return false;
	}

	std::vector<std::uint32_t> sectorOffsets(sectorTableCount);
	if (!ReadAt(blockStart, sectorOffsets.data(), sectorTableBytes)) {
		std::fprintf(stderr, "MPQ read failed: unable to read sector table for file: %s\n", filePath.c_str());
		outData.clear();
		return false;
	}

	if (isEncrypted) {
		DecryptBlock(sectorOffsets.data(), sectorOffsets.size(), fileKey - 1, cryptTable_);
	}

	if (sectorOffsets[0] < sectorTableBytes) {
		std::fprintf(stderr, "MPQ read failed: sector table first offset invalid for file: %s\n", filePath.c_str());
		outData.clear();
		return false;
	}

	for (std::size_t i = 0; i + 1 < sectorOffsets.size(); ++i) {
		if (sectorOffsets[i] > sectorOffsets[i + 1]) {
			std::fprintf(stderr, "MPQ read failed: sector offsets not monotonic for file: %s\n", filePath.c_str());
			outData.clear();
			return false;
		}
		if (sectorOffsets[i + 1] > block.compressedSize) {
			std::fprintf(stderr, "MPQ read failed: sector offset out of bounds for file: %s\n", filePath.c_str());
			outData.clear();
			return false;
		}
	}

	const std::size_t blockByteCount = static_cast<std::size_t>(block.compressedSize);
	std::vector<std::byte> blockBuffer(blockByteCount);
	if (!ReadAt(blockStart, blockBuffer.data(), blockBuffer.size())) {
		std::fprintf(stderr, "MPQ read failed: unable to read block payload for file: %s\n", filePath.c_str());
		outData.clear();
		return false;
	}

	std::size_t outOffset = 0;
	for (std::uint32_t sectorIndex = 0; sectorIndex < sectorCount; ++sectorIndex) {
		const std::size_t chunkStart = static_cast<std::size_t>(sectorOffsets[sectorIndex]);
		const std::size_t chunkEnd = static_cast<std::size_t>(sectorOffsets[sectorIndex + 1]);
		if (chunkEnd < chunkStart || chunkEnd > blockBuffer.size()) {
			std::fprintf(stderr, "MPQ read failed: chunk range invalid for file: %s\n", filePath.c_str());
			outData.clear();
			return false;
		}

		const std::size_t chunkSize = chunkEnd - chunkStart;
		const std::size_t remaining = outData.size() - outOffset;
		const std::size_t expectedOutSize = std::min<std::size_t>(sectorSize, remaining);
		if (chunkSize == 0 || expectedOutSize == 0) {
			std::fprintf(stderr, "MPQ read failed: chunk size invalid for file: %s\n", filePath.c_str());
			outData.clear();
			return false;
		}

		std::vector<std::byte> chunk(chunkSize);
		std::memcpy(chunk.data(), blockBuffer.data() + chunkStart, chunkSize);

		if (isEncrypted) {
			DecryptBytes(chunk.data(), chunk.size(), fileKey + sectorIndex, cryptTable_);
		}

		const std::byte* chunkData = chunk.data();
		if (chunkSize == expectedOutSize) {
			if (!ReadStoredSector(chunkData, chunkSize, outData.data() + outOffset, expectedOutSize)) {
				std::fprintf(stderr, "MPQ read failed: stored sector copy failed for file: %s\n", filePath.c_str());
				outData.clear();
				return false;
			}
		} else if (chunkSize == expectedOutSize + 1 && chunkData[0] == std::byte{0x00}) {
			if (!ReadStoredSector(chunkData + 1, expectedOutSize, outData.data() + outOffset, expectedOutSize)) {
				std::fprintf(stderr, "MPQ read failed: stored+mask sector copy failed for file: %s\n", filePath.c_str());
				outData.clear();
				return false;
			}
		} else if ((block.flags & kFileFlagImplode) != 0) {
			if (!DecompressImploded(chunkData, chunkSize, outData.data() + outOffset, expectedOutSize)) {
				std::fprintf(stderr, "MPQ read failed: implode decompress failed for sector %u in file: %s\n", sectorIndex, filePath.c_str());
				outData.clear();
				return false;
			}
		} else {
			std::fprintf(stderr, "MPQ read failed: unknown sector compression layout for file: %s\n", filePath.c_str());
			outData.clear();
			return false;
		}

		outOffset += expectedOutSize;
	}

	if (outOffset != outData.size()) {
		std::fprintf(stderr, "MPQ read failed: output size mismatch for file: %s\n", filePath.c_str());
		outData.clear();
		return false;
	}

	return true;
}

bool StormLib::ReadStoredSector(const std::byte* sectorData, std::size_t sectorSize, std::byte* outData, std::size_t outSize) const
{
	if (sectorSize != outSize) {
		return false;
	}

	std::memcpy(outData, sectorData, outSize);
	return true;
}

bool StormLib::DecompressImploded(const std::byte* inData, std::size_t inSize, std::byte* outData, std::size_t outSize) const
{
	auto workBuffer = std::make_unique<char[]>(EXP_BUFFER_SIZE);
	std::memset(workBuffer.get(), 0, EXP_BUFFER_SIZE);

	PkExplodeInfo info{};
	info.in = reinterpret_cast<const unsigned char*>(inData);
	info.inEnd = info.in + inSize;
	info.out = reinterpret_cast<unsigned char*>(outData);
	info.outEnd = info.out + outSize;
	info.overflow = false;

	const unsigned int result = explode(ReadInputData, WriteOutputData, workBuffer.get(), &info);
	if (result != CMP_NO_ERROR || info.overflow) {
		std::fprintf(stderr, "MPQ implode failed: result=%u overflow=%d in=%zu out=%zu\n",
			result,
			info.overflow ? 1 : 0,
			inSize,
			outSize);
		return false;
	}

	return info.out == info.outEnd;
}

bool StormLib::FindMpqHeader(std::uint64_t fileSize, std::uint64_t& headerOffset, MpqHeader& header) const
{
	constexpr std::uint64_t kScanStep = 0x200;
	constexpr std::uint64_t kScanLimit = 0x08000000;

	if (fileSize < sizeof(MpqHeader)) {
		return false;
	}

	const std::uint64_t maxScan = std::min(fileSize - sizeof(MpqHeader), kScanLimit);
	for (std::uint64_t offset = 0; offset <= maxScan; offset += kScanStep) {
		MpqHeader candidate{};
		if (!ReadAt(offset, &candidate, sizeof(candidate))) {
			return false;
		}

		if (candidate.id != kMpqSignature) {
			continue;
		}

		if (candidate.headerSize < sizeof(MpqHeader)) {
			continue;
		}

		if (candidate.hashTableSize == 0 || candidate.blockTableSize == 0) {
			continue;
		}

		headerOffset = offset;
		header = candidate;
		return true;
	}

	return false;
}

bool StormLib::LoadTables()
{
	if (header_.hashTableSize > (std::numeric_limits<std::size_t>::max() / sizeof(HashEntry))) {
		return false;
	}

	if (header_.blockTableSize > (std::numeric_limits<std::size_t>::max() / sizeof(BlockEntry))) {
		return false;
	}

	const std::size_t hashBytes = static_cast<std::size_t>(header_.hashTableSize) * sizeof(HashEntry);
	const std::size_t blockBytes = static_cast<std::size_t>(header_.blockTableSize) * sizeof(BlockEntry);

	std::vector<std::uint32_t> hashRaw(hashBytes / sizeof(std::uint32_t));
	std::vector<std::uint32_t> blockRaw(blockBytes / sizeof(std::uint32_t));

	const std::uint64_t hashOffset = archiveOffset_ + header_.hashTablePos;
	const std::uint64_t blockOffset = archiveOffset_ + header_.blockTablePos;

	if (!ReadAt(hashOffset, hashRaw.data(), hashBytes)) {
		return false;
	}

	if (!ReadAt(blockOffset, blockRaw.data(), blockBytes)) {
		return false;
	}

	const std::uint32_t hashKey = HashString("(hash table)", kHashTableKeyType, cryptTable_);
	const std::uint32_t blockKey = HashString("(block table)", kHashTableKeyType, cryptTable_);

	DecryptBlock(hashRaw.data(), hashRaw.size(), hashKey, cryptTable_);
	DecryptBlock(blockRaw.data(), blockRaw.size(), blockKey, cryptTable_);

	hashTable_.resize(header_.hashTableSize);
	blockTable_.resize(header_.blockTableSize);

	std::memcpy(hashTable_.data(), hashRaw.data(), hashBytes);
	std::memcpy(blockTable_.data(), blockRaw.data(), blockBytes);
	return true;
}

bool StormLib::ReadAt(std::uint64_t offset, void* outBuffer, std::size_t size) const
{
	if (!archiveStream_.is_open()) {
		return false;
	}

	if (size == 0) {
		return true;
	}

	archiveStream_.clear();
	archiveStream_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
	if (!archiveStream_) {
		return false;
	}

	archiveStream_.read(static_cast<char*>(outBuffer), static_cast<std::streamsize>(size));
	return archiveStream_.good();
}

int StormLib::FindBlockIndex(const std::string& filePath) const
{
	if (hashTable_.empty()) {
		return -1;
	}

	const std::string normalized = NormalizePath(filePath);
	const std::uint32_t hashOffset = HashString(normalized, kHashTypeTableOffset, cryptTable_);
	const std::uint32_t hashA = HashString(normalized, kHashTypeNameA, cryptTable_);
	const std::uint32_t hashB = HashString(normalized, kHashTypeNameB, cryptTable_);

	const std::size_t tableSize = hashTable_.size();
	std::size_t index = static_cast<std::size_t>(hashOffset % tableSize);

	for (std::size_t probe = 0; probe < tableSize; ++probe) {
		const HashEntry& entry = hashTable_[index];

		if (entry.blockIndex == kHashEntryFree) {
			return -1;
		}

		if (entry.blockIndex != kHashEntryDeleted &&
			entry.nameHashA == hashA &&
			entry.nameHashB == hashB &&
			entry.blockIndex < blockTable_.size()) {
			return static_cast<int>(entry.blockIndex);
		}

		index = (index + 1) % tableSize;
	}

	return -1;
}

std::string StormLib::NormalizePath(const std::string& path)
{
	std::string result;
	result.reserve(path.size());

	for (unsigned char ch : path) {
		if (ch == '/') {
			result.push_back('\\');
		} else {
			result.push_back(static_cast<char>(std::toupper(ch)));
		}
	}

	return result;
}

void StormLib::BuildCryptTable(std::uint32_t table[0x500])
{
	std::uint32_t seed = 0x00100001;

	for (std::uint32_t index1 = 0; index1 < 0x100; ++index1) {
		std::uint32_t index2 = index1;

		for (std::uint32_t i = 0; i < 5; ++i, index2 += 0x100) {
			seed = (seed * 125 + 3) % 0x2AAAAB;
			const std::uint32_t temp1 = (seed & 0xFFFF) << 16;

			seed = (seed * 125 + 3) % 0x2AAAAB;
			const std::uint32_t temp2 = seed & 0xFFFF;

			table[index2] = temp1 | temp2;
		}
	}
}

std::uint32_t StormLib::HashString(const std::string& text, std::uint32_t hashType, const std::uint32_t table[0x500])
{
	std::uint32_t seed1 = 0x7FED7FED;
	std::uint32_t seed2 = 0xEEEEEEEE;

	for (unsigned char ch : text) {
		unsigned char upper = ch;
		if (upper == '/') {
			upper = '\\';
		}
		upper = static_cast<unsigned char>(std::toupper(upper));

		seed1 = table[(hashType << 8) + upper] ^ (seed1 + seed2);
		seed2 = upper + seed1 + seed2 + (seed2 << 5) + 3;
	}

	return seed1;
}

void StormLib::DecryptBlock(std::uint32_t* data, std::size_t count, std::uint32_t key, const std::uint32_t table[0x500])
{
	std::uint32_t seed1 = key;
	std::uint32_t seed2 = 0xEEEEEEEE;

	for (std::size_t i = 0; i < count; ++i) {
		seed2 += table[0x400 + (seed1 & 0xFF)];
		const std::uint32_t value = data[i] ^ (seed1 + seed2);
		seed1 = ((~seed1 << 21) + 0x11111111) | (seed1 >> 11);
		seed2 = value + seed2 + (seed2 << 5) + 3;
		data[i] = value;
	}
}

void StormLib::DecryptBytes(std::byte* data, std::size_t length, std::uint32_t key, const std::uint32_t table[0x500])
{
	std::uint32_t seed1 = key;
	std::uint32_t seed2 = 0xEEEEEEEE;

	const std::size_t dwordCount = length / 4;
	for (std::size_t i = 0; i < dwordCount; ++i) {
		const std::size_t offset = i * 4;
		seed2 += table[0x400 + (seed1 & 0xFF)];

		std::uint32_t enc = 0;
		for (std::size_t j = 0; j < 4; ++j) {
			enc |= (static_cast<std::uint32_t>(std::to_integer<unsigned char>(data[offset + j])) << (j * 8));
		}

		const std::uint32_t dec = enc ^ (seed1 + seed2);
		seed1 = ((~seed1 << 21) + 0x11111111) | (seed1 >> 11);
		seed2 = dec + seed2 + (seed2 << 5) + 3;

		for (std::size_t j = 0; j < 4; ++j) {
			data[offset + j] = static_cast<std::byte>((dec >> (j * 8)) & 0xFF);
		}
	}
}

std::uint32_t StormLib::CalcFileKey(const std::string& filePath, const BlockEntry& block, const std::uint32_t table[0x500])
{
	const std::size_t slash = filePath.find_last_of("\\/");
	const std::string plainName = (slash == std::string::npos) ? filePath : filePath.substr(slash + 1);

	std::uint32_t key = HashString(plainName, kHashTableKeyType, table);
	if ((block.flags & kFileFlagKeyV2) != 0) {
		key = (key + block.filePos) ^ block.fileSize;
	}

	return key;
}