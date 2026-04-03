#pragma once

#include "heroes/hero.hpp"

class Warrior final : public Hero {
public:
	Warrior();

	ClassType GetClassType() const override;
	const char* GetClassName() const override;
};
