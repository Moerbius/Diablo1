#include "graphics/cel.hpp"

#include "storm/stormlib.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

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

std::uint32_t ReadLE32(const std::vector<std::byte>& data, std::size_t offset)
{
	if (offset + 3 >= data.size()) {
		return 0;
	}

	const std::uint32_t b0 = static_cast<std::uint8_t>(data[offset + 0]);
	const std::uint32_t b1 = static_cast<std::uint8_t>(data[offset + 1]);
	const std::uint32_t b2 = static_cast<std::uint8_t>(data[offset + 2]);
	const std::uint32_t b3 = static_cast<std::uint8_t>(data[offset + 3]);
	return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

bool CountRegularEncodedPixels(const std::vector<std::byte>& data,
	std::size_t begin,
	std::size_t end,
	std::uint32_t& outPixelCount)
{
	outPixelCount = 0;
	std::size_t pos = begin;

	while (pos < end) {
		const std::uint8_t encoding = static_cast<std::uint8_t>(data[pos++]);
		const std::uint32_t pixelCount = (encoding <= 0x7F) ? encoding : (256u - encoding);
		outPixelCount += pixelCount;

		if (encoding <= 0x7F) {
			if (pos + pixelCount > end) {
				return false;
			}
			pos += pixelCount;
		}
	}

	return true;
}

bool ValidateRegularFrameLayout(const std::vector<std::byte>& data,
	std::size_t begin,
	std::size_t end,
	std::uint32_t width,
	std::uint32_t& outHeight)
{
	outHeight = 0;
	if (width == 0) {
		return false;
	}

	std::size_t pos = begin;
	std::uint32_t x = 0;
	std::uint32_t rows = 0;

	while (pos < end) {
		const std::uint8_t encoding = static_cast<std::uint8_t>(data[pos++]);
		const std::uint32_t runLength = (encoding <= 0x7F) ? encoding : (256u - encoding);
		const std::uint32_t remainingOnLine = width - x;

		if (runLength > remainingOnLine) {
			return false;
		}

		if (encoding <= 0x7F) {
			if (pos + runLength > end) {
				return false;
			}
			pos += runLength;
		}

		x += runLength;
		if (x == width) {
			x = 0;
			++rows;
		}
	}

	if (x != 0 || rows == 0) {
		return false;
	}

	outHeight = rows;
	return true;
}

bool DecodeRegularFrame(const std::vector<std::byte>& data,
	std::size_t begin,
	std::size_t end,
	std::uint32_t width,
	std::uint32_t height,
	CELFrame& outFrame)
{
	if (width == 0 || height == 0) {
		return false;
	}

	outFrame.width = width;
	outFrame.height = height;
	outFrame.pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);

	std::size_t pos = begin;
	std::uint32_t x = 0;
	std::int32_t y = static_cast<std::int32_t>(height) - 1;

	while (pos < end && y >= 0) {
		const std::uint8_t encoding = static_cast<std::uint8_t>(data[pos++]);
		const bool opaqueRun = encoding <= 0x7F;
		const std::uint32_t runLength = opaqueRun ? encoding : (256u - encoding);
		const std::uint32_t remainingOnLine = width - x;

		if (runLength > remainingOnLine) {
			return false;
		}

		if (opaqueRun) {
			if (pos + runLength > end) {
				return false;
			}

			for (std::uint32_t i = 0; i < runLength; ++i) {
				const std::size_t pixelIndex = static_cast<std::size_t>(y) * width + (x + i);
				outFrame.pixels[pixelIndex] = static_cast<std::uint8_t>(data[pos + i]);
			}
			pos += runLength;
		}

		x += runLength;
		if (x == width) {
			x = 0;
			--y;
		}
	}

	return y < 0;
}

