#include "font.hpp"

#include "pcx.hpp"
#include "storm/stormlib.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace {
constexpr int kGlyphRightPaddingPx = 2;
}

const char* Font::GetPresetImageName(Preset preset)
{
	switch (preset) {
	case Preset::Font16g: return "font16g.pcx";
	case Preset::Font16s: return "font16s.pcx";
	case Preset::Font24g: return "font24g.pcx";
	case Preset::Font24s: return "font24s.pcx";
	case Preset::Font30g: return "font30g.pcx";
	case Preset::Font30s: return "font30s.pcx";
	case Preset::Font42g: return "font42g.pcx";
	case Preset::Font42y: return "font42y.pcx";
	default: return nullptr;
	}
}

const char* Font::GetPresetMetricsName(Preset preset)
{
	switch (preset) {
	case Preset::Font16g:
	case Preset::Font16s:
		return "font16.bin";
	case Preset::Font24g:
	case Preset::Font24s:
		return "font24.bin";
	case Preset::Font30g:
	case Preset::Font30s:
		return "font30.bin";
	case Preset::Font42g:
	case Preset::Font42y:
		return "font42.bin";
	default:
		return nullptr;
	}
}

bool Font::LoadPresetFromMpq(StormLib& mpq, Preset preset, const std::string& fontRoot)
{
	const char* imageName = GetPresetImageName(preset);
	const char* metricsName = GetPresetMetricsName(preset);
	if (imageName == nullptr || metricsName == nullptr || fontRoot.empty()) {
		return false;
	}

	const std::string imagePath = fontRoot + "\\" + imageName;
	const std::string metricsPath = fontRoot + "\\" + metricsName;
	return LoadFromMpq(mpq, imagePath, metricsPath);
}

bool Font::LoadPresetWithFallback(StormLib& mpq, Preset preset)
{
	constexpr std::array<const char*, 3> kRoots = {"ui_art", "ui_art2", "ui_art3"};
	for (const char* root : kRoots) {
		if (LoadPresetFromMpq(mpq, preset, root)) {
			return true;
		}
	}

	return false;
}

Font::Font()
	: atlasWidth_(0)
	, atlasHeight_(0)
	, glyphColumns_(0)
	, glyphRows_(0)
	, glyphWidth_(0)
	, glyphHeight_(0)
	, texture_(nullptr)
	, textureRenderer_(nullptr)
	, loaded_(false)
{
}

Font::~Font()
{
	Clear();
}

