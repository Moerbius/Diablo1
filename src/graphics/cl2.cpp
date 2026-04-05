#include "graphics/cl2.hpp"

#include "storm/stormlib.hpp"

#include <algorithm>
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

bool CountCL2Pixels(const std::vector<std::byte>& data,
	std::size_t begin,
	std::size_t end,
	std::uint32_t& outPixelCount)
{
	outPixelCount = 0;
	std::size_t pos = begin;

	while (pos < end) {
		const std::uint8_t encoding = static_cast<std::uint8_t>(data[pos++]);
		if (encoding == 0) {
			continue;
		}

		if (encoding <= 0x7F) {
			outPixelCount += encoding;
			continue;
		}

		if (encoding <= 0xBE) {
			if (pos >= end) {
				return false;
			}
			++pos;
			outPixelCount += static_cast<std::uint32_t>(0xBFu - encoding);
			continue;
		}

		const std::uint32_t runLength = static_cast<std::uint32_t>(256u - encoding);
		if (pos + runLength > end) {
			return false;
		}
		pos += runLength;
		outPixelCount += runLength;
	}

	return true;
}

[[maybe_unused]] bool DecodeCL2Chunk(const std::vector<std::byte>& data,
	std::size_t begin,
	std::size_t end,
	std::uint32_t width,
	std::uint32_t height,
	CL2Frame& outFrame)
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
		if (encoding == 0) {
			continue;
		}

		std::uint32_t runLength = 0;
		bool hasPixels = false;
		std::uint8_t repeatedColor = 0;

		if (encoding <= 0x7F) {
			runLength = encoding;
		} else if (encoding <= 0xBE) {
			runLength = static_cast<std::uint32_t>(0xBFu - encoding);
			if (pos >= end) {
				return false;
			}
			hasPixels = true;
			repeatedColor = static_cast<std::uint8_t>(data[pos++]);
		} else {
			runLength = static_cast<std::uint32_t>(256u - encoding);
			hasPixels = true;
			if (pos + runLength > end) {
				return false;
			}
		}

		if (runLength == 0 || x + runLength > width) {
			return false;
		}

		if (hasPixels) {
			if (encoding <= 0xBE) {
				for (std::uint32_t i = 0; i < runLength; ++i) {
					const std::size_t pixelIndex = static_cast<std::size_t>(y) * width + (x + i);
					outFrame.pixels[pixelIndex] = repeatedColor;
				}
			} else {
				for (std::uint32_t i = 0; i < runLength; ++i) {
					const std::size_t pixelIndex = static_cast<std::size_t>(y) * width + (x + i);
					outFrame.pixels[pixelIndex] = static_cast<std::uint8_t>(data[pos + i]);
				}
				pos += runLength;
			}
		}

		x += runLength;
		if (x == width) {
			x = 0;
			--y;
		}
	}

	return y < 0;
}

[[maybe_unused]] bool ValidateCL2Layout(const std::vector<std::byte>& data,
	std::size_t begin,
	std::size_t end,
	std::uint32_t width,
	std::uint32_t& outHeight)
{
	if (width == 0) {
		return false;
	}

	std::size_t pos = begin;
	std::uint32_t x = 0;
	std::uint32_t rows = 0;

	while (pos < end) {
		const std::uint8_t encoding = static_cast<std::uint8_t>(data[pos++]);
		if (encoding == 0) {
			continue;
		}

		std::uint32_t runLength = 0;
		if (encoding <= 0x7F) {
			runLength = encoding;
		} else if (encoding <= 0xBE) {
			runLength = static_cast<std::uint32_t>(0xBFu - encoding);
			if (pos >= end) {
				return false;
			}
			++pos;
		} else {
			runLength = static_cast<std::uint32_t>(256u - encoding);
			if (pos + runLength > end) {
				return false;
			}
			pos += runLength;
		}

		if (runLength == 0 || x + runLength > width) {
			return false;
		}

		x += runLength;
		if (x == width) {
			x = 0;
			++rows;
		}
	}

	if (rows == 0 || x != 0) {
		return false;
	}

	outHeight = rows;
	return true;
}

