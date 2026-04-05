#include "game.hpp"
#include "savegame_reader.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {
constexpr bool kPreserveTitleAspectRatio = true;

struct SaveEntry {
	std::string displayName;
	std::string className;
	int level = 1;
	int strength = 0;
	int magic = 0;
	int dexterity = 0;
	int vitality = 0;
	bool isNewHero = false;
};

int PortraitIndexFromClassName(const std::string& className)
{
	std::string lower = className;
	for (char& c : lower) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}

	if (lower == "warrior") {
		return 0;
	}
	if (lower == "rogue") {
		return 1;
	}
	if (lower == "sorcerer") {
		return 2;
	}
	return 0;
}

std::vector<SaveEntry> BuildSaveEntries()
{
	std::filesystem::path basePath = std::filesystem::current_path();
	if (const char* sdlBasePath = SDL_GetBasePath(); sdlBasePath != nullptr) {
		basePath = sdlBasePath;
	}

	std::vector<SaveEntry> entries;
	for (int i = 9; i >= 0; --i) {
		const std::filesystem::path savePath = basePath / ("single_" + std::to_string(i) + ".sv");
		if (std::filesystem::exists(savePath)) {
			SaveGameSummary summary;
			if (ReadSingleSavegameSummary(savePath.string(), i, summary)) {
				entries.push_back({
					summary.name,
					summary.className,
					summary.level,
					summary.strength,
					summary.magic,
					summary.dexterity,
					summary.vitality,
					false });
			} else {
				entries.push_back({ "Save" + std::to_string(i), "Unknown", 1, 0, 0, 0, 0, false });
			}
		}
	}
	entries.push_back({ "New Hero", "", 1, 0, 0, 0, 0, true });
	return entries;
}
}

void Game::UpdateShowSavegameState(double dt)
{
	logoRenderer_.Update(dt);
	focusRenderer_.Update(dt);
}

