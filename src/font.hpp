#pragma once

#include <cstdint>
#include <string>
#include <vector>

class StormLib;
struct SDL_Renderer;
struct SDL_Texture;

class Font {
public:
	enum class Preset {
		Font16g,
		Font16s,
		Font24g,
		Font24s,
		Font30g,
		Font30s,
		Font42g,
		Font42y
	};

	Font();
	~Font();

	Font(const Font&) = delete;
	Font& operator=(const Font&) = delete;

	bool LoadFromMpq(StormLib& mpq, const std::string& imagePath, const std::string& metricsPath);
	bool LoadPresetFromMpq(StormLib& mpq, Preset preset, const std::string& fontRoot = "ui_art");
	bool LoadPresetWithFallback(StormLib& mpq, Preset preset);
	bool RenderText(SDL_Renderer* renderer, const std::string& text, int x, int y, float scale = 1.0f);
	int MeasureTextWidth(const std::string& text, float scale = 1.0f) const;
	int GetLineHeight(float scale = 1.0f) const;
	void Clear();

	bool IsLoaded() const;

private:
	bool EnsureTexture(SDL_Renderer* renderer);
	static const char* GetPresetImageName(Preset preset);
	static const char* GetPresetMetricsName(Preset preset);

	static constexpr int kGlyphCount = 256;

	std::vector<std::uint32_t> atlasRgba_;
	std::vector<std::uint8_t> glyphWidths_;
	std::vector<std::uint8_t> glyphLefts_;
	std::vector<std::uint8_t> glyphDrawWidths_;
	std::vector<std::uint8_t> glyphAdvances_;
	int atlasWidth_;
	int atlasHeight_;
	int glyphColumns_;
	int glyphRows_;
	int glyphWidth_;
	int glyphHeight_;
	SDL_Texture* texture_;
	SDL_Renderer* textureRenderer_;
	bool loaded_;
};
