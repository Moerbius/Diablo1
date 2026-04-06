#include "game.hpp"

#include "graphics/dun.hpp"
#include "graphics/min.hpp"
#include "graphics/pal.hpp"
#include "graphics/til.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

struct TownRenderCache {
	bool attemptedLoad = false;
	bool loaded = false;
	int width = 0;
	int height = 0;
	std::vector<std::uint32_t> pixels;
};

struct RawDungeonCelImage {
	std::vector<std::vector<std::byte>> frames;

	[[nodiscard]] bool IsValid() const
	{
		return !frames.empty();
	}
};

constexpr int kDungeonCelFrameWidth = 32;
constexpr int kDungeonCelFrameHeight = 32;

enum class DungeonCelFrameType : std::uint8_t {
	Square = 0,
	TransparentSquare = 1,
	LeftTriangle = 2,
	RightTriangle = 3,
	LeftTrapezoid = 4,
	RightTrapezoid = 5,
};

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

std::uint32_t PaletteIndexToRGBA(std::uint8_t index, const std::vector<std::uint8_t>& palette)
{
	if (index == 0) {
		return 0u;
	}

	const std::size_t paletteOffset = static_cast<std::size_t>(index) * 3u;
	if (paletteOffset + 2 >= palette.size()) {
		return 0u;
	}

	const std::uint32_t r = palette[paletteOffset + 0];
	const std::uint32_t g = palette[paletteOffset + 1];
	const std::uint32_t b = palette[paletteOffset + 2];
	return (r << 0) | (g << 8) | (b << 16) | (255u << 24);
}

void SetDungeonFramePixel(
	std::vector<std::uint32_t>& pixels,
	int x,
	int y,
	std::uint8_t paletteIndex,
	const std::vector<std::uint8_t>& palette)
{
	if (x < 0 || x >= kDungeonCelFrameWidth || y < 0 || y >= kDungeonCelFrameHeight) {
		return;
	}
	pixels[static_cast<std::size_t>(y) * kDungeonCelFrameWidth + x] = PaletteIndexToRGBA(paletteIndex, palette);
}

bool ParseRawDungeonCelData(const std::vector<std::byte>& data, RawDungeonCelImage& image)
{
	image.frames.clear();
	if (data.size() < 12) {
		return false;
	}

	const std::uint32_t frameCount = ReadLE32(data, 0);
	if (frameCount == 0 || frameCount > 50000) {
		return false;
	}

	const std::size_t headerBytes = static_cast<std::size_t>(frameCount + 2u) * 4u;
	if (headerBytes > data.size()) {
		return false;
	}

	std::vector<std::uint32_t> offsets;
	offsets.reserve(static_cast<std::size_t>(frameCount) + 1u);
	for (std::uint32_t i = 0; i <= frameCount; ++i) {
		offsets.push_back(ReadLE32(data, 4u + static_cast<std::size_t>(i) * 4u));
	}

	if (offsets.front() < headerBytes || offsets.back() > data.size()) {
		return false;
	}

	for (std::size_t i = 1; i < offsets.size(); ++i) {
		if (offsets[i] < offsets[i - 1]) {
			return false;
		}
	}

	image.frames.reserve(frameCount);
	for (std::uint32_t i = 0; i < frameCount; ++i) {
		const std::size_t frameBegin = offsets[i];
		const std::size_t frameEnd = offsets[i + 1];
		image.frames.emplace_back(data.begin() + static_cast<std::ptrdiff_t>(frameBegin),
			data.begin() + static_cast<std::ptrdiff_t>(frameEnd));
	}

	return image.IsValid();
}

void DecodeTriangleLower(
	std::vector<std::uint32_t>& pixels,
	const std::vector<std::byte>& frame,
	std::size_t& pos,
	int& srcRow,
	bool rightAligned,
	const std::vector<std::uint8_t>& palette)
{
	int width = 0;
	for (int i = 0; i < 8 && pos <= frame.size(); ++i) {
		pos += 2;
		width += 2;
		const int dstY0 = 31 - srcRow++;
		const int dstX0 = rightAligned ? (kDungeonCelFrameWidth - width) : 0;
		for (int x = 0; x < width && pos < frame.size(); ++x) {
			SetDungeonFramePixel(pixels, dstX0 + x, dstY0, static_cast<std::uint8_t>(frame[pos++]), palette);
		}

		width += 2;
		const int dstY1 = 31 - srcRow++;
		const int dstX1 = rightAligned ? (kDungeonCelFrameWidth - width) : 0;
		for (int x = 0; x < width && pos < frame.size(); ++x) {
			SetDungeonFramePixel(pixels, dstX1 + x, dstY1, static_cast<std::uint8_t>(frame[pos++]), palette);
		}
	}
}

