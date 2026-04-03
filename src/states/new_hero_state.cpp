#include "game.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>

namespace {
constexpr bool kPreserveTitleAspectRatio = true;
}

void Game::UpdateNewHeroState(double dt)
{
	logoRenderer_.Update(dt);
	focusRenderer_.Update(dt);
}

bool Game::RenderNewHeroState()
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
		const int portraitIndex = std::clamp(newHeroClassSelectionIndex_, 0, 2);
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

	// Render "New Single Player Hero" text below logo
	if (heroCreationFontLoaded_) {
		const float textScale = static_cast<float>(uniformScale);
		const char* heroText = "New Single Player Hero";
		const int textWidth = heroCreationFont_.MeasureTextWidth(heroText, textScale);
		const int textX = (windowWidth_ - textWidth) / 2;
		const int textY = renderY + scaledLogoHeight + static_cast<int>(5 * uniformScale);

		if (!heroCreationFont_.RenderText(renderer, heroText, textX, textY, textScale)) {
			return false;
		}

		// Place class prompt inside the right panel of the 640x480 selhero layout.
		const char* chooseClassText = "Choose Class";
		const int chooseClassWidth = heroCreationFont_.MeasureTextWidth(chooseClassText, textScale);
		const int rightPanelCenterXNative = 425;
		const int rightPanelTitleYNative = 210;
		const int chooseClassX = renderX + static_cast<int>(rightPanelCenterXNative * uniformScale) - (chooseClassWidth / 2);
		const int chooseClassY = renderY + static_cast<int>(rightPanelTitleYNative * uniformScale);

		if (!heroCreationFont_.RenderText(renderer, chooseClassText, chooseClassX, chooseClassY, textScale)) {
			return false;
		}

		const int lineHeight = std::max(1, heroCreationFont_.GetLineHeight(textScale));
		const int classStartY = chooseClassY + (lineHeight * 2) + static_cast<int>(10 * uniformScale); // one blank line after heading + slight downward nudge
		const int classCenterX = renderX + static_cast<int>(rightPanelCenterXNative * uniformScale);
		const char* classNames[] = { "Warrior", "Rogue", "Sorcerer" };
		const int selectedIndex = std::clamp(newHeroClassSelectionIndex_, 0, 2);
		const int warriorWidth = heroClassFontLoaded_
			? heroClassFont_.MeasureTextWidth(classNames[0], textScale)
			: heroCreationFont_.MeasureTextWidth(classNames[0], textScale);
		const int selectedTextX = classCenterX - (warriorWidth / 2);
		int selectedTextY = 0;
		const int selectedTextWidth = warriorWidth;

		for (int i = 0; i < 3; ++i) {
			const int classTextWidth = heroClassFontLoaded_
				? heroClassFont_.MeasureTextWidth(classNames[i], textScale)
				: heroCreationFont_.MeasureTextWidth(classNames[i], textScale);
			const int classTextX = classCenterX - (classTextWidth / 2);
			const int classTextY = classStartY + (i * lineHeight);
			if (i == selectedIndex) {
				selectedTextY = classTextY;
			}
			const bool classRenderOk = heroClassFontLoaded_
				? heroClassFont_.RenderText(renderer, classNames[i], classTextX, classTextY, textScale)
				: heroCreationFont_.RenderText(renderer, classNames[i], classTextX, classTextY, textScale);
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
			const int selectedClass = std::clamp(newHeroClassSelectionIndex_, 0, 2);
			int level = 1;
			int strenght = 30;
			int magic = 10;
			int dexterity = 20;
			int vitality = 25;

			if (selectedClass == 1) {
				level = 1;
				strenght = 20;
				magic = 15;
				dexterity = 30;
				vitality = 20;
			} else if (selectedClass == 2) {
				level = 1;
				strenght = 15;
				magic = 35;
				dexterity = 15;
				vitality = 20;
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

			auto renderStatLine = [&](int y, const char* label, int value) -> bool {
				const int labelWidth = heroStatsFont_.MeasureTextWidth(label, textScale);
				const int labelX = colonX - labelWidth;
				if (!heroStatsFont_.RenderText(renderer, label, labelX, y, textScale)) {
					return false;
				}
				if (!heroStatsFont_.RenderText(renderer, ":", colonX, y, textScale)) {
					return false;
				}
				char valueBuffer[16];
				std::snprintf(valueBuffer, sizeof(valueBuffer), "%d", value);
				const int valueWidth = heroStatsFont_.MeasureTextWidth(valueBuffer, textScale);
				const int centeredValueX = valueFieldX + ((maxPairWidth - valueWidth) / 2);
				return heroStatsFont_.RenderText(renderer, valueBuffer, centeredValueX, y, textScale);
			};

			if (!renderStatLine(statsY, "Level", level)) {
				return false;
			}
			if (!renderStatLine(strengthY, "Strenght", strenght)) {
				return false;
			}
			if (!renderStatLine(magicY, "Magic", magic)) {
				return false;
			}
			if (!renderStatLine(dexterityY, "Dexterity", dexterity)) {
				return false;
			}
			if (!renderStatLine(vitalityY, "Vitality", vitality)) {
				return false;
			}
		}
	}

	SDL_RenderPresent(renderer);

	return true;
}