bool Font::LoadFromMpq(StormLib& mpq, const std::string& imagePath, const std::string& metricsPath)
{
	Clear();

	const PCXImage atlas = PCX::LoadFromMPQ(mpq, imagePath);
	if (atlas.pixels.empty() || atlas.width == 0 || atlas.height == 0) {
		return false;
	}

	atlasWidth_ = static_cast<int>(atlas.width);
	atlasHeight_ = static_cast<int>(atlas.height);

	// Diablo font sheets can be stored as 16x16 grids or single-column strips.
	glyphColumns_ = 16;
	glyphRows_ = 16;
	if ((atlasHeight_ % kGlyphCount) == 0) {
		glyphColumns_ = 1;
		glyphRows_ = kGlyphCount;
	} else if ((atlasWidth_ % kGlyphCount) == 0) {
		glyphColumns_ = kGlyphCount;
		glyphRows_ = 1;
	} else if ((atlasWidth_ % 16) != 0 || (atlasHeight_ % 16) != 0) {
		Clear();
		return false;
	}

	glyphWidth_ = atlasWidth_ / glyphColumns_;
	glyphHeight_ = atlasHeight_ / glyphRows_;
	if (glyphWidth_ <= 0 || glyphHeight_ <= 0) {
		Clear();
		return false;
	}

	atlasRgba_ = PCX::ConvertToRGBA32(atlas);

	std::vector<std::byte> metricsData;
	if (!mpq.ReadFile(metricsPath, metricsData)) {
		Clear();
		return false;
	}

	glyphWidths_.assign(kGlyphCount, static_cast<std::uint8_t>(glyphWidth_));

	const std::size_t availableWidths = std::min<std::size_t>(static_cast<std::size_t>(kGlyphCount), metricsData.size());
	int widthSum = 0;
	int widthsAboveOne = 0;
	for (std::size_t i = 0; i < availableWidths; ++i) {
		const int width = static_cast<int>(std::to_integer<std::uint8_t>(metricsData[i]));
		const std::uint8_t clamped = static_cast<std::uint8_t>(std::clamp(width, 1, glyphWidth_));
		glyphWidths_[i] = clamped;
		widthSum += static_cast<int>(clamped);
		if (clamped > 1) {
			++widthsAboveOne;
		}
	}

	// Some BIN layouts are not raw width tables; if the parsed values are mostly 1px,
	// fall back to fixed-width advances instead of producing unreadable text.
	if (availableWidths > 0) {
		const double averageWidth = static_cast<double>(widthSum) / static_cast<double>(availableWidths);
		if (averageWidth < 2.0 || widthsAboveOne < static_cast<int>(availableWidths / 4)) {
			std::fill(glyphWidths_.begin(), glyphWidths_.end(), static_cast<std::uint8_t>(glyphWidth_));
		}
	}

	glyphLefts_.assign(kGlyphCount, 0);
	glyphDrawWidths_.assign(kGlyphCount, static_cast<std::uint8_t>(glyphWidth_));
	glyphAdvances_.assign(kGlyphCount, static_cast<std::uint8_t>(glyphWidth_));

	for (int glyphIndex = 0; glyphIndex < kGlyphCount; ++glyphIndex) {
		const int glyphColumn = glyphIndex % glyphColumns_;
		const int glyphRow = glyphIndex / glyphColumns_;
		const int baseX = glyphColumn * glyphWidth_;
		const int baseY = glyphRow * glyphHeight_;

		int minX = glyphWidth_;
		int maxX = -1;
		for (int y = 0; y < glyphHeight_; ++y) {
			for (int x = 0; x < glyphWidth_; ++x) {
				const int atlasX = baseX + x;
				const int atlasY = baseY + y;
				const std::size_t atlasIndex = static_cast<std::size_t>(atlasY * atlasWidth_ + atlasX);
				const std::uint32_t px = atlasRgba_[atlasIndex];
				const std::uint8_t alpha = static_cast<std::uint8_t>((px >> 24) & 0xFFu);
				if (alpha != 0) {
					minX = std::min(minX, x);
					maxX = std::max(maxX, x);
				}
			}
		}

		const int metricAdvance = static_cast<int>(glyphWidths_[glyphIndex]);
		if (maxX < minX) {
			const bool isSpaceGlyph = (glyphIndex == static_cast<int>(' '));
			const int defaultEmptyAdvance = isSpaceGlyph
				? std::max(1, glyphWidth_ / 3)
				: std::max(1, glyphWidth_ / 2);
			const int chosenEmptyAdvance = std::max(defaultEmptyAdvance, std::max(1, metricAdvance));
			glyphLefts_[glyphIndex] = 0;
			glyphDrawWidths_[glyphIndex] = 0;
			glyphAdvances_[glyphIndex] = static_cast<std::uint8_t>(std::clamp(chosenEmptyAdvance, 1, 250));
			continue;
		}

		const int drawWidth = (maxX - minX) + 1;
		const int visualAdvance = drawWidth + kGlyphRightPaddingPx;

		glyphLefts_[glyphIndex] = static_cast<std::uint8_t>(minX);
		glyphDrawWidths_[glyphIndex] = static_cast<std::uint8_t>(drawWidth);
		glyphAdvances_[glyphIndex] = static_cast<std::uint8_t>(std::clamp(visualAdvance, 1, 250));
	}

	loaded_ = true;
	return true;
}

bool Font::EnsureTexture(SDL_Renderer* renderer)
{
	if (!loaded_ || renderer == nullptr || atlasRgba_.empty()) {
		return false;
	}

	if (texture_ != nullptr && textureRenderer_ == renderer) {
		return true;
	}

	if (texture_ != nullptr) {
		SDL_DestroyTexture(texture_);
		texture_ = nullptr;
	}

	texture_ = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STATIC,
		atlasWidth_,
		atlasHeight_);
	if (texture_ == nullptr) {
		return false;
	}

	if (!SDL_UpdateTexture(texture_, nullptr, atlasRgba_.data(), atlasWidth_ * 4)) {
		SDL_DestroyTexture(texture_);
		texture_ = nullptr;
		return false;
	}

	if (!SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND)) {
		SDL_DestroyTexture(texture_);
		texture_ = nullptr;
		return false;
	}

	if (!SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST)) {
		SDL_DestroyTexture(texture_);
		texture_ = nullptr;
		return false;
	}

	textureRenderer_ = renderer;
	return true;
}

