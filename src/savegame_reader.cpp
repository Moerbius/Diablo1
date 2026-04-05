#include "savegame_reader.hpp"

#include "storm/stormlib.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kBlockSize = 16;
constexpr std::size_t kSha1HashSize = 5;
constexpr std::size_t kBlockSizeBytes = kBlockSize * sizeof(std::uint32_t);
constexpr std::size_t kSignatureSize = 8;
constexpr std::size_t kPlayerNameLength = 32;
constexpr std::size_t kClassOffset = 48;
constexpr std::size_t kBaseStrengthOffset = 49;
constexpr std::size_t kBaseMagicOffset = 50;
constexpr std::size_t kBaseDexterityOffset = 51;
constexpr std::size_t kBaseVitalityOffset = 52;
constexpr std::size_t kLevelOffset = 53;
constexpr std::size_t kNameOffset = 16;

struct Sha1Context {
	std::uint32_t state[kSha1HashSize] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
	std::uint32_t buffer[kBlockSize] = {};
};

struct CodecSignature {
	std::uint32_t checksum;
	std::uint8_t error;
	std::uint8_t lastChunkSize;
};

std::uint32_t LoadLE32(const std::byte* b)
{
	return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[3])) << 24)
		| (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[2])) << 16)
		| (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[1])) << 8)
		| static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[0]));
}

std::uint32_t LoadLE32(const char* b)
{
	return (static_cast<std::uint32_t>(static_cast<unsigned char>(b[3])) << 24)
		| (static_cast<std::uint32_t>(static_cast<unsigned char>(b[2])) << 16)
		| (static_cast<std::uint32_t>(static_cast<unsigned char>(b[1])) << 8)
		| static_cast<std::uint32_t>(static_cast<unsigned char>(b[0]));
}

std::uint32_t Sha1CircularShift(std::uint32_t word, std::size_t bits)
{
	if ((word & (1u << 31)) != 0) {
		return (0xFFFFFFFFu << bits) | (word >> (32 - bits));
	}
	return (word << bits) | (word >> (32 - bits));
}

void Sha1ProcessMessageBlock(Sha1Context& context)
{
	std::uint32_t w[80];
	std::memcpy(w, context.buffer, kBlockSize * sizeof(std::uint32_t));
	for (int i = 16; i < 80; ++i) {
		w[i] = w[i - 16] ^ w[i - 14] ^ w[i - 8] ^ w[i - 3];
	}

	std::uint32_t a = context.state[0];
	std::uint32_t b = context.state[1];
	std::uint32_t c = context.state[2];
	std::uint32_t d = context.state[3];
	std::uint32_t e = context.state[4];

	for (int i = 0; i < 20; ++i) {
		const std::uint32_t temp = Sha1CircularShift(a, 5) + ((b & c) | ((~b) & d)) + e + w[i] + 0x5A827999;
		e = d;
		d = c;
		c = Sha1CircularShift(b, 30);
		b = a;
		a = temp;
	}

	for (int i = 20; i < 40; ++i) {
		const std::uint32_t temp = Sha1CircularShift(a, 5) + (b ^ c ^ d) + e + w[i] + 0x6ED9EBA1;
		e = d;
		d = c;
		c = Sha1CircularShift(b, 30);
		b = a;
		a = temp;
	}

	for (int i = 40; i < 60; ++i) {
		const std::uint32_t temp = Sha1CircularShift(a, 5) + ((b & c) | (b & d) | (c & d)) + e + w[i] + 0x8F1BBCDC;
		e = d;
		d = c;
		c = Sha1CircularShift(b, 30);
		b = a;
		a = temp;
	}

	for (int i = 60; i < 80; ++i) {
		const std::uint32_t temp = Sha1CircularShift(a, 5) + (b ^ c ^ d) + e + w[i] + 0xCA62C1D6;
		e = d;
		d = c;
		c = Sha1CircularShift(b, 30);
		b = a;
		a = temp;
	}

	context.state[0] += a;
	context.state[1] += b;
	context.state[2] += c;
	context.state[3] += d;
	context.state[4] += e;
}

void Sha1Result(const Sha1Context& context, std::uint32_t digest[kSha1HashSize])
{
	std::memcpy(digest, context.state, sizeof(context.state));
}

void Sha1Calculate(Sha1Context& context, const std::uint32_t data[kBlockSize])
{
	std::memcpy(context.buffer, data, kBlockSize * sizeof(std::uint32_t));
	Sha1ProcessMessageBlock(context);
}