bool DecodeLevelSpecialFrame(const std::vector<std::byte>& data,
	std::size_t begin,
	std::size_t end,
	CELFrame& outFrame)
{
	outFrame.width = 32;
	outFrame.height = 32;
	outFrame.pixels.assign(32u * 32u, 0);

	const std::size_t frameSize = end - begin;
	if (frameSize == 0x400) {
		for (std::size_t i = 0; i < 32u * 32u; ++i) {
			outFrame.pixels[i] = static_cast<std::uint8_t>(data[begin + i]);
		}
		return true;
	}

	if (frameSize != 0x220 && frameSize != 0x320) {
		return false;
	}

	std::size_t pos = begin;
	std::uint32_t x = 0;
	std::int32_t y = 31;

	while (pos < end && y >= 0) {
		if (pos + 1 < end && data[pos] == std::byte{0x00} && data[pos + 1] == std::byte{0x00}) {
			// In special level CEL frames, 0x00 0x00 means fill the remainder of the
			// current line with transparent pixels.
			x = 32;
			pos += 2;
		} else {
			const std::size_t pixelIndex = static_cast<std::size_t>(y) * 32u + x;
			outFrame.pixels[pixelIndex] = static_cast<std::uint8_t>(data[pos++]);
			++x;
		}

		if (x == 32) {
			x = 0;
			--y;
		}
	}

	return true;
}

std::uint32_t InferRegularFrameWidth(const std::vector<std::byte>& data,
	std::size_t begin,
	std::size_t end)
{
	static constexpr std::uint32_t kCommonWidths[] = {640, 512, 320, 256, 192, 160, 128, 96, 64, 48, 40, 32, 24, 16, 8};
	for (std::uint32_t candidate : kCommonWidths) {
		std::uint32_t height = 0;
		if (ValidateRegularFrameLayout(data, begin, end, candidate, height)) {
			return candidate;
		}
	}

	for (std::uint32_t candidate = 1024; candidate >= 1; --candidate) {
		std::uint32_t height = 0;
		if (ValidateRegularFrameLayout(data, begin, end, candidate, height)) {
			return candidate;
		}
	}

	return 0;
}

bool LooksLikeFrameHeader(const std::vector<std::byte>& data, std::size_t begin, std::size_t end)
{
	if (end - begin < 10) {
		return false;
	}

	if (ReadLE16(data, begin) != 0x000A) {
		return false;
	}

	std::uint16_t previous = 0;
	bool sawZeroOffset = false;
	for (int i = 0; i < 5; ++i) {
		const std::uint16_t offset = ReadLE16(data, begin + static_cast<std::size_t>(i) * 2);
		if (offset == 0) {
			sawZeroOffset = true;
			continue;
		}

		if (sawZeroOffset || offset < previous || offset >= (end - begin)) {
			return false;
		}
		previous = offset;
	}

	return true;
}

bool DecodeFrameWithHeader(const std::vector<std::byte>& data,
	std::size_t frameBegin,
	std::size_t frameEnd,
	CELFrame& outFrame)
{
	std::vector<std::uint16_t> chunkOffsets;
	chunkOffsets.reserve(6);
	for (int i = 0; i < 5; ++i) {
		const std::uint16_t offset = ReadLE16(data, frameBegin + static_cast<std::size_t>(i) * 2);
		if (offset != 0) {
			chunkOffsets.push_back(offset);
		}
	}
	if (chunkOffsets.empty()) {
		return false;
	}
	chunkOffsets.push_back(static_cast<std::uint16_t>(frameEnd - frameBegin));

	std::uint32_t frameWidth = 0;
	for (std::size_t chunk = 0; chunk + 1 < chunkOffsets.size(); ++chunk) {
		const std::size_t chunkBegin = frameBegin + chunkOffsets[chunk];
		const std::size_t chunkEnd = frameBegin + chunkOffsets[chunk + 1];
		if (chunkEnd < chunkBegin || chunkEnd > frameEnd) {
			return false;
		}

		std::uint32_t chunkPixels = 0;
		if (!CountRegularEncodedPixels(data, chunkBegin, chunkEnd, chunkPixels)) {
			return false;
		}
		if (chunkPixels % 32 != 0) {
			return false;
		}

		const std::uint32_t chunkWidth = chunkPixels / 32;
		if (frameWidth == 0) {
			frameWidth = chunkWidth;
		} else if (chunkWidth != 0 && chunkWidth != frameWidth) {
			// Some malformed assets can diverge slightly; keep the largest width.
			frameWidth = std::max(frameWidth, chunkWidth);
		}
	}

	if (frameWidth == 0) {
		return false;
	}

	outFrame.width = frameWidth;
	outFrame.height = static_cast<std::uint32_t>((chunkOffsets.size() - 1) * 32u);
	outFrame.pixels.assign(static_cast<std::size_t>(outFrame.width) * outFrame.height, 0);

	for (std::size_t chunk = 0; chunk + 1 < chunkOffsets.size(); ++chunk) {
		const std::size_t chunkBegin = frameBegin + chunkOffsets[chunk];
		const std::size_t chunkEnd = frameBegin + chunkOffsets[chunk + 1];

		CELFrame chunkFrame;
		if (!DecodeRegularFrame(data, chunkBegin, chunkEnd, frameWidth, 32, chunkFrame)) {
			return false;
		}

		const std::uint32_t chunkBaseY = outFrame.height - static_cast<std::uint32_t>(chunk + 1u) * 32u;
		for (std::uint32_t y = 0; y < 32; ++y) {
			const std::size_t dstRow = static_cast<std::size_t>(chunkBaseY + y) * frameWidth;
			const std::size_t srcRow = static_cast<std::size_t>(y) * frameWidth;
			std::copy_n(chunkFrame.pixels.begin() + static_cast<std::ptrdiff_t>(srcRow),
				frameWidth,
				outFrame.pixels.begin() + static_cast<std::ptrdiff_t>(dstRow));
		}
	}

	return true;
}