bool Font::RenderText(SDL_Renderer* renderer, const std::string& text, int x, int y, float scale)
{
	if (scale <= 0.0f) {
		return false;
	}

	if (!EnsureTexture(renderer)) {
		return false;
	}

	const int scaledGlyphHeight = std::max(1, static_cast<int>(glyphHeight_ * scale));

	int penX = x;
	int penY = y;

	for (std::size_t i = 0; i < text.size(); ) {
		const std::uint8_t byte = static_cast<std::uint8_t>(text[i]);

		// Decode UTF-8: combine multi-byte sequences into a codepoint (Latin-1 range only)
		unsigned int codepoint = byte;
		if ((byte & 0xE0u) == 0xC0u && i + 1 < text.size() && (static_cast<std::uint8_t>(text[i + 1]) & 0xC0u) == 0x80u) {
			codepoint = ((byte & 0x1Fu) << 6u) | (static_cast<std::uint8_t>(text[i + 1]) & 0x3Fu);
			i += 2;
		} else {
			++i;
		}

		if (codepoint == '\n') {
			penX = x;
			penY += scaledGlyphHeight;
			continue;
		}

		const std::uint8_t glyphIndex = (codepoint <= 255u) ? static_cast<std::uint8_t>(codepoint) : static_cast<std::uint8_t>('?');
		const int glyphColumn = glyphIndex % glyphColumns_;
		const int glyphRow = glyphIndex / glyphColumns_;
		const int left = static_cast<int>(glyphLefts_[glyphIndex]);
		const int drawWidth = static_cast<int>(glyphDrawWidths_[glyphIndex]);
		const int advance = std::max(1, static_cast<int>(glyphAdvances_[glyphIndex] * scale));

		if (drawWidth <= 0) {
			penX += advance;
			continue;
		}

		SDL_FRect src{};
		src.x = static_cast<float>((glyphColumn * glyphWidth_) + left);
		src.y = static_cast<float>(glyphRow * glyphHeight_);
		src.w = static_cast<float>(drawWidth);
		src.h = static_cast<float>(glyphHeight_);

		SDL_FRect dst{};
		dst.x = static_cast<float>(penX);
		dst.y = static_cast<float>(penY);
		dst.w = static_cast<float>(std::max(1, static_cast<int>(drawWidth * scale)));
		dst.h = static_cast<float>(scaledGlyphHeight);

		if (!SDL_RenderTexture(renderer, texture_, &src, &dst)) {
			return false;
		}

		penX += advance;
	}

	return true;
}

int Font::MeasureTextWidth(const std::string& text, float scale) const
{
	if (!loaded_ || scale <= 0.0f) {
		return 0;
	}

	int lineWidth = 0;
	int maxWidth = 0;

	for (std::size_t i = 0; i < text.size(); ) {
		const std::uint8_t byte = static_cast<std::uint8_t>(text[i]);

		unsigned int codepoint = byte;
		if ((byte & 0xE0u) == 0xC0u && i + 1 < text.size() && (static_cast<std::uint8_t>(text[i + 1]) & 0xC0u) == 0x80u) {
			codepoint = ((byte & 0x1Fu) << 6u) | (static_cast<std::uint8_t>(text[i + 1]) & 0x3Fu);
			i += 2;
		} else {
			++i;
		}

		if (codepoint == '\n') {
			maxWidth = std::max(maxWidth, lineWidth);
			lineWidth = 0;
			continue;
		}

		const std::uint8_t glyphIndex = (codepoint <= 255u) ? static_cast<std::uint8_t>(codepoint) : static_cast<std::uint8_t>('?');
		lineWidth += std::max(1, static_cast<int>(glyphAdvances_[glyphIndex] * scale));
	}

	return std::max(maxWidth, lineWidth);
}

int Font::GetLineHeight(float scale) const
{
	if (!loaded_ || scale <= 0.0f) {
		return 0;
	}

	return static_cast<int>(glyphHeight_ * scale);
}

void Font::Clear()
{
	if (texture_ != nullptr) {
		SDL_DestroyTexture(texture_);
		texture_ = nullptr;
	}

	textureRenderer_ = nullptr;
	atlasRgba_.clear();
	glyphWidths_.clear();
	glyphLefts_.clear();
	glyphDrawWidths_.clear();
	glyphAdvances_.clear();
	atlasWidth_ = 0;
	atlasHeight_ = 0;
	glyphColumns_ = 0;
	glyphRows_ = 0;
	glyphWidth_ = 0;
	glyphHeight_ = 0;
	loaded_ = false;
}

bool Font::IsLoaded() const
{
	return loaded_;
}