bool Game::RenderShowSavegameState()
{
	if (selheroImage_.empty() || selheroWidth_ <= 0 || selheroHeight_ <= 0) {
		return true;
	}

	SDL_Renderer* renderer = video_.GetRenderer();
	if (renderer == nullptr) {
		return false;
	}

	double scaleX = static_cast<double>(windowWidth_) / selheroWidth_;
	double scaleY = static_cast<double>(windowHeight_) / selheroHeight_;
	double uniformScale = std::min(scaleX, scaleY);
	const std::vector<SaveEntry> saveEntries = BuildSaveEntries();
	if (saveEntries.empty()) {
		return false;
	}
	const int selectedIndex = std::clamp(newHeroClassSelectionIndex_, 0, static_cast<int>(saveEntries.size()) - 1);
	const SaveEntry& selectedEntry = saveEntries[static_cast<std::size_t>(selectedIndex)];
	const bool isNewHeroSelected = selectedEntry.isNewHero;
	const int selectedPortraitIndex = isNewHeroSelected ? 3 : PortraitIndexFromClassName(selectedEntry.className);
	int renderX = 0;
	int renderY = 0;
	int renderWidth = windowWidth_;
	int renderHeight = windowHeight_;

	if (kPreserveTitleAspectRatio) {
		renderWidth = static_cast<int>(selheroWidth_ * uniformScale);
		renderHeight = static_cast<int>(selheroHeight_ * uniformScale);
		renderX = (windowWidth_ - renderWidth) / 2;
		renderY = (windowHeight_ - renderHeight) / 2;
	}

	if (!video_.RenderPCXImageAt(
		selheroImage_.data(), selheroWidth_, selheroHeight_,
		renderX, renderY,
		renderWidth, renderHeight)) {
		return false;
	}

	// Render selected class portrait from selected atlas into the left portrait panel.
	const PortraitAtlas portraitAtlas = PortraitAtlas::Heros;
	const std::vector<std::uint32_t>* portraitImage = nullptr;
	int portraitAtlasWidth = 0;
	int portraitAtlasHeight = 0;
	int portraitFrameCount = 0;
	bool portraitPreferVertical = true;
	if (GetPortraitAtlas(
			portraitAtlas,
			portraitImage,
			portraitAtlasWidth,
			portraitAtlasHeight,
			portraitFrameCount,
			portraitPreferVertical) &&
		portraitImage != nullptr && !portraitImage->empty() &&
		portraitAtlasWidth > 0 && portraitAtlasHeight > 0 && portraitFrameCount > 0) {
		const int portraitIndex = selectedPortraitIndex;
		const int portraitBoxX = renderX + static_cast<int>(30 * uniformScale);
		const int portraitBoxY = renderY + static_cast<int>(211 * uniformScale);
		const int portraitBoxWidth = static_cast<int>(180 * uniformScale);
		const int portraitBoxHeight = static_cast<int>(76 * uniformScale);
		if (!portraitRenderer_.RenderFrameFitted(
				video_,
				*portraitImage,
				portraitAtlasWidth,
				portraitAtlasHeight,
				portraitFrameCount,
				portraitIndex,
				portraitPreferVertical,
				portraitBoxX,
				portraitBoxY,
				portraitBoxWidth,
				portraitBoxHeight)) {
			return false;
		}
	}

	// Render scaled-down animated logo over the background
	const double logoScale = 0.7;
	int scaledLogoHeight = 0;
	if (!logoRenderer_.RenderTopCentered(
			video_,
			logoImage_,
			logoWidth_,
			logoHeight_,
			uniformScale,
			logoScale,
			renderX,
			renderY,
			renderWidth,
			&scaledLogoHeight)) {
		return false;
	}

	// Mirror New Hero visuals as baseline for Savegame screen tweaks.
	if (heroCreationFontLoaded_) {
		const float textScale = static_cast<float>(uniformScale);
		const char* heroText = "Single Player Characters";
		const int textWidth = heroCreationFont_.MeasureTextWidth(heroText, textScale);
		const int textX = (windowWidth_ - textWidth) / 2;
		const int textY = renderY + scaledLogoHeight + static_cast<int>(5 * uniformScale);

		if (!heroCreationFont_.RenderText(renderer, heroText, textX, textY, textScale)) {
			return false;
		}

		// Place class prompt inside the right panel of the 640x480 selhero layout.
		const char* chooseClassText = "Select Hero";
		const int chooseClassWidth = heroCreationFont_.MeasureTextWidth(chooseClassText, textScale);
		const int rightPanelCenterXNative = 425;
		const int rightPanelTitleYNative = 210;
		const int chooseClassX = renderX + static_cast<int>(rightPanelCenterXNative * uniformScale) - (chooseClassWidth / 2);
		const int chooseClassY = renderY + static_cast<int>(rightPanelTitleYNative * uniformScale);

		if (!heroCreationFont_.RenderText(renderer, chooseClassText, chooseClassX, chooseClassY, textScale)) {
			return false;
		}

		const int lineHeight = std::max(1, heroCreationFont_.GetLineHeight(textScale));
		const int classStartY = chooseClassY + lineHeight;
		const int classCenterX = renderX + static_cast<int>(rightPanelCenterXNative * uniformScale);
		const char* selectedLabel = saveEntries[static_cast<std::size_t>(selectedIndex)].displayName.c_str();
		const int selectedLabelWidth = heroClassFontLoaded_
			? heroClassFont_.MeasureTextWidth(selectedLabel, textScale)
			: heroCreationFont_.MeasureTextWidth(selectedLabel, textScale);
		const int selectedTextX = classCenterX - (selectedLabelWidth / 2);
		int selectedTextY = 0;
		const int selectedTextWidth = selectedLabelWidth;

		for (std::size_t i = 0; i < saveEntries.size(); ++i) {
			const char* entryText = saveEntries[i].displayName.c_str();
			const int classTextWidth = heroClassFontLoaded_
				? heroClassFont_.MeasureTextWidth(entryText, textScale)
				: heroCreationFont_.MeasureTextWidth(entryText, textScale);
			const int classTextX = classCenterX - (classTextWidth / 2);
			const int classTextY = classStartY + (static_cast<int>(i) * lineHeight);
			if (static_cast<int>(i) == selectedIndex) {
				selectedTextY = classTextY;
			}
			const bool classRenderOk = heroClassFontLoaded_
				? heroClassFont_.RenderText(renderer, entryText, classTextX, classTextY, textScale)
				: heroCreationFont_.RenderText(renderer, entryText, classTextX, classTextY, textScale);
			if (!classRenderOk) {
				return false;
			}
		}

		const FocusAtlas focusAtlas = FocusAtlas::Focus16;
		const std::vector<std::uint32_t>* focusImage = nullptr;
		int focusWidth = 0;
		int focusHeight = 0;
		int focusFrameCount = 0;
		if (GetFocusAtlas(focusAtlas, focusImage, focusWidth, focusHeight, focusFrameCount) &&
			focusImage != nullptr && !focusImage->empty() && focusWidth > 0 && focusHeight > 0 && focusFrameCount > 0) {
			const int markerPadding = std::max(1, heroCreationFont_.MeasureTextWidth("    ", textScale));
			if (!focusRenderer_.RenderPairAroundText(
					video_,
					*focusImage,
					focusWidth,
					focusHeight,
					focusFrameCount,
					uniformScale,
					selectedTextX,
					selectedTextY,
					selectedTextWidth,
					lineHeight,
					markerPadding)) {
				return false;
			}
		}

		if (heroStatsFontLoaded_) {
			int level = std::max(1, selectedEntry.level);
			int strenght = selectedEntry.strength;
			int magic = selectedEntry.magic;
			int dexterity = selectedEntry.dexterity;
			int vitality = selectedEntry.vitality;
			if (isNewHeroSelected) {
				level = 1;
				strenght = 0;
				magic = 0;
				dexterity = 0;
				vitality = 0;
			}

			const int statsX = renderX + static_cast<int>(60 * uniformScale);
			const int statsY = renderY + static_cast<int>(320 * uniformScale);
			const int statsLineHeight = std::max(1, heroStatsFont_.GetLineHeight(textScale));

			const std::array<const char*, 5> labels = {
				"Level",
				"Strenght",
				"Magic",
				"Dexterity",
				"Vitality"
			};
			int maxLabelWidth = 0;
			for (const char* label : labels) {
				maxLabelWidth = std::max(maxLabelWidth, heroStatsFont_.MeasureTextWidth(label, textScale));
			}

			const int spaceWidth = std::max(1, heroStatsFont_.MeasureTextWidth(" ", textScale));
			const int colonWidth = heroStatsFont_.MeasureTextWidth(":", textScale);
			const int colonX = statsX + maxLabelWidth + spaceWidth;
			const int valueX = colonX + colonWidth + (spaceWidth * 3);

			int maxPairWidth = 0;
			char pairBuffer[3] = { '0', '0', '\0' };
			for (int tens = 0; tens <= 9; ++tens) {
				for (int ones = 0; ones <= 9; ++ones) {
					pairBuffer[0] = static_cast<char>('0' + tens);
					pairBuffer[1] = static_cast<char>('0' + ones);
					maxPairWidth = std::max(maxPairWidth, heroStatsFont_.MeasureTextWidth(pairBuffer, textScale));
				}
			}
			const int valueFieldX = valueX;
			const int attributeLineStep = statsLineHeight + std::max(1, static_cast<int>(4 * uniformScale));
			const int strengthY = statsY + (statsLineHeight * 2);
			const int magicY = strengthY + attributeLineStep;
			const int dexterityY = magicY + attributeLineStep;
			const int vitalityY = dexterityY + attributeLineStep;

			auto renderStatLine = [&](int y, const char* label, const char* valueText) -> bool {
				const int labelWidth = heroStatsFont_.MeasureTextWidth(label, textScale);
				const int labelX = colonX - labelWidth;
				if (!heroStatsFont_.RenderText(renderer, label, labelX, y, textScale)) {
					return false;
				}
				if (!heroStatsFont_.RenderText(renderer, ":", colonX, y, textScale)) {
					return false;
				}
				const int valueWidth = heroStatsFont_.MeasureTextWidth(valueText, textScale);
				const int centeredValueX = valueFieldX + ((maxPairWidth - valueWidth) / 2);
				return heroStatsFont_.RenderText(renderer, valueText, centeredValueX, y, textScale);
			};

			char levelBuffer[16];
			char strenghtBuffer[16];
			char magicBuffer[16];
			char dexterityBuffer[16];
			char vitalityBuffer[16];
			std::snprintf(levelBuffer, sizeof(levelBuffer), "%d", level);
			std::snprintf(strenghtBuffer, sizeof(strenghtBuffer), "%d", strenght);
			std::snprintf(magicBuffer, sizeof(magicBuffer), "%d", magic);
			std::snprintf(dexterityBuffer, sizeof(dexterityBuffer), "%d", dexterity);
			std::snprintf(vitalityBuffer, sizeof(vitalityBuffer), "%d", vitality);

			const char* levelText = isNewHeroSelected ? "--" : levelBuffer;
			const char* strenghtText = isNewHeroSelected ? "--" : strenghtBuffer;
			const char* magicText = isNewHeroSelected ? "--" : magicBuffer;
			const char* dexterityText = isNewHeroSelected ? "--" : dexterityBuffer;
			const char* vitalityText = isNewHeroSelected ? "--" : vitalityBuffer;

			if (!renderStatLine(statsY, "Level", levelText)) {
				return false;
			}
			if (!renderStatLine(strengthY, "Strenght", strenghtText)) {
				return false;
			}
			if (!renderStatLine(magicY, "Magic", magicText)) {
				return false;
			}
			if (!renderStatLine(dexterityY, "Dexterity", dexterityText)) {
				return false;
			}
			if (!renderStatLine(vitalityY, "Vitality", vitalityText)) {
				return false;
			}
		}
	}

	SDL_RenderPresent(renderer);

	return true;
}