bool DecodeSingleFrame(const std::vector<std::byte>& data,
	std::size_t frameBegin,
	std::size_t frameEnd,
	const CELOptions& options,
	CELFrame& outFrame)
{
	if (frameEnd <= frameBegin) {
		return false;
	}

	const std::size_t frameSize = frameEnd - frameBegin;

	if (frameSize == 0x400 || frameSize == 0x220 || frameSize == 0x320) {
		if (DecodeLevelSpecialFrame(data, frameBegin, frameEnd, outFrame)) {
			return true;
		}
	}

	if (LooksLikeFrameHeader(data, frameBegin, frameEnd)) {
		if (DecodeFrameWithHeader(data, frameBegin, frameEnd, outFrame)) {
			return true;
		}
	}

	std::uint32_t pixelCount = 0;
	if (!CountRegularEncodedPixels(data, frameBegin, frameEnd, pixelCount)) {
		return false;
	}

	std::uint32_t width = options.fallbackWidth;
	if (width == 0) {
		width = InferRegularFrameWidth(data, frameBegin, frameEnd);
	}
	if (width == 0) {
		return false;
	}

	std::uint32_t height = options.fallbackHeight;
	if (height == 0) {
		if (!ValidateRegularFrameLayout(data, frameBegin, frameEnd, width, height)) {
			return false;
		}
	}

	if (height == 0 || static_cast<std::uint64_t>(width) * height != pixelCount) {
		return false;
	}

	return DecodeRegularFrame(data, frameBegin, frameEnd, width, height, outFrame);
}

bool ParseSingleCEL(const std::vector<std::byte>& data,
	std::size_t celBegin,
	std::size_t celEnd,
	const CELOptions& options,
	CELImage& outImage)
{
	if (celEnd <= celBegin || celEnd - celBegin < 8) {
		return false;
	}

	const std::uint32_t frameCount = ReadLE32(data, celBegin);
	if (frameCount == 0 || frameCount > 50000) {
		return false;
	}

	const std::size_t headerBytes = static_cast<std::size_t>(frameCount + 2u) * 4u;
	if (celBegin + headerBytes > celEnd) {
		return false;
	}

	std::vector<std::uint32_t> offsets;
	offsets.reserve(static_cast<std::size_t>(frameCount) + 1u);
	for (std::uint32_t i = 0; i <= frameCount; ++i) {
		offsets.push_back(ReadLE32(data, celBegin + 4u + static_cast<std::size_t>(i) * 4u));
	}

	const std::uint32_t subFileSize = static_cast<std::uint32_t>(celEnd - celBegin);
	if (offsets.front() < headerBytes || offsets.back() > subFileSize) {
		return false;
	}

	for (std::size_t i = 1; i < offsets.size(); ++i) {
		if (offsets[i] < offsets[i - 1]) {
			return false;
		}
	}

	for (std::uint32_t i = 0; i < frameCount; ++i) {
		const std::size_t frameBegin = celBegin + offsets[i];
		const std::size_t frameEnd = celBegin + offsets[i + 1];

		CELFrame frame;
		if (!DecodeSingleFrame(data, frameBegin, frameEnd, options, frame)) {
			std::fprintf(stderr, "CEL: Failed to decode frame %u\n", i);
			return false;
		}

		outImage.frames.push_back(std::move(frame));
	}

	return true;
}