std::uint32_t InferCL2FrameWidth(const std::vector<std::byte>& data, std::size_t begin, std::size_t end)
{
	std::uint32_t totalPixels = 0;
	if (!CountCL2Pixels(data, begin, end, totalPixels) || totalPixels == 0) {
		return 0;
	}

	static constexpr std::uint32_t kCommonWidths[] = {640, 512, 320, 256, 192, 160, 128, 96, 64, 48, 40, 32, 24, 16, 8};
	for (std::uint32_t width : kCommonWidths) {
		if (totalPixels % width == 0) {
			return width;
		}
	}

	for (std::uint32_t width = 1024; width >= 1; --width) {
		if (totalPixels % width == 0) {
			return width;
		}
	}

	return 0;
}

bool DecodeCL2Linear(
	const std::vector<std::byte>& data,
	std::size_t begin,
	std::size_t end,
	std::uint32_t width,
	CL2Frame& outFrame)
{
	if (width == 0) {
		return false;
	}

	std::vector<std::vector<std::uint8_t>> rows;
	rows.reserve(256);
	std::vector<std::uint8_t> currentRow;
	currentRow.reserve(width);

	std::size_t pos = begin;
	while (pos < end) {
		const std::uint8_t encoding = static_cast<std::uint8_t>(data[pos++]);
		if (encoding == 0) {
			continue;
		}

		if (encoding <= 0x7F) {
			for (std::uint32_t i = 0; i < encoding; ++i) {
				currentRow.push_back(0);
				if (currentRow.size() == width) {
					rows.push_back(currentRow);
					currentRow.clear();
				}
			}
			continue;
		}

		if (encoding <= 0xBE) {
			if (pos >= end) {
				break;
			}
			const std::uint8_t color = static_cast<std::uint8_t>(data[pos++]);
			const std::uint32_t runLength = static_cast<std::uint32_t>(0xBFu - encoding);
			for (std::uint32_t i = 0; i < runLength; ++i) {
				currentRow.push_back(color);
				if (currentRow.size() == width) {
					rows.push_back(currentRow);
					currentRow.clear();
				}
			}
			continue;
		}

		const std::uint32_t runLength = static_cast<std::uint32_t>(256u - encoding);
		if (pos + runLength > end) {
			break;
		}
		for (std::uint32_t i = 0; i < runLength; ++i) {
			currentRow.push_back(static_cast<std::uint8_t>(data[pos + i]));
			if (currentRow.size() == width) {
				rows.push_back(currentRow);
				currentRow.clear();
			}
		}
		pos += runLength;
	}

	if (rows.empty()) {
		return false;
	}

	outFrame.width = width;
	outFrame.height = static_cast<std::uint32_t>(rows.size());
	outFrame.pixels.assign(static_cast<std::size_t>(outFrame.width) * outFrame.height, 0);

	for (std::size_t srcRow = 0; srcRow < rows.size(); ++srcRow) {
		const std::size_t dstY = rows.size() - 1u - srcRow;
		std::copy_n(rows[srcRow].begin(), outFrame.width,
			outFrame.pixels.begin() + static_cast<std::ptrdiff_t>(dstY * outFrame.width));
	}

	return true;
}

bool DecodeCL2FrameWithoutHeader(
	const std::vector<std::byte>& data,
	std::size_t frameDataBegin,
	std::size_t frameEnd,
	CL2Frame& outFrame)
{
	const std::uint32_t width = InferCL2FrameWidth(data, frameDataBegin, frameEnd);
	if (width == 0) {
		return false;
	}

	return DecodeCL2Linear(data, frameDataBegin, frameEnd, width, outFrame);
}

