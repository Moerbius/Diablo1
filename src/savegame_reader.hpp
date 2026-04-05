#pragma once

#include <string>

struct SaveGameSummary {
	int slot = -1;
	std::string name;
	std::string className;
	int level = 1;
	int strength = 0;
	int magic = 0;
	int dexterity = 0;
	int vitality = 0;
};

bool ReadSingleSavegameSummary(const std::string& savePath, int slot, SaveGameSummary& outSummary);