bool ParseCompiledCEL(const std::vector<std::byte>& data, const CELOptions& options, CELImage& outImage)
{
	if (data.size() < 8) {
		return false;
	}

	const std::uint32_t firstOffset = ReadLE32(data, 0);
	if (firstOffset < 8 || firstOffset % 4 != 0 || firstOffset > data.size()) {
		return false;
	}

	const std::uint32_t celCount = firstOffset / 4;
	if (celCount == 0 || celCount > 50000) {
		return false;
	}

	std::vector<std::uint32_t> celOffsets;
	celOffsets.reserve(static_cast<std::size_t>(celCount) + 1u);
	for (std::uint32_t i = 0; i < celCount; ++i) {
		celOffsets.push_back(ReadLE32(data, static_cast<std::size_t>(i) * 4u));
	}
	celOffsets.push_back(static_cast<std::uint32_t>(data.size()));

	for (std::size_t i = 1; i < celOffsets.size(); ++i) {
		if (celOffsets[i] < celOffsets[i - 1] || celOffsets[i] > data.size()) {
			return false;
		}
	}

	for (std::uint32_t i = 0; i < celCount; ++i) {
		if (!ParseSingleCEL(data, celOffsets[i], celOffsets[i + 1], options, outImage)) {
			return false;
		}
	}

	return true;
}

}  // namespace

CELImage CEL::LoadFromMPQ(StormLib& mpq, const std::string& filename, const CELOptions& options)
{
	std::vector<std::byte> data;
	if (!mpq.ReadFile(filename, data)) {
		std::fprintf(stderr, "CEL: Failed to read file from MPQ: %s\n", filename.c_str());
		return CELImage{};
	}

	return ParseCELData(data, options);
}

CELImage CEL::ParseCELData(const std::vector<std::byte>& data, const CELOptions& options)
{
	CELImage image;
	if (ParseSingleCEL(data, 0, data.size(), options, image)) {
		return image;
	}

	image.frames.clear();
	if (ParseCompiledCEL(data, options, image)) {
		return image;
	}

	std::fprintf(stderr, "CEL: Failed to parse CEL data\n");
	return CELImage{};
}

std::vector<std::uint32_t> CEL::ConvertFrameToRGBA32(
	const CELFrame& frame,
	const std::vector<std::uint8_t>& palette,
	std::uint8_t transparentIndex)
{
	std::vector<std::uint32_t> rgba;
	const std::size_t pixelCount = static_cast<std::size_t>(frame.width) * frame.height;
	rgba.reserve(pixelCount);

	for (std::size_t i = 0; i < pixelCount; ++i) {
		const std::uint8_t index = i < frame.pixels.size() ? frame.pixels[i] : 0;
		if (index == transparentIndex) {
			rgba.push_back(0u);
			continue;
		}

		const std::size_t paletteOffset = static_cast<std::size_t>(index) * 3u;
		if (paletteOffset + 2 >= palette.size()) {
			rgba.push_back(0u);
			continue;
		}

		const std::uint32_t r = palette[paletteOffset + 0];
		const std::uint32_t g = palette[paletteOffset + 1];
		const std::uint32_t b = palette[paletteOffset + 2];
		const std::uint32_t pixel = (r << 0) | (g << 8) | (b << 16) | (255u << 24);
		rgba.push_back(pixel);
	}

	return rgba;
}