bool DecodeCL2Frame(const std::vector<std::byte>& data,
	std::size_t frameBegin,
	std::size_t frameEnd,
	CL2Frame& outFrame)
{
	if (frameEnd <= frameBegin) {
		return false;
	}

	const std::uint16_t headerSize = ReadLE16(data, frameBegin);
	if (headerSize < 2 || (headerSize % 2) != 0 || static_cast<std::size_t>(headerSize) >= (frameEnd - frameBegin)) {
		return DecodeCL2FrameWithoutHeader(data, frameBegin, frameEnd, outFrame);
	}
	const std::size_t frameDataBegin = frameBegin + headerSize;

	std::uint32_t frameWidth = 0;
	std::uint16_t lastChunkOffset = headerSize;
	std::uint16_t lastNonZeroOffset = headerSize;
	const std::size_t offsetCount = static_cast<std::size_t>(headerSize / 2);
	for (std::size_t i = 1; i < offsetCount; ++i) {
		const std::uint16_t nextChunkOffset = ReadLE16(data, frameBegin + i * 2u);
		if (nextChunkOffset == 0) {
			break;
		}
		if (nextChunkOffset <= lastChunkOffset || frameBegin + nextChunkOffset > frameEnd) {
			return DecodeCL2FrameWithoutHeader(data, frameDataBegin, frameEnd, outFrame);
		}

		std::uint32_t chunkPixels = 0;
		if (!CountCL2Pixels(data, frameBegin + lastChunkOffset, frameBegin + nextChunkOffset, chunkPixels)) {
			return DecodeCL2FrameWithoutHeader(data, frameDataBegin, frameEnd, outFrame);
		}
		if (chunkPixels == 0 || (chunkPixels % 32u) != 0) {
			return DecodeCL2FrameWithoutHeader(data, frameDataBegin, frameEnd, outFrame);
		}

		const std::uint32_t chunkWidth = chunkPixels / 32u;
		if (frameWidth != 0 && frameWidth != chunkWidth) {
			return DecodeCL2FrameWithoutHeader(data, frameDataBegin, frameEnd, outFrame);
		}
		frameWidth = chunkWidth;
		lastChunkOffset = nextChunkOffset;
		lastNonZeroOffset = nextChunkOffset;
	}

	if (frameWidth == 0) {
		return DecodeCL2FrameWithoutHeader(data, frameDataBegin, frameEnd, outFrame);
	}

	std::size_t frameDataEnd = frameEnd;
	if (lastNonZeroOffset > headerSize && frameBegin + static_cast<std::size_t>(lastNonZeroOffset) < frameEnd) {
		frameDataEnd = frameBegin + static_cast<std::size_t>(lastNonZeroOffset);
	}

	return DecodeCL2Linear(data, frameDataBegin, frameDataEnd, frameWidth, outFrame);
}

bool ParseFramesFromClipOffsets(
	const std::vector<std::byte>& data,
	std::size_t clipOffset,
	const std::vector<std::uint32_t>& frameOffsets,
	std::uint32_t endOffset,
	bool offsetsAreRelativeToClip,
	CL2Image& outImage)
{
	if (frameOffsets.empty()) {
		return false;
	}

	for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(frameOffsets.size()); ++i) {
		const std::uint32_t frameStartOffset = frameOffsets[static_cast<std::size_t>(i)];
		const std::uint32_t frameStopOffset =
			(i + 1u < static_cast<std::uint32_t>(frameOffsets.size()))
				? frameOffsets[static_cast<std::size_t>(i) + 1u]
				: endOffset;

		if (frameStopOffset <= frameStartOffset) {
			return false;
		}

		const std::size_t frameBegin = offsetsAreRelativeToClip
			? (clipOffset + static_cast<std::size_t>(frameStartOffset))
			: static_cast<std::size_t>(frameStartOffset);
		const std::size_t frameEnd = offsetsAreRelativeToClip
			? (clipOffset + static_cast<std::size_t>(frameStopOffset))
			: static_cast<std::size_t>(frameStopOffset);

		if (frameEnd <= frameBegin || frameEnd > data.size()) {
			return false;
		}

		CL2Frame frame;
		if (!DecodeCL2Frame(data, frameBegin, frameEnd, frame)) {
			return false;
		}

		outImage.frames.push_back(std::move(frame));
	}

	return true;
}

