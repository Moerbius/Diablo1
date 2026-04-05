#include "game.hpp"

#include "graphics/cel.hpp"
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

	const MINFile min = MIN::LoadFromMPQ(mpq, minPath, MINParseOptions{ 16 });
	if (!min.IsValid()) {
		std::fprintf(stderr, "Town: could not load %s\n", minPath.c_str());
		return false;
	}

	const TILFile til = TIL::LoadFromMPQ(mpq, tilPath);
	if (!til.IsValid()) {
		std::fprintf(stderr, "Town: could not load %s\n", tilPath.c_str());
		return false;
	}

	const CELImage cel = CEL::LoadFromMPQ(mpq, celPath);
	if (cel.frames.empty()) {
		std::fprintf(stderr, "Town: could not load %s\n", celPath.c_str());
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

	std::vector<std::vector<std::uint32_t>> celRgba(cel.frames.size());
	for (std::size_t i = 0; i < cel.frames.size(); ++i) {
		celRgba[i] = CEL::ConvertFrameToRGBA32(cel.frames[i], palette, 0);
	}

	constexpr int kTileStepX = 64;
	constexpr int kTileStepY = 32;
	constexpr int kCanvasWidth = 3072;
	constexpr int kCanvasHeight = 1792;
	constexpr int kChunkTilesX = 20;
	constexpr int kChunkTilesY = 20;

	cache.width = kCanvasWidth;
	cache.height = kCanvasHeight;
	cache.pixels.assign(static_cast<std::size_t>(cache.width) * cache.height, 0xFF000000u);

	const bool hasDun = dun.IsValid();
	const int maxTilesX = hasDun ? std::min<int>(kChunkTilesX, dun.width) : kChunkTilesX;
	const int maxTilesY = hasDun ? std::min<int>(kChunkTilesY, dun.height) : kChunkTilesY;
	const int originX = (maxTilesY * kTileStepX) + 64;
	const int originY = 64;

	const std::array<std::pair<int, int>, 4> tileSubtileOffsets {
		std::pair<int, int>{ 32, 0 },
		std::pair<int, int>{ 64, 16 },
		std::pair<int, int>{ 0, 16 },
		std::pair<int, int>{ 32, 32 }
	};

	for (int ty = 0; ty < maxTilesY; ++ty) {
		for (int tx = 0; tx < maxTilesX; ++tx) {
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

			for (int subtile = 0; subtile < 4; ++subtile) {
				const std::uint16_t rawSubtile = tile.rawSubTileIndices[static_cast<std::size_t>(subtile)];
				if (static_cast<std::size_t>(rawSubtile) >= min.subTiles.size()) {
					continue;
				}

				const MINSubTile& sub = min.subTiles[static_cast<std::size_t>(rawSubtile)];
				const int subtileBaseX = tileBaseX + tileSubtileOffsets[static_cast<std::size_t>(subtile)].first;
				const int subtileBaseY = tileBaseY + tileSubtileOffsets[static_cast<std::size_t>(subtile)].second;

				for (std::size_t refIndex = 0; refIndex < sub.frameRefs.size(); ++refIndex) {
					const MINFrameReference& ref = sub.frameRefs[refIndex];
					if (ref.celFrameIndex < 0 || static_cast<std::size_t>(ref.celFrameIndex) >= cel.frames.size()) {
						continue;
					}

					const int localCol = static_cast<int>(refIndex % 2u);
					const int localRow = static_cast<int>(refIndex / 2u);
					const int frameX = subtileBaseX + localCol * 32;
					const int frameY = subtileBaseY + localRow * 32;
					const CELFrame& frame = cel.frames[static_cast<std::size_t>(ref.celFrameIndex)];
					const std::vector<std::uint32_t>& frameRgba = celRgba[static_cast<std::size_t>(ref.celFrameIndex)];
					BlitFrameToCanvas(
						frameRgba,
						static_cast<int>(frame.width),
						static_cast<int>(frame.height),
						cache.pixels,
						cache.width,
						cache.height,
						frameX,
						frameY);
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