Sha1Context CodecInitKey(const char* password)
{
	std::uint32_t pw[kBlockSize];
	std::size_t j = 0;
	for (std::uint32_t& value : pw) {
		if (password[j] == '\0') {
			j = 0;
		}
		value = LoadLE32(&password[j]);
		j += sizeof(std::uint32_t);
	}

	std::uint32_t digest[kSha1HashSize];
	{
		Sha1Context context;
		Sha1Calculate(context, pw);
		Sha1Result(context, digest);
	}

	std::uint32_t key[kBlockSize] {
		2908958655u, 4146550480u, 658981742u, 1113311088u, 3927878744u, 679301322u, 1760465731u, 3305370375u,
		2269115995u, 3928541685u, 580724401u, 2607446661u, 2233092279u, 2416822349u, 4106933702u, 3046442503u
	};

	for (std::size_t i = 0; i < kBlockSize; ++i) {
		key[i] ^= digest[(i + 3) % kSha1HashSize];
	}

	Sha1Context context;
	Sha1Calculate(context, key);
	return context;
}

void ByteSwapBlock(std::uint32_t* data)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	(void)data;
#else
	for (std::size_t i = 0; i < kBlockSize; ++i) {
		data[i] = __builtin_bswap32(data[i]);
	}
#endif
}

void XorBlock(const std::uint32_t* shaResult, std::uint32_t* out)
{
	for (std::size_t i = 0; i < kBlockSize; ++i) {
		out[i] ^= shaResult[i % kSha1HashSize];
	}
}

CodecSignature GetCodecSignature(const std::byte* src)
{
	CodecSignature result;
	result.checksum = LoadLE32(src);
	src += 4;
	result.error = static_cast<std::uint8_t>(src[0]);
	result.lastChunkSize = static_cast<std::uint8_t>(src[1]);
	return result;
}

std::size_t CodecDecode(std::byte* srcDst, std::size_t size, const char* password)
{
	std::uint32_t block[kBlockSize];
	std::uint32_t digest[kSha1HashSize];

	Sha1Context context = CodecInitKey(password);
	if (size <= kSignatureSize) {
		return 0;
	}

	size -= kSignatureSize;
	if ((size % kBlockSizeBytes) != 0) {
		return 0;
	}

	for (std::size_t i = 0; i < size; srcDst += kBlockSizeBytes, i += kBlockSizeBytes) {
		std::memcpy(block, srcDst, kBlockSizeBytes);
		ByteSwapBlock(block);
		Sha1Result(context, digest);
		XorBlock(digest, block);
		Sha1Calculate(context, block);
		ByteSwapBlock(block);
		std::memcpy(srcDst, block, kBlockSizeBytes);
	}

	const CodecSignature sig = GetCodecSignature(srcDst);
	if (sig.error > 0) {
		return 0;
	}

	Sha1Result(context, digest);
	if (sig.checksum != digest[0]) {
		return 0;
	}

	size += sig.lastChunkSize - kBlockSizeBytes;
	return size;
}

std::string ClassToString(std::uint8_t classId)
{
	switch (classId) {
	case 0:
		return "Warrior";
	case 1:
		return "Rogue";
	case 2:
		return "Sorcerer";
	case 3:
		return "Monk";
	case 4:
		return "Bard";
	case 5:
		return "Barbarian";
	default:
		return "Unknown";
	}
}

std::string ReadPlayerName(const std::byte* data, std::size_t size)
{
	if (size < kNameOffset + kPlayerNameLength) {
		return {};
	}

	const char* nameData = reinterpret_cast<const char*>(data + kNameOffset);
	std::size_t length = 0;
	while (length < kPlayerNameLength && nameData[length] != '\0') {
		++length;
	}

	std::string result(nameData, length);
	for (char& c : result) {
		if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) {
			c = '?';
		}
	}
	return result;
}

} // namespace

bool ReadSingleSavegameSummary(const std::string& savePath, int slot, SaveGameSummary& outSummary)
{
	StormLib archive;
	if (!archive.OpenArchive(savePath)) {
		return false;
	}

	std::vector<std::byte> heroData;
	if (!archive.ReadFile("hero", heroData) || heroData.empty()) {
		return false;
	}

	std::size_t decodedSize = CodecDecode(heroData.data(), heroData.size(), "xrgyrkj1");
	if (decodedSize == 0 || decodedSize < (kLevelOffset + 1)) {
		return false;
	}

	outSummary.slot = slot;
	outSummary.name = ReadPlayerName(heroData.data(), decodedSize);
	if (outSummary.name.empty()) {
		outSummary.name = "Unknown";
	}
	const std::uint8_t classId = static_cast<std::uint8_t>(heroData[kClassOffset]);
	outSummary.className = ClassToString(classId);
	outSummary.strength = static_cast<int>(static_cast<std::uint8_t>(heroData[kBaseStrengthOffset]));
	outSummary.magic = static_cast<int>(static_cast<std::uint8_t>(heroData[kBaseMagicOffset]));
	outSummary.dexterity = static_cast<int>(static_cast<std::uint8_t>(heroData[kBaseDexterityOffset]));
	outSummary.vitality = static_cast<int>(static_cast<std::uint8_t>(heroData[kBaseVitalityOffset]));
	outSummary.level = static_cast<int>(static_cast<std::uint8_t>(heroData[kLevelOffset]));
	if (outSummary.level <= 0) {
		outSummary.level = 1;
	}
	return true;
}