std::vector<std::uint32_t> DecodeDungeonCelFrame(
	const std::vector<std::byte>& frame,
	DungeonCelFrameType type,
	const std::vector<std::uint8_t>& palette)
{
	std::vector<std::uint32_t> pixels(static_cast<std::size_t>(kDungeonCelFrameWidth) * kDungeonCelFrameHeight, 0u);
	std::size_t pos = 0;

	switch (type) {
	case DungeonCelFrameType::Square:
		for (int srcRow = 0; srcRow < kDungeonCelFrameHeight && pos < frame.size(); ++srcRow) {
			const int dstY = 31 - srcRow;
			for (int x = 0; x < kDungeonCelFrameWidth && pos < frame.size(); ++x) {
				SetDungeonFramePixel(pixels, x, dstY, static_cast<std::uint8_t>(frame[pos++]), palette);
			}
		}
		break;

	case DungeonCelFrameType::TransparentSquare:
		for (int srcRow = 0; srcRow < kDungeonCelFrameHeight && pos < frame.size(); ++srcRow) {
			const int dstY = 31 - srcRow;
			int x = 0;
			while (x < kDungeonCelFrameWidth && pos < frame.size()) {
				const int run = static_cast<std::int8_t>(static_cast<std::uint8_t>(frame[pos++]));
				if (run == 0) {
					break;
				}
				if (run > 0) {
					for (int i = 0; i < run && x < kDungeonCelFrameWidth && pos < frame.size(); ++i) {
						SetDungeonFramePixel(pixels, x++, dstY, static_cast<std::uint8_t>(frame[pos++]), palette);
					}
				} else {
					x = std::min(kDungeonCelFrameWidth, x - run);
				}
			}
		}
		break;

	case DungeonCelFrameType::LeftTriangle: {
		int srcRow = 0;
		DecodeTriangleLower(pixels, frame, pos, srcRow, true, palette);
		int width = kDungeonCelFrameWidth;
		for (int i = 0; i < 7 && pos <= frame.size(); ++i) {
			pos += 2;
			width -= 2;
			const int dstY0 = 31 - srcRow++;
			const int dstX0 = kDungeonCelFrameWidth - width;
			for (int x = 0; x < width && pos < frame.size(); ++x) {
				SetDungeonFramePixel(pixels, dstX0 + x, dstY0, static_cast<std::uint8_t>(frame[pos++]), palette);
			}

			width -= 2;
			const int dstY1 = 31 - srcRow++;
			const int dstX1 = kDungeonCelFrameWidth - width;
			for (int x = 0; x < width && pos < frame.size(); ++x) {
				SetDungeonFramePixel(pixels, dstX1 + x, dstY1, static_cast<std::uint8_t>(frame[pos++]), palette);
			}
		}
		pos += 2;
		width -= 2;
		const int dstY = 31 - srcRow;
		const int dstX = kDungeonCelFrameWidth - width;
		for (int x = 0; x < width && pos < frame.size(); ++x) {
			SetDungeonFramePixel(pixels, dstX + x, dstY, static_cast<std::uint8_t>(frame[pos++]), palette);
		}
		break;
	}

	case DungeonCelFrameType::RightTriangle: {
		int srcRow = 0;
		DecodeTriangleLower(pixels, frame, pos, srcRow, false, palette);
		int width = kDungeonCelFrameWidth;
		for (int i = 0; i < 7 && pos <= frame.size(); ++i) {
			width -= 2;
			const int dstY0 = 31 - srcRow++;
			for (int x = 0; x < width && pos < frame.size(); ++x) {
				SetDungeonFramePixel(pixels, x, dstY0, static_cast<std::uint8_t>(frame[pos++]), palette);
			}
			pos += 2;

			width -= 2;
			const int dstY1 = 31 - srcRow++;
			for (int x = 0; x < width && pos < frame.size(); ++x) {
				SetDungeonFramePixel(pixels, x, dstY1, static_cast<std::uint8_t>(frame[pos++]), palette);
			}
		}
		width -= 2;
		const int dstY = 31 - srcRow;
		for (int x = 0; x < width && pos < frame.size(); ++x) {
			SetDungeonFramePixel(pixels, x, dstY, static_cast<std::uint8_t>(frame[pos++]), palette);
		}
		break;
	}

	case DungeonCelFrameType::LeftTrapezoid: {
		int srcRow = 0;
		DecodeTriangleLower(pixels, frame, pos, srcRow, true, palette);
		for (int rectRow = 0; rectRow < 16 && pos < frame.size(); ++rectRow) {
			const int dstY = 15 - rectRow;
			for (int x = 0; x < kDungeonCelFrameWidth && pos < frame.size(); ++x) {
				SetDungeonFramePixel(pixels, x, dstY, static_cast<std::uint8_t>(frame[pos++]), palette);
			}
		}
		break;
	}

	case DungeonCelFrameType::RightTrapezoid: {
		int srcRow = 0;
		DecodeTriangleLower(pixels, frame, pos, srcRow, false, palette);
		for (int rectRow = 0; rectRow < 16 && pos < frame.size(); ++rectRow) {
			const int dstY = 15 - rectRow;
			for (int x = 0; x < kDungeonCelFrameWidth && pos < frame.size(); ++x) {
				SetDungeonFramePixel(pixels, x, dstY, static_cast<std::uint8_t>(frame[pos++]), palette);
			}
		}
		break;
	}
	}

	return pixels;
}