bool ParseCL2Clip(const std::vector<std::byte>& data, std::size_t clipOffset, CL2Image& outImage)
{
	if (clipOffset + 8 > data.size()) {
		return false;
	}

	const std::uint32_t frameCount = ReadLE32(data, clipOffset);
	if (frameCount == 0 || frameCount > 50000) {
		return false;
	}

	const std::size_t headerBytes = static_cast<std::size_t>(frameCount + 2u) * 4u;
	if (clipOffset + headerBytes > data.size()) {
		return false;
	}

	std::vector<std::uint32_t> frameOffsets;
	frameOffsets.reserve(static_cast<std::size_t>(frameCount));
	for (std::uint32_t i = 0; i < frameCount; ++i) {
		frameOffsets.push_back(ReadLE32(data, clipOffset + 4u + static_cast<std::size_t>(i) * 4u));
	}
	const std::uint32_t endOffset = ReadLE32(data, clipOffset + 4u + static_cast<std::size_t>(frameCount) * 4u);

	if (frameOffsets.empty()) {
		return false;
	}

	if (endOffset > data.size()) {
		return false;
	}

	for (std::size_t i = 1; i < frameOffsets.size(); ++i) {
		if (frameOffsets[i] < frameOffsets[i - 1]) {
			return false;
		}
	}

	if (endOffset < frameOffsets.back()) {
		return false;
	}

	const std::uint32_t minRelativeOffset = static_cast<std::uint32_t>(headerBytes);
	const bool canUseRelative = frameOffsets.front() >= minRelativeOffset;
	const bool canUseAbsolute = frameOffsets.front() >= static_cast<std::uint32_t>(clipOffset + headerBytes);

	CL2Image temp;
	if (canUseRelative && ParseFramesFromClipOffsets(data, clipOffset, frameOffsets, endOffset, true, temp)) {
		outImage.frames.insert(outImage.frames.end(), temp.frames.begin(), temp.frames.end());
		return true;
	}

	temp.frames.clear();
	if (canUseAbsolute && ParseFramesFromClipOffsets(data, clipOffset, frameOffsets, endOffset, false, temp)) {
		outImage.frames.insert(outImage.frames.end(), temp.frames.begin(), temp.frames.end());
		return true;
	}

	return false;
}

bool ParseSingleGroupCL2(const std::vector<std::byte>& data, CL2Image& outImage)
{
	const std::size_t firstIndex = outImage.frames.size();
	if (!ParseCL2Clip(data, 0, outImage)) {
		return false;
	}

	if (outImage.frames.size() <= firstIndex) {
		return false;
	}

	outImage.groupFrameRanges.emplace_back(firstIndex, outImage.frames.size() - 1u);
	return true;
}

bool ParseMultiGroupCL2(const std::vector<std::byte>& data, CL2Image& outImage)
{
	if (data.size() < 8) {
		return false;
	}

	const std::uint32_t firstGroupOffset = ReadLE32(data, 0);
	if (firstGroupOffset < 4 || firstGroupOffset > data.size() || (firstGroupOffset % 4u) != 0) {
		return false;
	}

	const std::uint32_t groupCount = firstGroupOffset / 4u;
	if (groupCount == 0 || groupCount > 5000) {
		return false;
	}

	std::vector<std::uint32_t> groupOffsets;
	groupOffsets.reserve(groupCount);
	for (std::uint32_t i = 0; i < groupCount; ++i) {
		groupOffsets.push_back(ReadLE32(data, static_cast<std::size_t>(i) * 4u));
	}

	for (std::size_t i = 0; i < groupOffsets.size(); ++i) {
		if (groupOffsets[i] >= data.size()) {
			return false;
		}
		if (i > 0 && groupOffsets[i] <= groupOffsets[i - 1]) {
			return false;
		}
	}

	for (std::uint32_t groupOffset : groupOffsets) {
		const std::size_t firstIndex = outImage.frames.size();
		if (!ParseCL2Clip(data, groupOffset, outImage)) {
			return false;
		}

		if (outImage.frames.size() <= firstIndex) {
			return false;
		}

		outImage.groupFrameRanges.emplace_back(firstIndex, outImage.frames.size() - 1u);
	}

	return !outImage.frames.empty();
}

} // namespace

CL2Image CL2::LoadFromMPQ(StormLib& mpq, const std::string& filename)
{
	std::vector<std::byte> data;
	if (!mpq.ReadFile(filename, data)) {
		std::fprintf(stderr, "CL2: Failed to read file from MPQ: %s\n", filename.c_str());
		return CL2Image{};
	}

	return ParseCL2Data(data);
}

CL2Image CL2::ParseCL2Data(const std::vector<std::byte>& data)
{
	CL2Image image;
	if (ParseSingleGroupCL2(data, image)) {
		return image;
	}

	image.frames.clear();
	if (ParseMultiGroupCL2(data, image)) {
		return image;
	}

	std::fprintf(stderr, "CL2: Failed to parse CL2 data\n");
	return CL2Image{};
}

std::vector<std::uint32_t> CL2::ConvertFrameToRGBA32(
	const CL2Frame& frame,
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