void BlitFrameToCanvas(
	const std::vector<std::uint32_t>& src,
	int srcWidth,
	int srcHeight,
	std::vector<std::uint32_t>& dst,
	int dstWidth,
	int dstHeight,
	int dstX,
	int dstY)
{
	for (int y = 0; y < srcHeight; ++y) {
		const int py = dstY + y;
		if (py < 0 || py >= dstHeight) {
			continue;
		}
		for (int x = 0; x < srcWidth; ++x) {
			const int px = dstX + x;
			if (px < 0 || px >= dstWidth) {
				continue;
			}
			const std::uint32_t pixel = src[static_cast<std::size_t>(y) * srcWidth + x];
			if ((pixel >> 24) == 0) {
				continue;
			}
			dst[static_cast<std::size_t>(py) * dstWidth + px] = pixel;
		}
	}
}

std::string FirstExistingPath(StormLib& mpq, const std::vector<std::string>& candidates)
{
	for (const std::string& path : candidates) {
		if (mpq.HasFile(path)) {
			return path;
		}
	}
	return {};
}

bool BuildTownImage(StormLib& mpq, TownRenderCache& cache)
{
	const std::string minPath = FirstExistingPath(mpq, {
		"levels\\towndata\\town.min",
		"levels\\towndata\\l1.min"
	});
	const std::string tilPath = FirstExistingPath(mpq, {
		"levels\\towndata\\town.til",
		"levels\\towndata\\l1.til"
	});
	const std::string celPath = FirstExistingPath(mpq, {
		"levels\\towndata\\town.cel",
		"levels\\towndata\\l1.cel"
	});
	const std::string palPath = FirstExistingPath(mpq, {
		"levels\\towndata\\town.pal",
		"levels\\towndata\\l1.pal",
		"levels\\l1data\\l1.pal"
	});

	if (minPath.empty() || tilPath.empty() || celPath.empty() || palPath.empty()) {
		std::fprintf(stderr, "Town: missing core assets (min/til/cel/pal)\n");
		return false;
	}

	const MINFile min = MIN::LoadFromMPQ(mpq, minPath);
	if (!min.IsValid()) {
		std::fprintf(stderr, "Town: could not load %s\n", minPath.c_str());
		return false;
	}

	const TILFile til = TIL::LoadFromMPQ(mpq, tilPath);
	if (!til.IsValid()) {
		std::fprintf(stderr, "Town: could not load %s\n", tilPath.c_str());
		return false;
	}

	std::vector<std::byte> celData;
	if (!mpq.ReadFile(celPath, celData)) {
		std::fprintf(stderr, "Town: could not load %s\n", celPath.c_str());
		return false;
	}

	RawDungeonCelImage cel;
	if (!ParseRawDungeonCelData(celData, cel)) {
		std::fprintf(stderr, "Town: could not parse %s\n", celPath.c_str());
		return false;
	}

	std::vector<std::uint8_t> palette = PAL::LoadFromMPQ(mpq, palPath);
	if (palette.empty()) {
		std::fprintf(stderr, "Town: could not load a palette for town rendering\n");
		return false;
	}

	DUNMap dun;
	std::string dunPath;
	if (mpq.HasFile("levels\\towndata\\town.dun")) {
		dunPath = "levels\\towndata\\town.dun";
	}
	if (dunPath.empty()) {
		for (int i = 1; i <= 16; ++i) {
			const std::string candidate = "levels\\towndata\\sector" + std::to_string(i) + "s.dun";
			if (mpq.HasFile(candidate)) {
				dunPath = candidate;
				break;
			}
		}
	}
	if (dunPath.empty()) {
		for (int i = 1; i <= 16; ++i) {
			const std::string candidate = "levels\\towndata\\sector" + std::to_string(i) + ".dun";
			if (mpq.HasFile(candidate)) {
				dunPath = candidate;
				break;
			}
		}
	}
	if (!dunPath.empty()) {
		dun = DUN::LoadFromMPQ(mpq, dunPath);
		if (!dun.IsValid()) {
			std::fprintf(stderr, "Town: failed to parse %s, using fallback tile layout\n", dunPath.c_str());
		}
	} else {
		std::fprintf(stderr, "Town: no DUN found, using fallback tile layout\n");
	}

	std::vector<std::array<std::vector<std::uint32_t>, 6>> celRgba(cel.frames.size());
	std::vector<std::array<bool, 6>> celDecoded(cel.frames.size());

	constexpr int kTileStepX = 64;
	constexpr int kTileStepY = 32;
	constexpr int kPieceBlockWidth = 32;
	constexpr int kCanvasWidth = 3072;
	constexpr int kCanvasHeight = 1792;
	constexpr int kChunkTilesX = 20;
	constexpr int kChunkTilesY = 20;

	cache.width = kCanvasWidth;
	cache.height = kCanvasHeight;
	cache.pixels.assign(static_cast<std::size_t>(cache.width) * cache.height, 0xFF000000u);

	const bool hasDun = dun.IsValid();
	const bool hasExtendedLayers = hasDun && dun.HasExtendedLayers();
	const int maxTilesX = hasDun ? std::min<int>(kChunkTilesX, dun.width) : kChunkTilesX;
	const int maxTilesY = hasDun ? std::min<int>(kChunkTilesY, dun.height) : kChunkTilesY;
	const int kBlockRows = static_cast<int>(min.refsPerSubTile) / 2;
	const int originX = (maxTilesY * kTileStepX) + 64;
	const int originY = 64 + (kBlockRows - 1) * kDungeonCelFrameHeight;

	// Draw all rows of a subtile column directly, floor (row 0) first, roof last.
	// Callers must invoke this in diagonal back-to-front order so painter's algorithm
	// works correctly without any global sort.
	auto renderSubtileFrames = [&](std::uint16_t subtileIndex, int subtileBaseX, int subtileBaseY) {
		if (static_cast<std::size_t>(subtileIndex) >= min.subTiles.size()) {
			return;
		}

		const MINSubTile& sub = min.subTiles[static_cast<std::size_t>(subtileIndex)];
		constexpr int kCols = 2;
		const int numRows = static_cast<int>(sub.frameRefs.size()) / kCols;
		for (int row = 0; row < numRows; ++row) {
			const int frameY = subtileBaseY - row * kDungeonCelFrameHeight;
			for (int col = 0; col < kCols; ++col) {
				const std::size_t refIndex = static_cast<std::size_t>(row * kCols + col);
				const MINFrameReference& ref = sub.frameRefs[refIndex];
				if (ref.celFrameIndex < 0 || static_cast<std::size_t>(ref.celFrameIndex) >= cel.frames.size()) {
					continue;
				}
				if (ref.celFrameType > static_cast<std::uint8_t>(DungeonCelFrameType::RightTrapezoid)) {
					continue;
				}
				const int frameX = subtileBaseX + col * kPieceBlockWidth;
				const std::size_t frameIndex = static_cast<std::size_t>(ref.celFrameIndex);
				const std::size_t typeIndex = static_cast<std::size_t>(ref.celFrameType);
				if (!celDecoded[frameIndex][typeIndex]) {
					celDecoded[frameIndex][typeIndex] = true;
					celRgba[frameIndex][typeIndex] = DecodeDungeonCelFrame(
						cel.frames[frameIndex],
						static_cast<DungeonCelFrameType>(ref.celFrameType),
						palette);
				}
				BlitFrameToCanvas(
					celRgba[frameIndex][typeIndex],
					kDungeonCelFrameWidth,
					kDungeonCelFrameHeight,
					cache.pixels,
					cache.width,
					cache.height,
					frameX,
					frameY);
			}
		}
	};

	const std::array<std::pair<int, int>, 4> tileSubtileOffsets {
		std::pair<int, int>{ 32, 0 },
		std::pair<int, int>{ 64, 16 },
		std::pair<int, int>{ 0, 16 },
		std::pair<int, int>{ 32, 32 }
	};

	const int maxDiag = maxTilesX + maxTilesY - 1;
	for (int diag = 0; diag < maxDiag; ++diag) {
		const int txStart = std::max(0, diag - (maxTilesY - 1));
		const int txEnd = std::min(maxTilesX - 1, diag);
		for (int tx = txStart; tx <= txEnd; ++tx) {
			const int ty = diag - tx;
			int tileIndex = -1;
			if (hasDun) {
				tileIndex = dun.BaseTileIndexAt(static_cast<std::size_t>(tx), static_cast<std::size_t>(ty));
			}
			if (tileIndex < 0 || static_cast<std::size_t>(tileIndex) >= til.tiles.size()) {
				const std::size_t fallback = (static_cast<std::size_t>(tx + ty * maxTilesX)) % til.tiles.size();
				tileIndex = static_cast<int>(fallback);
			}

			const int tileBaseX = originX + (tx - ty) * kTileStepX;
			const int tileBaseY = originY + (tx + ty) * kTileStepY;
			const TILTile& tile = til.tiles[static_cast<std::size_t>(tileIndex)];
			const std::size_t tileLinear = static_cast<std::size_t>(ty) * static_cast<std::size_t>(dun.width)
				+ static_cast<std::size_t>(tx);

			for (int subtile = 0; subtile < 4; ++subtile) {
				const std::uint16_t rawSubtile = tile.rawSubTileIndices[static_cast<std::size_t>(subtile)];
				const int subtileBaseX = tileBaseX + tileSubtileOffsets[static_cast<std::size_t>(subtile)].first;
				const int subtileBaseY = tileBaseY + tileSubtileOffsets[static_cast<std::size_t>(subtile)].second;
				renderSubtileFrames(rawSubtile, subtileBaseX, subtileBaseY);

				if (hasExtendedLayers) {
					const std::size_t subIndex = tileLinear * 4u + static_cast<std::size_t>(subtile);
					if (subIndex < dun.itemsLayer.size()) {
						const std::uint16_t itemEntry = dun.itemsLayer[subIndex];
						if (itemEntry != 0) {
							renderSubtileFrames(static_cast<std::uint16_t>(itemEntry - 1u), subtileBaseX, subtileBaseY);
						}
					}
					if (subIndex < dun.objectsLayer.size()) {
						const std::uint16_t objectEntry = dun.objectsLayer[subIndex];
						if (objectEntry != 0) {
							renderSubtileFrames(static_cast<std::uint16_t>(objectEntry - 1u), subtileBaseX, subtileBaseY);
						}
					}
				}
			}
		}
	}

	return true;
}

TownRenderCache& GetTownRenderCache()
{
	static TownRenderCache cache;
	return cache;
}

} // namespace

void Game::UpdatePlayingState(double dt)
{
	(void)dt;
}

bool Game::RenderPlayingState()
{
	SDL_Renderer* renderer = video_.GetRenderer();
	if (renderer == nullptr) {
		return false;
	}

	TownRenderCache& cache = GetTownRenderCache();
	if (!cache.attemptedLoad) {
		cache.attemptedLoad = true;
		cache.loaded = BuildTownImage(mpq_, cache);
	}

	if (!cache.loaded || cache.pixels.empty() || cache.width <= 0 || cache.height <= 0) {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		if (!SDL_RenderClear(renderer)) {
			return false;
		}
		SDL_RenderPresent(renderer);
		return true;
	}

	if (!video_.RenderPCXImageAt(cache.pixels.data(), cache.width, cache.height, 0, 0, windowWidth_, windowHeight_)) {
		return false;
	}

	SDL_RenderPresent(renderer);
	return true;
}